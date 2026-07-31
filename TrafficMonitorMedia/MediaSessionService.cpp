#include "pch.h"
#include "MediaSessionService.h"
#include "MediaSessionSelection.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>

namespace
{
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus;

    constexpr auto kRefreshInterval = std::chrono::seconds(1);
    constexpr auto kCoverRetryInterval = std::chrono::seconds(10);

    struct CoverCache
    {
        std::wstring key;
        std::shared_ptr<const media::MediaCoverImage> image;
        std::chrono::steady_clock::time_point retry_after{};

        void Clear()
        {
            key.clear();
            image.reset();
            retry_after = {};
        }
    };

    std::wstring ToWString(const winrt::hstring& value)
    {
        return { value.c_str(), value.size() };
    }

    media::MediaPlaybackState ToPlaybackState(
        GlobalSystemMediaTransportControlsSessionPlaybackStatus status)
    {
        switch (status)
        {
        case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing:
            return media::MediaPlaybackState::Playing;
        case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused:
        case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped:
        case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Closed:
            return media::MediaPlaybackState::Paused;
        case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Opened:
        case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Changing:
        default:
            return media::MediaPlaybackState::Unknown;
        }
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

    std::vector<GlobalSystemMediaTransportControlsSession> GetSessions(
        const GlobalSystemMediaTransportControlsSessionManager& manager)
    {
        const auto session_view = manager.GetSessions();
        std::vector<GlobalSystemMediaTransportControlsSession> sessions;
        sessions.reserve(session_view.Size());
        for (const GlobalSystemMediaTransportControlsSession& session : session_view)
        {
            if (session)
            {
                sessions.push_back(session);
            }
        }
        return sessions;
    }

    media::SessionIdentity GetSessionIdentity(
        const GlobalSystemMediaTransportControlsSession& session)
    {
        if (!session)
        {
            return media::kNoSessionIdentity;
        }

        const auto unknown = session.as<winrt::Windows::Foundation::IUnknown>();
        return reinterpret_cast<media::SessionIdentity>(winrt::get_abi(unknown));
    }

    struct ResolvedSession
    {
        GlobalSystemMediaTransportControlsSession session{ nullptr };
        std::size_t count{};
        bool manual_selection{};
    };

    ResolvedSession ResolveSession(
        const GlobalSystemMediaTransportControlsSessionManager& manager,
        const GlobalSystemMediaTransportControlsSession& selected_session,
        bool manual_selection,
        std::optional<media::SessionSwitchDirection> switch_direction)
    {
        const std::vector<GlobalSystemMediaTransportControlsSession> sessions = GetSessions(manager);
        if (sessions.empty())
        {
            return {};
        }

        std::vector<media::SessionIdentity> identities;
        identities.reserve(sessions.size());
        for (const GlobalSystemMediaTransportControlsSession& session : sessions)
        {
            identities.push_back(GetSessionIdentity(session));
        }

        const GlobalSystemMediaTransportControlsSession system_current = manager.GetCurrentSession();
        const std::span<const media::SessionIdentity> identity_span(identities);
        const media::SessionIdentity system_identity = GetSessionIdentity(system_current);
        const media::SessionIdentity selected_identity = GetSessionIdentity(selected_session);

        media::SessionSelection selection;
        if (!switch_direction)
        {
            selection = media::ResolveSessionSelection(
                identity_span,
                system_identity,
                selected_identity,
                manual_selection);
        }
        else if (*switch_direction == media::SessionSwitchDirection::Previous)
        {
            selection = media::SelectPreviousSession(
                identity_span,
                system_identity,
                selected_identity,
                manual_selection);
        }
        else
        {
            selection = media::SelectNextSession(
                identity_span,
                system_identity,
                selected_identity,
                manual_selection);
        }

        if (!selection.HasSelection())
        {
            return { .count = sessions.size() };
        }

        return {
            .session = sessions[selection.selected_index],
            .count = sessions.size(),
            .manual_selection = selection.manual_selection,
        };
    }

    std::wstring MakeCoverKey(
        std::wstring_view source_app_id,
        std::wstring_view title,
        std::wstring_view artist,
        std::wstring_view album_title)
    {
        std::wstring key;
        key.reserve(source_app_id.size() + title.size() + artist.size() + album_title.size() + 4);
        key.append(source_app_id);
        key.push_back(L'\x1f');
        key.append(title);
        key.push_back(L'\x1f');
        key.append(artist);
        key.push_back(L'\x1f');
        key.append(album_title);
        return key;
    }

    MediaTitleSnapshot ReadSession(
        const GlobalSystemMediaTransportControlsSession& session,
        std::size_t session_count,
        bool need_cover,
        CoverCache& cover_cache)
    {
        if (!session)
        {
            cover_cache.Clear();
            return MakeNoSessionSnapshot();
        }

        MediaTitleSnapshot snapshot;
        snapshot.state = media::MediaTitleState::Ready;
        snapshot.can_switch_session = session_count > 1;
        snapshot.session_count = session_count;
        snapshot.source_app_id = ToWString(session.SourceAppUserModelId());
        snapshot.source_app_name = snapshot.source_app_id;
        snapshot.session_identity = GetSessionIdentity(session);

        const auto playback_info = session.GetPlaybackInfo();
        if (playback_info)
        {
            snapshot.playback_state = ToPlaybackState(playback_info.PlaybackStatus());
            try
            {
                const auto controls = playback_info.Controls();
                if (controls)
                {
                    snapshot.can_play_pause = controls.IsPlayPauseToggleEnabled()
                        || controls.IsPlayEnabled()
                        || controls.IsPauseEnabled();
                    snapshot.can_skip_previous = controls.IsPreviousEnabled();
                    snapshot.can_skip_next = controls.IsNextEnabled();
                    snapshot.can_seek = controls.IsPlaybackPositionEnabled();
                }
            }
            catch (...)
            {
                snapshot.can_play_pause = true;
                snapshot.can_skip_previous = true;
                snapshot.can_skip_next = true;
                snapshot.can_seek = false;
            }
        }

        const auto properties = session.TryGetMediaPropertiesAsync().get();
        if (properties)
        {
            snapshot.title = ToWString(properties.Title());
            snapshot.artist = ToWString(properties.Artist());

            if (need_cover)
            {
                std::wstring album_title;
                try
                {
                    album_title = ToWString(properties.AlbumTitle());
                }
                catch (...)
                {
                    // 专辑字段不是封面显示的必要条件，读取失败时继续使用其他媒体字段作为缓存键。
                }

                const std::wstring cover_key = MakeCoverKey(
                    snapshot.source_app_id,
                    snapshot.title,
                    snapshot.artist,
                    album_title);
                const auto now = std::chrono::steady_clock::now();
                const bool media_changed = cover_cache.key != cover_key;
                if (media_changed)
                {
                    cover_cache.key = cover_key;
                    cover_cache.image.reset();
                    cover_cache.retry_after = {};
                }

                if (!cover_cache.image && now >= cover_cache.retry_after)
                {
                    try
                    {
                        cover_cache.image = media::DecodeMediaCover(properties.Thumbnail());
                    }
                    catch (...)
                    {
                        cover_cache.image.reset();
                    }
                    cover_cache.retry_after = cover_cache.image
                        ? std::chrono::steady_clock::time_point{}
                        : now + kCoverRetryInterval;
                }
                snapshot.cover = cover_cache.image;
            }
            else
            {
                cover_cache.Clear();
            }
        }
        else
        {
            cover_cache.Clear();
        }

        const auto timeline = session.GetTimelineProperties();
        if (timeline)
        {
            const std::int64_t start = timeline.StartTime().count();
            const std::int64_t end = timeline.EndTime().count();
            const std::int64_t position = timeline.Position().count();
            snapshot.timeline_start_ticks = start;
            snapshot.timeline_end_ticks = end;
            snapshot.position_ticks = media::ClampTimelinePosition(position, start, end);
            snapshot.has_timeline = end > start;
            snapshot.progress_fraction = media::CalculateProgressFraction(position, start, end);
        }
        return snapshot;
    }

    void ExecuteControlAction(
        const GlobalSystemMediaTransportControlsSession& session,
        media::MediaControlAction action)
    {
        if (!session)
        {
            return;
        }

        switch (action)
        {
        case media::MediaControlAction::TogglePlayPause:
            static_cast<void>(session.TryTogglePlayPauseAsync().get());
            break;
        case media::MediaControlAction::SkipPrevious:
            static_cast<void>(session.TrySkipPreviousAsync().get());
            break;
        case media::MediaControlAction::SkipNext:
            static_cast<void>(session.TrySkipNextAsync().get());
            break;
        case media::MediaControlAction::OpenMediaCard:
        case media::MediaControlAction::None:
        default:
            break;
        }
    }

    bool ExecuteSeekRequest(
        const GlobalSystemMediaTransportControlsSession& session,
        std::int64_t requested_position_ticks)
    {
        if (!session)
        {
            return false;
        }

        const auto playback_info = session.GetPlaybackInfo();
        if (!playback_info)
        {
            return false;
        }

        const auto controls = playback_info.Controls();
        if (!controls || !controls.IsPlaybackPositionEnabled())
        {
            return false;
        }

        const auto timeline = session.GetTimelineProperties();
        if (!timeline)
        {
            return false;
        }

        const std::int64_t start = timeline.StartTime().count();
        const std::int64_t end = timeline.EndTime().count();
        if (end <= start)
        {
            return false;
        }

        const std::int64_t target = media::ClampTimelinePosition(requested_position_ticks, start, end);
        static_cast<void>(session.TryChangePlaybackPositionAsync(target).get());
        return true;
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
    m_session_switch_requests.clear();
    m_seek_requests.clear();
    m_control_actions.clear();
    for (auto& pending_single_click : m_pending_single_clicks)
    {
        pending_single_click.reset();
    }
    m_suppress_single_click_until = {};
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
        m_session_switch_requests.clear();
        m_seek_requests.clear();
        m_control_actions.clear();
        for (auto& pending_single_click : m_pending_single_clicks)
        {
            pending_single_click.reset();
        }
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

void CMediaSessionService::SetCoverBackgroundEnabled(bool enabled)
{
    {
        std::lock_guard<std::mutex> lock(m_worker_mutex);
        if (m_cover_background_enabled == enabled)
        {
            return;
        }
        m_cover_background_enabled = enabled;
        m_refresh_requested = true;
    }
    m_worker_cv.notify_one();
}

void CMediaSessionService::SetMediaCardVisible(bool visible)
{
    {
        std::lock_guard<std::mutex> lock(m_worker_mutex);
        if (m_media_card_visible == visible)
        {
            return;
        }
        m_media_card_visible = visible;
        m_refresh_requested = true;
    }
    m_worker_cv.notify_one();
}

void CMediaSessionService::RequestSwitchSession(media::SessionSwitchDirection direction)
{
    {
        std::lock_guard<std::mutex> lock(m_worker_mutex);
        if (!m_started)
        {
            return;
        }
        m_session_switch_requests.push_back(direction);
    }
    m_worker_cv.notify_one();
}

void CMediaSessionService::RequestSeekToPosition(
    media::SessionIdentity session_identity,
    std::int64_t position_ticks)
{
    if (session_identity == media::kNoSessionIdentity)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_worker_mutex);
        if (!m_started)
        {
            return;
        }
        m_seek_requests.push_back({ session_identity, position_ticks });
    }
    m_worker_cv.notify_one();
}

void CMediaSessionService::RequestImmediateAction(media::MediaControlAction action)
{
    if (action == media::MediaControlAction::None
        || action == media::MediaControlAction::OpenMediaCard)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_worker_mutex);
        if (!m_started)
        {
            return;
        }
        m_control_actions.push_back(action);
    }
    m_worker_cv.notify_one();
}

