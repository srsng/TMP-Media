#pragma once

#include "MediaCoverImage.h"
#include "MediaItemInput.h"
#include "MediaSessionSelection.h"
#include "MediaText.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
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
    std::wstring source_app_name;
    media::SessionIdentity session_identity{ media::kNoSessionIdentity };
    std::wstring error_message;
    bool has_timeline{};
    std::int64_t timeline_start_ticks{};
    std::int64_t timeline_end_ticks{};
    std::int64_t position_ticks{};
    double progress_fraction{};
    bool can_switch_session{};
    std::size_t session_count{};
    bool can_play_pause{};
    bool can_skip_previous{};
    bool can_skip_next{};
    bool can_seek{};
    std::shared_ptr<const media::MediaCoverImage> cover;
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
    void SetCoverBackgroundEnabled(bool enabled);
    void SetMediaCardVisible(bool visible);
    void RequestSwitchSession(media::SessionSwitchDirection direction);
    void RequestSeekToPosition(media::SessionIdentity session_identity, std::int64_t position_ticks);
    void RequestImmediateAction(media::MediaControlAction action);
    void RequestSingleClick(
        media::MediaControlAction action,
        media::MediaItemHitRegion hit_region,
        unsigned int confirmation_delay_milliseconds);
    void RequestDoubleClick(
        media::MediaControlAction action,
        media::MediaItemHitRegion hit_region);

    [[nodiscard]] MediaTitleSnapshot GetSnapshot() const;
    [[nodiscard]] std::wstring GetDisplayText() const;
    [[nodiscard]] std::wstring GetTooltipText() const;

private:
    struct WorkerRequest
    {
        bool stop{};
        bool refresh{};
        std::optional<media::SessionSwitchDirection> switch_direction;
        std::optional<media::SeekRequest> seek;
        media::MediaControlAction action{ media::MediaControlAction::None };
        bool cover_background_enabled{};
        bool media_card_visible{};
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
    std::deque<media::SessionSwitchDirection> m_session_switch_requests;
    std::deque<media::SeekRequest> m_seek_requests;

    std::deque<media::MediaControlAction> m_control_actions;
    std::array<std::optional<PendingSingleClick>, 2> m_pending_single_clicks;
    std::array<std::chrono::steady_clock::time_point, 2> m_suppress_single_click_until{};
    bool m_started{};
    bool m_stop_requested{};
    bool m_refresh_requested{};
    bool m_cover_background_enabled{};
    bool m_media_card_visible{};
};
