#pragma once

#include "MediaText.h"

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

struct MediaTitleSnapshot
{
    media::MediaTitleState state{ media::MediaTitleState::Loading };
    std::wstring title;
    std::wstring artist;
    std::wstring source_app_id;
    std::wstring error_message;
};

// 所有 GSMTC 调用都在内部工作线程完成。TrafficMonitor 的 UI 线程只能读取快照或请求刷新。
class CMediaSessionService
{
public:
    CMediaSessionService() = default;
    ~CMediaSessionService();

    CMediaSessionService(const CMediaSessionService&) = delete;
    CMediaSessionService& operator=(const CMediaSessionService&) = delete;

    void Start();
    void Stop();
    void RequestRefresh();

    [[nodiscard]] MediaTitleSnapshot GetSnapshot() const;
    [[nodiscard]] std::wstring GetDisplayText() const;
    [[nodiscard]] std::wstring GetTooltipText() const;

private:
    void WorkerLoop();
    bool WaitForRefreshRequest();
    void Publish(MediaTitleSnapshot snapshot);

    mutable std::mutex m_snapshot_mutex;
    MediaTitleSnapshot m_snapshot;

    std::mutex m_worker_mutex;
    std::condition_variable m_worker_cv;
    std::thread m_worker;
    bool m_started{};
    bool m_stop_requested{};
    bool m_refresh_requested{};
};
