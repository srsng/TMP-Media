#include "pch.h"
#include "SystemVolumeService.h"

#include <cmath>
#include <cstdint>

#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace
{
    using Microsoft::WRL::ComPtr;

    HRESULT OpenDefaultEndpointVolume(ComPtr<IAudioEndpointVolume>& endpoint_volume)
    {
        endpoint_volume.Reset();

        ComPtr<IMMDeviceEnumerator> enumerator;
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            IID_PPV_ARGS(&enumerator));
        if (FAILED(hr))
        {
            return hr;
        }

        ComPtr<IMMDevice> device;
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (FAILED(hr))
        {
            return hr;
        }

        return device->Activate(
            __uuidof(IAudioEndpointVolume),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void**>(endpoint_volume.GetAddressOf()));
    }

    SystemVolumeSnapshot ReadSnapshot(IAudioEndpointVolume& endpoint_volume, HRESULT last_error = S_OK)
    {
        SystemVolumeSnapshot snapshot;
        snapshot.last_error = last_error;

        float level{};
        HRESULT hr = endpoint_volume.GetMasterVolumeLevelScalar(&level);
        if (FAILED(hr))
        {
            snapshot.last_error = hr;
            return snapshot;
        }

        BOOL muted{};
        hr = endpoint_volume.GetMute(&muted);
        if (FAILED(hr))
        {
            snapshot.last_error = hr;
            return snapshot;
        }

        snapshot.available = true;
        snapshot.level = media::ClampSystemVolumeLevel(level);
        snapshot.muted = muted != FALSE;
        return snapshot;
    }

    SystemVolumeSnapshot MakeErrorSnapshot(HRESULT hr)
    {
        SystemVolumeSnapshot snapshot;
        snapshot.last_error = hr;
        return snapshot;
    }

    bool IsFinite(float value) noexcept
    {
        return std::isfinite(static_cast<double>(value));
    }
}

CSystemVolumeService::~CSystemVolumeService()
{
    Stop();
}

void CSystemVolumeService::Start()
{
    std::lock_guard lock(m_worker_mutex);
    if (m_started)
    {
        return;
    }

    m_stop_requested = false;
    m_requests.clear();
    m_requests.push_back({ .type = RequestType::Refresh });
    m_worker = std::thread(&CSystemVolumeService::WorkerLoop, this);
    m_started = true;
}

void CSystemVolumeService::Stop()
{
    {
        std::lock_guard lock(m_worker_mutex);
        if (!m_started)
        {
            return;
        }
        m_stop_requested = true;
        m_requests.clear();
    }
    m_worker_cv.notify_one();

    if (m_worker.joinable())
    {
        m_worker.join();
    }

    std::lock_guard lock(m_worker_mutex);
    m_started = false;
}

void CSystemVolumeService::RequestRefresh()
{
    QueueRequest({ .type = RequestType::Refresh });
}

void CSystemVolumeService::RequestSetLevel(float level)
{
    QueueRequest({ .type = RequestType::SetLevel, .level = level });
}

void CSystemVolumeService::RequestAdjustLevel(float delta)
{
    QueueRequest({ .type = RequestType::AdjustLevel, .level = delta });
}

void CSystemVolumeService::RequestSetMuted(bool muted)
{
    QueueRequest({ .type = RequestType::SetMuted, .muted = muted });
}

void CSystemVolumeService::RequestToggleMuted()
{
    QueueRequest({ .type = RequestType::ToggleMuted });
}

SystemVolumeSnapshot CSystemVolumeService::GetSnapshot() const
{
    std::lock_guard lock(m_snapshot_mutex);
    return m_snapshot;
}

void CSystemVolumeService::QueueRequest(WorkerRequest request)
{
    {
        std::lock_guard lock(m_worker_mutex);
        if (!m_started)
        {
            return;
        }
        m_requests.push_back(request);
    }
    m_worker_cv.notify_one();
}

void CSystemVolumeService::WorkerLoop()
{
    const HRESULT init_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(init_hr))
    {
        Publish(MakeErrorSnapshot(init_hr));
        return;
    }

    while (true)
    {
        WorkerRequest request;
        if (!WaitForRequest(request))
        {
            break;
        }

        ComPtr<IAudioEndpointVolume> endpoint_volume;
        HRESULT hr = OpenDefaultEndpointVolume(endpoint_volume);
        if (FAILED(hr))
        {
            Publish(MakeErrorSnapshot(hr));
            continue;
        }

        switch (request.type)
        {
        case RequestType::Refresh:
            Publish(ReadSnapshot(*endpoint_volume.Get()));
            break;
        case RequestType::SetLevel:
            if (!IsFinite(request.level))
            {
                Publish(MakeErrorSnapshot(E_INVALIDARG));
                break;
            }
            hr = endpoint_volume->SetMasterVolumeLevelScalar(
                media::ClampSystemVolumeLevel(request.level),
                nullptr);
            Publish(SUCCEEDED(hr) ? ReadSnapshot(*endpoint_volume.Get(), hr) : MakeErrorSnapshot(hr));
            break;
        case RequestType::AdjustLevel:
        {
            if (!IsFinite(request.level))
            {
                Publish(MakeErrorSnapshot(E_INVALIDARG));
                break;
            }
            float current_level{};
            hr = endpoint_volume->GetMasterVolumeLevelScalar(&current_level);
            if (FAILED(hr))
            {
                Publish(MakeErrorSnapshot(hr));
                break;
            }
            hr = endpoint_volume->SetMasterVolumeLevelScalar(
                media::ClampSystemVolumeLevel(current_level + request.level),
                nullptr);
            Publish(SUCCEEDED(hr) ? ReadSnapshot(*endpoint_volume.Get(), hr) : MakeErrorSnapshot(hr));
            break;
        }
        case RequestType::SetMuted:
            hr = endpoint_volume->SetMute(request.muted ? TRUE : FALSE, nullptr);
            Publish(SUCCEEDED(hr) ? ReadSnapshot(*endpoint_volume.Get(), hr) : MakeErrorSnapshot(hr));
            break;
        case RequestType::ToggleMuted:
        {
            BOOL current_mute{};
            hr = endpoint_volume->GetMute(&current_mute);
            if (FAILED(hr))
            {
                Publish(MakeErrorSnapshot(hr));
                break;
            }
            hr = endpoint_volume->SetMute(current_mute == FALSE ? TRUE : FALSE, nullptr);
            Publish(SUCCEEDED(hr) ? ReadSnapshot(*endpoint_volume.Get(), hr) : MakeErrorSnapshot(hr));
            break;
        }
        }
    }

    CoUninitialize();
}

bool CSystemVolumeService::WaitForRequest(WorkerRequest& request)
{
    std::unique_lock lock(m_worker_mutex);
    m_worker_cv.wait(lock, [this]
        {
            return m_stop_requested || !m_requests.empty();
        });

    if (m_stop_requested)
    {
        return false;
    }

    request = m_requests.front();
    m_requests.pop_front();
    return true;
}

void CSystemVolumeService::Publish(SystemVolumeSnapshot snapshot)
{
    std::lock_guard lock(m_snapshot_mutex);
    m_snapshot = snapshot;
}
