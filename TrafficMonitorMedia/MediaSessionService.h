#pragma once

#include "MediaText.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

struct MediaTitleSnapshot
{
    media::MediaTitleState state{ media::MediaTitleState::Loading };
    media::MediaPlaybackState playback_state{ media::MediaPlaybackState::Unknown };
    std::wstring title;
    std::wstring artist;
    std::wstring source_app_id;
    std::wstring error_message;
    bool has_timeline{};
    double progress_fraction{};
};

// 所有 GSMTC 调用都在内部工作线程完成。TrafficMonitor 的 UI 线程只能读取快照或提交请求。
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
    void RequestImmediateAction(media::MediaControlAction action);
    void RequestSingleClick(media::MediaControlAction action);
    void RequestDoubleClick(media::MediaControlAction action);

    [[nodiscard]] MediaTitleSnapshot GetSnapshot() const;
    [[nodiscard]] std::wstring GetDisplayText() const;
    [[nodiscard]] std::wstring GetTooltipText() const;

private:
    struct WorkerRequest
    {
        bool stop{};
        bool refresh{};
        media::MediaControlAction action{ media::MediaControlAction::None };
    };

    struct PendingSingleClick
    {
        media::MediaControlAction action{ media::MediaControlAction::None };
        std::chrono::steady_clock::time_point deadline;
    };

    void WorkerLoop();
    WorkerRequest WaitForWork(std::chrono::steady_clock::time_point next_periodic_refresh);
    void Publish(MediaTitleSnapshot snapshot);

    mutable std::mutex m_snapshot_mutex;
    MediaTitleSnapshot m_snapshot;

    std::mutex m_worker_mutex;
    std::condition_variable m_worker_cv;
    std::thread m_worker;
    std::deque<media::MediaControlAction> m_control_actions;
    std::optional<PendingSingleClick> m_pending_single_click;
    std::chrono::steady_clock::time_point m_suppress_single_click_until{};
    bool m_started{};
    bool m_stop_requested{};
    bool m_refresh_requested{};
};
