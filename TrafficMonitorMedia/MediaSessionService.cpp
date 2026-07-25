#include "pch.h"
#include "MediaSessionService.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

namespace
{
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;
    using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus;

    constexpr auto kRefreshInterval = std::chrono::seconds(1);

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

        const auto playback_info = session.GetPlaybackInfo();
        if (playback_info)
        {
            snapshot.playback_state = ToPlaybackState(playback_info.PlaybackStatus());
        }

        const auto properties = session.TryGetMediaPropertiesAsync().get();
        if (properties)
        {
            snapshot.title = ToWString(properties.Title());
            snapshot.artist = ToWString(properties.Artist());
        }

        const auto timeline = session.GetTimelineProperties();
        if (timeline)
        {
            const std::int64_t start = timeline.StartTime().count();
            const std::int64_t end = timeline.EndTime().count();
            const std::int64_t position = timeline.Position().count();
            snapshot.has_timeline = end > start;
            snapshot.progress_fraction = media::CalculateProgressFraction(position, start, end);
        }
        return snapshot;
    }

    void ExecuteControlAction(
        const GlobalSystemMediaTransportControlsSessionManager& manager,
        media::MediaControlAction action)
    {
        const GlobalSystemMediaTransportControlsSession session = manager.GetCurrentSession();
        if (!session)
        {
            return;
        }

        switch (action)
        {
        case media::MediaControlAction::TogglePlayPause:
            static_cast<void>(session.TryTogglePlayPauseAsync().get());
            break;
        case media::MediaControlAction::SkipNext:
            static_cast<void>(session.TrySkipNextAsync().get());
            break;
        case media::MediaControlAction::None:
        default:
            break;
        }
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
    m_control_actions.clear();
    m_pending_single_click.reset();
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
        m_control_actions.clear();
        m_pending_single_click.reset();
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

void CMediaSessionService::RequestImmediateAction(media::MediaControlAction action)
{
    if (action == media::MediaControlAction::None)
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

void CMediaSessionService::RequestSingleClick(media::MediaControlAction action)
{
    {
        std::lock_guard<std::mutex> lock(m_worker_mutex);
        if (!m_started)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (!media::ShouldScheduleSingleClick(action, now < m_suppress_single_click_until))
        {
            return;
        }

        m_pending_single_click = PendingSingleClick{
            .action = action,
            .deadline = now + std::chrono::milliseconds(GetDoubleClickTime()),
        };
    }
    m_worker_cv.notify_one();
}

void CMediaSessionService::RequestDoubleClick(media::MediaControlAction action)
{
    {
        std::lock_guard<std::mutex> lock(m_worker_mutex);
        if (!m_started)
        {
            return;
        }

        const auto double_click_interval = std::chrono::milliseconds(GetDoubleClickTime());
        m_pending_single_click.reset();
        m_suppress_single_click_until = std::chrono::steady_clock::now() + double_click_interval;
        if (action != media::MediaControlAction::None)
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

                if (request.action != media::MediaControlAction::None)
                {
                    ExecuteControlAction(manager, request.action);
                }
                if (request.refresh || request.action != media::MediaControlAction::None)
                {
                    Publish(ReadCurrentSession(manager));
                    next_periodic_refresh = std::chrono::steady_clock::now() + kRefreshInterval;
                }
            }
            catch (const winrt::hresult_error& error)
            {
                manager = nullptr;
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
            return { .stop = true };
        }

        const auto now = std::chrono::steady_clock::now();
        const bool has_queued_action = !m_control_actions.empty();
        const media::MediaControlAction queued_action = has_queued_action
            ? m_control_actions.front()
            : media::MediaControlAction::None;
        const bool has_matured_single_click = m_pending_single_click.has_value()
            && now >= m_pending_single_click->deadline;
        const media::MediaControlAction single_click_action = m_pending_single_click
            ? m_pending_single_click->action
            : media::MediaControlAction::None;
        const media::MediaControlAction action = media::ResolveClickAction(
            has_queued_action,
            queued_action,
            has_matured_single_click,
            single_click_action);

        if (has_queued_action)
        {
            m_control_actions.pop_front();
            return { .action = action };
        }
        if (has_matured_single_click)
        {
            m_pending_single_click.reset();
            return { .action = action };
        }

        if (m_refresh_requested || now >= next_periodic_refresh)
        {
            m_refresh_requested = false;
            return { .refresh = true };
        }

        auto wake_time = next_periodic_refresh;
        if (m_pending_single_click)
        {
            wake_time = (std::min)(wake_time, m_pending_single_click->deadline);
        }

        // 任意通知后重新计算等待截止时间，确保新单击使用系统双击间隔，而不是旧的刷新时间。
        m_worker_cv.wait_until(lock, wake_time);
    }
}

void CMediaSessionService::Publish(MediaTitleSnapshot snapshot)
{
    std::lock_guard<std::mutex> lock(m_snapshot_mutex);
    m_snapshot = std::move(snapshot);
}
