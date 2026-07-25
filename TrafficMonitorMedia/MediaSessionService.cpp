#include "pch.h"
#include "MediaSessionService.h"

#include <chrono>
#include <exception>
#include <utility>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

namespace
{
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;

    std::wstring ToWString(const winrt::hstring& value)
    {
        return { value.c_str(), value.size() };
    }

    MediaTitleSnapshot MakeNoSessionSnapshot()
    {
        MediaTitleSnapshot snapshot;
        snapshot.state = media::MediaTitleState::NoSession;
        return snapshot;
    }

    MediaTitleSnapshot MakeErrorSnapshot(const std::wstring& message)
    {
        MediaTitleSnapshot snapshot;
        snapshot.state = media::MediaTitleState::Error;
        snapshot.error_message = message;
        return snapshot;
    }

    MediaTitleSnapshot ReadCurrentSession(
        const GlobalSystemMediaTransportControlsSessionManager& manager)
    {
        const GlobalSystemMediaTransportControlsSession session = manager.GetCurrentSession();
        if (!session)
        {
            return MakeNoSessionSnapshot();
        }

        MediaTitleSnapshot snapshot;
        snapshot.state = media::MediaTitleState::Ready;
        snapshot.source_app_id = ToWString(session.SourceAppUserModelId());

        const auto properties = session.TryGetMediaPropertiesAsync().get();
        if (properties)
        {
            snapshot.title = ToWString(properties.Title());
            snapshot.artist = ToWString(properties.Artist());
        }
        return snapshot;
    }
}

CMediaSessionService::~CMediaSessionService()
{
    Stop();
}

void CMediaSessionService::Start()
{
    std::lock_guard<std::mutex> lock(m_worker_mutex);
    if (m_started)
    {
        return;
    }

    m_stop_requested = false;
    m_refresh_requested = true;
    m_worker = std::thread(&CMediaSessionService::WorkerLoop, this);
    m_started = true;
}

void CMediaSessionService::Stop()
{
    {
        std::lock_guard<std::mutex> lock(m_worker_mutex);
        if (!m_started)
        {
            return;
        }
        m_stop_requested = true;
        m_refresh_requested = true;
    }
    m_worker_cv.notify_one();

    if (m_worker.joinable())
    {
        m_worker.join();
    }

    std::lock_guard<std::mutex> lock(m_worker_mutex);
    m_started = false;
}

void CMediaSessionService::RequestRefresh()
{
    {
        std::lock_guard<std::mutex> lock(m_worker_mutex);
        if (!m_started)
        {
            return;
        }
        m_refresh_requested = true;
    }
    m_worker_cv.notify_one();
}

MediaTitleSnapshot CMediaSessionService::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_snapshot_mutex);
    return m_snapshot;
}

std::wstring CMediaSessionService::GetDisplayText() const
{
    const MediaTitleSnapshot snapshot = GetSnapshot();
    return std::wstring(media::SelectDisplayText(snapshot.state, snapshot.title, snapshot.source_app_id));
}

std::wstring CMediaSessionService::GetTooltipText() const
{
    const MediaTitleSnapshot snapshot = GetSnapshot();
    if (snapshot.state == media::MediaTitleState::Error)
    {
        std::wstring tooltip = std::wstring(media::kUnavailableMediaText);
        if (!snapshot.error_message.empty())
        {
            tooltip += L"\n";
            tooltip += snapshot.error_message;
        }
        return tooltip;
    }

    std::wstring tooltip(
        media::SelectDisplayText(snapshot.state, snapshot.title, snapshot.source_app_id));
    if (!snapshot.artist.empty())
    {
        tooltip += L"\n";
        tooltip += snapshot.artist;
    }
    return tooltip;
}

void CMediaSessionService::WorkerLoop()
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        GlobalSystemMediaTransportControlsSessionManager manager{ nullptr };

        while (WaitForRefreshRequest())
        {
            try
            {
                if (!manager)
                {
                    manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
                }
                Publish(ReadCurrentSession(manager));
            }
            catch (const winrt::hresult_error& error)
            {
                manager = nullptr;
                Publish(MakeErrorSnapshot(ToWString(error.message())));
            }
        }
    }
    catch (const winrt::hresult_error& error)
    {
        Publish(MakeErrorSnapshot(ToWString(error.message())));
    }
    catch (const std::exception&)
    {
        Publish(MakeErrorSnapshot(L"媒体服务发生意外错误"));
    }
}

bool CMediaSessionService::WaitForRefreshRequest()
{
    std::unique_lock<std::mutex> lock(m_worker_mutex);
    m_worker_cv.wait_for(lock, std::chrono::seconds(1), [this]
    {
        return m_stop_requested || m_refresh_requested;
    });

    if (m_stop_requested)
    {
        return false;
    }

    m_refresh_requested = false;
    return true;
}

void CMediaSessionService::Publish(MediaTitleSnapshot snapshot)
{
    std::lock_guard<std::mutex> lock(m_snapshot_mutex);
    m_snapshot = std::move(snapshot);
}