void CMediaSessionService::RequestSingleClick(
    media::MediaControlAction action,
    media::MediaItemHitRegion hit_region,
    unsigned int confirmation_delay_milliseconds)
{
    {
        std::lock_guard<std::mutex> lock(m_worker_mutex);
        if (!m_started)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const std::size_t hit_region_index = media::MediaItemHitRegionIndex(hit_region);
        if (!media::ShouldScheduleSingleClick(
                action,
                now < m_suppress_single_click_until[hit_region_index]))
        {
            return;
        }

        m_pending_single_clicks[hit_region_index] = PendingSingleClick{
            .action = action,
            .deadline = now + std::chrono::milliseconds(confirmation_delay_milliseconds),
        };
    }
    m_worker_cv.notify_one();
}

void CMediaSessionService::RequestDoubleClick(
    media::MediaControlAction action,
    media::MediaItemHitRegion hit_region)
{
    {
        std::lock_guard<std::mutex> lock(m_worker_mutex);
        if (!m_started)
        {
            return;
        }

        const auto double_click_interval = std::chrono::milliseconds(GetDoubleClickTime());
        const std::size_t hit_region_index = media::MediaItemHitRegionIndex(hit_region);
        m_pending_single_clicks[hit_region_index].reset();
        m_suppress_single_click_until[hit_region_index] =
            std::chrono::steady_clock::now() + double_click_interval;
        if (action != media::MediaControlAction::None
            && action != media::MediaControlAction::OpenMediaCard)
        {
            m_control_actions.push_back(action);
        }
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
        GlobalSystemMediaTransportControlsSession selected_session{ nullptr };
        bool manual_selection{};
        CoverCache cover_cache;
        auto next_periodic_refresh = std::chrono::steady_clock::now();

        while (true)
        {
            const WorkerRequest request = WaitForWork(next_periodic_refresh);
            if (request.stop)
            {
                return;
            }

            try
            {
                if (!manager)
                {
                    manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
                }

                const ResolvedSession resolved = ResolveSession(
                    manager,
                    selected_session,
                    manual_selection,
                    request.switch_direction);
                selected_session = resolved.session;
                manual_selection = resolved.manual_selection;

                if (request.seek.has_value()
                    && media::IsSeekRequestForSession(
                        *request.seek,
                        GetSessionIdentity(selected_session)))
                {
                    static_cast<void>(ExecuteSeekRequest(
                        selected_session,
                        request.seek->position_ticks));
                }

                if (request.action != media::MediaControlAction::None)
                {
                    ExecuteControlAction(selected_session, request.action);
                }

                const bool need_cover = request.cover_background_enabled || request.media_card_visible;
                if (request.refresh
                    || request.switch_direction.has_value()
                    || request.seek.has_value()
                    || request.action != media::MediaControlAction::None)
                {
                    Publish(ReadSession(
                        selected_session,
                        resolved.count,
                        need_cover,
                        cover_cache));
                    next_periodic_refresh = std::chrono::steady_clock::now() + kRefreshInterval;
                }
            }
            catch (const winrt::hresult_error& error)
            {
                manager = nullptr;
                selected_session = nullptr;
                manual_selection = false;
                cover_cache.Clear();
                Publish(MakeErrorSnapshot(ToWString(error.message())));
                next_periodic_refresh = std::chrono::steady_clock::now() + kRefreshInterval;
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
CMediaSessionService::WorkerRequest CMediaSessionService::WaitForWork(
    std::chrono::steady_clock::time_point next_periodic_refresh)
{
    std::unique_lock<std::mutex> lock(m_worker_mutex);

    while (true)
    {
        if (m_stop_requested)
        {
            return {
                .stop = true,
                .cover_background_enabled = m_cover_background_enabled,
                .media_card_visible = m_media_card_visible,
            };
        }

        if (!m_session_switch_requests.empty())
        {
            const media::SessionSwitchDirection direction = m_session_switch_requests.front();
            m_session_switch_requests.pop_front();
            m_refresh_requested = false;
            return {
                .switch_direction = direction,
                .cover_background_enabled = m_cover_background_enabled,
                .media_card_visible = m_media_card_visible,
            };
        }

        if (!m_seek_requests.empty())
        {
            const media::SeekRequest seek = m_seek_requests.front();
            m_seek_requests.pop_front();
            m_refresh_requested = false;
            return {
                .seek = seek,
                .cover_background_enabled = m_cover_background_enabled,
                .media_card_visible = m_media_card_visible,
            };
        }

        const auto now = std::chrono::steady_clock::now();
        const bool has_queued_action = !m_control_actions.empty();
        const media::MediaControlAction queued_action = has_queued_action
            ? m_control_actions.front()
            : media::MediaControlAction::None;
        std::optional<std::size_t> matured_single_click_index;
        for (std::size_t index = 0; index < m_pending_single_clicks.size(); ++index)
        {
            if (m_pending_single_clicks[index]
                && now >= m_pending_single_clicks[index]->deadline)
            {
                matured_single_click_index = index;
                break;
            }
        }
        const bool has_matured_single_click = matured_single_click_index.has_value();
        const media::MediaControlAction single_click_action = has_matured_single_click
            ? m_pending_single_clicks[*matured_single_click_index]->action
            : media::MediaControlAction::None;
        const media::MediaControlAction action = media::ResolveClickAction(
            has_queued_action,
            queued_action,
            has_matured_single_click,
            single_click_action);

        if (has_queued_action)
        {
            m_control_actions.pop_front();
            return {
                .action = action,
                .cover_background_enabled = m_cover_background_enabled,
                .media_card_visible = m_media_card_visible,
            };
        }
        if (has_matured_single_click)
        {
            m_pending_single_clicks[*matured_single_click_index].reset();
            return {
                .action = action,
                .cover_background_enabled = m_cover_background_enabled,
                .media_card_visible = m_media_card_visible,
            };
        }

        if (m_refresh_requested || now >= next_periodic_refresh)
        {
            m_refresh_requested = false;
            return {
                .refresh = true,
                .cover_background_enabled = m_cover_background_enabled,
                .media_card_visible = m_media_card_visible,
            };
        }

        auto wake_time = next_periodic_refresh;
        for (const auto& pending_single_click : m_pending_single_clicks)
        {
            if (pending_single_click)
            {
                wake_time = (std::min)(wake_time, pending_single_click->deadline);
            }
        }

        // 任意通知后重新计算等待截止时间，确保分区单击按各自确认时长到期。
        m_worker_cv.wait_until(lock, wake_time);
    }
}

void CMediaSessionService::Publish(MediaTitleSnapshot snapshot)
{
    std::lock_guard<std::mutex> lock(m_snapshot_mutex);
    m_snapshot = std::move(snapshot);
}
