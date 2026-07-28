#pragma once

#include <Windows.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace media
{
    [[nodiscard]] constexpr float ClampSystemVolumeLevel(float level) noexcept
    {
        if (level < 0.0F)
        {
            return 0.0F;
        }
        if (level > 1.0F)
        {
            return 1.0F;
        }
        return level;
    }
}

struct SystemVolumeSnapshot
{
    bool available{};
    float level{};
    bool muted{};
    HRESULT last_error{ S_OK };
};

class CSystemVolumeService
{
public:
    CSystemVolumeService() = default;
    ~CSystemVolumeService();

    CSystemVolumeService(const CSystemVolumeService&) = delete;
    CSystemVolumeService& operator=(const CSystemVolumeService&) = delete;

    void Start();
    void Stop();

    void RequestRefresh();
    void RequestSetLevel(float level);
    void RequestAdjustLevel(float delta);
    void RequestSetMuted(bool muted);
    void RequestToggleMuted();

    [[nodiscard]] SystemVolumeSnapshot GetSnapshot() const;

private:
    enum class RequestType
    {
        Refresh,
        SetLevel,
        AdjustLevel,
        SetMuted,
        ToggleMuted,
    };

    struct WorkerRequest
    {
        RequestType type{ RequestType::Refresh };
        float level{};
        bool muted{};
    };

    void QueueRequest(WorkerRequest request);
    void WorkerLoop();
    [[nodiscard]] bool WaitForRequest(WorkerRequest& request);
    void Publish(SystemVolumeSnapshot snapshot);

    mutable std::mutex m_snapshot_mutex;
    SystemVolumeSnapshot m_snapshot;

    std::mutex m_worker_mutex;
    std::condition_variable m_worker_cv;
    std::thread m_worker;
    std::deque<WorkerRequest> m_requests;
    bool m_started{};
    bool m_stop_requested{};
};
