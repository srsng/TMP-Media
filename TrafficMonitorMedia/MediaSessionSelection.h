#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace media
{
    using SessionIdentity = std::uintptr_t;

    inline constexpr SessionIdentity kNoSessionIdentity{};
    inline constexpr std::size_t kNoSessionIndex = (std::numeric_limits<std::size_t>::max)();

    enum class SessionSwitchDirection
    {
        Previous,
        Next,
    };

    struct SessionSelection
    {
        std::size_t selected_index{ kNoSessionIndex };
        bool manual_selection{};

        [[nodiscard]] constexpr bool HasSelection() const noexcept
        {
            return selected_index != kNoSessionIndex;
        }
    };

    [[nodiscard]] constexpr std::size_t FindSessionIndex(
        std::span<const SessionIdentity> sessions,
        SessionIdentity identity) noexcept
    {
        if (identity == kNoSessionIdentity)
        {
            return kNoSessionIndex;
        }

        for (std::size_t index = 0; index < sessions.size(); ++index)
        {
            if (sessions[index] == identity)
            {
                return index;
            }
        }
        return kNoSessionIndex;
    }

    [[nodiscard]] constexpr SessionSelection ResolveSessionSelection(
        std::span<const SessionIdentity> sessions,
        SessionIdentity system_current,
        SessionIdentity selected,
        bool manual_selection) noexcept
    {
        if (sessions.empty())
        {
            return {};
        }

        if (manual_selection)
        {
            const std::size_t selected_index = FindSessionIndex(sessions, selected);
            if (selected_index != kNoSessionIndex)
            {
                return { selected_index, true };
            }
        }

        const std::size_t system_index = FindSessionIndex(sessions, system_current);
        if (system_index != kNoSessionIndex)
        {
            return { system_index, false };
        }

        return { 0, false };
    }

    [[nodiscard]] constexpr SessionSelection SelectPreviousSession(
        std::span<const SessionIdentity> sessions,
        SessionIdentity system_current,
        SessionIdentity selected,
        bool manual_selection) noexcept
    {
        SessionSelection selection = ResolveSessionSelection(
            sessions,
            system_current,
            selected,
            manual_selection);
        if (sessions.size() <= 1 || !selection.HasSelection())
        {
            selection.manual_selection = false;
            return selection;
        }

        selection.selected_index = selection.selected_index == 0
            ? sessions.size() - 1
            : selection.selected_index - 1;
        selection.manual_selection = true;
        return selection;
    }

    [[nodiscard]] constexpr SessionSelection SelectNextSession(
        std::span<const SessionIdentity> sessions,
        SessionIdentity system_current,
        SessionIdentity selected,
        bool manual_selection) noexcept
    {
        SessionSelection selection = ResolveSessionSelection(
            sessions,
            system_current,
            selected,
            manual_selection);
        if (sessions.size() <= 1 || !selection.HasSelection())
        {
            selection.manual_selection = false;
            return selection;
        }

        selection.selected_index = (selection.selected_index + 1) % sessions.size();
        selection.manual_selection = true;
        return selection;
    }
}
