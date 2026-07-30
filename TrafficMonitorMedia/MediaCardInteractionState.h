#pragma once

#include <cstdint>
#include <string_view>

namespace media
{
    inline constexpr unsigned int kMediaCardSingleClickConfirmationMaximumMilliseconds = 300;

    [[nodiscard]] constexpr unsigned int CalculateMediaCardSingleClickConfirmationDelay(
        unsigned int system_double_click_milliseconds,
        bool has_left_double_click_action) noexcept
    {
        if (!has_left_double_click_action)
        {
            return 0;
        }
        return system_double_click_milliseconds < kMediaCardSingleClickConfirmationMaximumMilliseconds
            ? system_double_click_milliseconds
            : kMediaCardSingleClickConfirmationMaximumMilliseconds;
    }

    enum class MediaCardAnimationPhase
    {
        Opening,
        Closing,
    };

    struct MediaCardAnimationFrame
    {
        unsigned char alpha{};
        int offset_y{};
    };

    [[nodiscard]] constexpr double ClampMediaCardAnimationProgress(double progress) noexcept
    {
        return progress < 0.0 ? 0.0 : (progress > 1.0 ? 1.0 : progress);
    }

    [[nodiscard]] constexpr double EaseOutCubic(double progress) noexcept
    {
        const double t = ClampMediaCardAnimationProgress(progress);
        const double inverse = 1.0 - t;
        return 1.0 - inverse * inverse * inverse;
    }

    [[nodiscard]] constexpr double EaseInCubic(double progress) noexcept
    {
        const double t = ClampMediaCardAnimationProgress(progress);
        return t * t * t;
    }

    [[nodiscard]] constexpr MediaCardAnimationFrame CalculateMediaCardAnimationFrame(
        MediaCardAnimationPhase phase,
        double progress,
        int opening_offset_y,
        int closing_offset_y) noexcept
    {
        const double eased = phase == MediaCardAnimationPhase::Opening
            ? EaseOutCubic(progress)
            : EaseInCubic(progress);
        if (phase == MediaCardAnimationPhase::Opening)
        {
            return {
                static_cast<unsigned char>(eased * 255.0 + 0.5),
                static_cast<int>((1.0 - eased) * opening_offset_y + 0.5),
            };
        }
        return {
            static_cast<unsigned char>((1.0 - eased) * 255.0 + 0.5),
            static_cast<int>(eased * closing_offset_y + 0.5),
        };
    }

    [[nodiscard]] constexpr bool ShouldScheduleMediaCardOpen(
        std::uint64_t now_milliseconds,
        std::uint64_t suppression_deadline_milliseconds) noexcept
    {
        return now_milliseconds >= suppression_deadline_milliseconds;
    }

    [[nodiscard]] constexpr std::uint64_t CalculateMediaCardOpenSuppressionDeadline(
        std::uint64_t now_milliseconds,
        std::uint64_t double_click_interval_milliseconds) noexcept
    {
        return now_milliseconds + double_click_interval_milliseconds;
    }

    [[nodiscard]] constexpr bool IsTrustedMediaCardOwnerClass(
        std::wstring_view class_name) noexcept
    {
        return class_name == L"Shell_TrayWnd"
            || class_name == L"Shell_SecondaryTrayWnd";
    }

    using MediaCardMessageGeneration = std::uintptr_t;

    [[nodiscard]] constexpr bool IsCurrentMediaCardMessageGeneration(
        MediaCardMessageGeneration message_generation,
        MediaCardMessageGeneration current_generation) noexcept
    {
        return message_generation == current_generation;
    }

    [[nodiscard]] constexpr bool IsMediaCardActivationVerified(
        bool has_been_activated,
        bool is_foreground_card) noexcept
    {
        return has_been_activated && is_foreground_card;
    }

    [[nodiscard]] constexpr bool ShouldCompleteMediaCardOpening(
        bool opening_animation_completed,
        bool activation_verified) noexcept
    {
        return opening_animation_completed && activation_verified;
    }

    [[nodiscard]] constexpr bool ShouldScheduleMediaCardDeactivateFallback(
        bool dismiss_message_posted) noexcept
    {
        return !dismiss_message_posted;
    }

    enum class MediaCardLifecyclePhase
    {
        Closed,
        Opening,
        Open,
        Closing,
    };

    enum class MediaCardActivationEvent
    {
        Active,
        ClickActive,
        Inactive,
    };

    struct MediaCardActivationLifecycle
    {
        bool has_been_activated{};
        bool deactivate_dismiss_posted{};
        MediaCardLifecyclePhase phase{ MediaCardLifecyclePhase::Closed };

        [[nodiscard]] constexpr bool ShouldMarkActivated(MediaCardActivationEvent event) const noexcept
        {
            return (phase == MediaCardLifecyclePhase::Opening
                    || phase == MediaCardLifecyclePhase::Open)
                && (event == MediaCardActivationEvent::Active
                    || event == MediaCardActivationEvent::ClickActive);
        }

        [[nodiscard]] constexpr bool ShouldPostDeactivateDismiss(
            MediaCardActivationEvent event) const noexcept
        {
            return event == MediaCardActivationEvent::Inactive
                && has_been_activated
                && !deactivate_dismiss_posted
                && (phase == MediaCardLifecyclePhase::Opening
                    || phase == MediaCardLifecyclePhase::Open);
        }

        [[nodiscard]] constexpr bool ShouldDismissAfterDeferredDeactivate(
            bool is_current_card,
            bool is_foreground_card) const noexcept
        {
            return is_current_card
                && has_been_activated
                && !is_foreground_card
                && (phase == MediaCardLifecyclePhase::Opening
                    || phase == MediaCardLifecyclePhase::Open);
        }
    };

    struct MediaCardLifecycleState
    {
        bool close_in_progress{};
        bool card_marked_visible{};

        [[nodiscard]] constexpr bool ShouldHandleUnexpectedCardDestroy() const noexcept
        {
            return card_marked_visible && !close_in_progress;
        }
    };
}
