#include "pch.h"
#include "TrafficMonitorMedia.h"
#include "OptionsDlg.h"

#include <cerrno>
#include <cwchar>
#include <limits>
#include <optional>
#include <string_view>

namespace
{
    constexpr wchar_t kConfigFileName[] = L"TrafficMonitorMedia.ini";
    constexpr wchar_t kDisplaySection[] = L"display";
    constexpr wchar_t kInputSection[] = L"input";

    std::wstring BuildConfigPath(const std::wstring& config_dir)
    {
        if (config_dir.empty())
        {
            return {};
        }

        std::wstring path = config_dir;
        if (path.back() != L'\\' && path.back() != L'/')
        {
            path.push_back(L'\\');
        }
        path.append(kConfigFileName);
        return path;
    }

    std::optional<std::wstring> ReadProfileValue(
        const std::wstring& path,
        const wchar_t* section,
        const wchar_t* key)
    {
        wchar_t buffer[128]{};
        const DWORD length = GetPrivateProfileStringW(
            section,
            key,
            nullptr,
            buffer,
            static_cast<DWORD>(std::size(buffer)),
            path.c_str());
        if (length == 0)
        {
            return std::nullopt;
        }
        return std::wstring(buffer, length);
    }

    bool ReadProfileBool(
        const std::wstring& path,
        const wchar_t* section,
        const wchar_t* key,
        bool fallback)
    {
        const auto value = ReadProfileValue(path, section, key);
        if (!value)
        {
            return fallback;
        }
        if (*value == L"0")
        {
            return false;
        }
        if (*value == L"1")
        {
            return true;
        }
        return fallback;
    }

    int ReadProfileInt(
        const std::wstring& path,
        const wchar_t* section,
        const wchar_t* key,
        int fallback)
    {
        const auto value = ReadProfileValue(path, section, key);
        if (!value)
        {
            return fallback;
        }

        wchar_t* end{};
        errno = 0;
        const long parsed = std::wcstol(value->c_str(), &end, 10);
        if (errno == ERANGE || end == value->c_str() || *end != L'\0' ||
            parsed < (std::numeric_limits<int>::min)() || parsed > (std::numeric_limits<int>::max)())
        {
            return fallback;
        }
        return static_cast<int>(parsed);
    }

    media::MediaControlAction ReadProfileAction(
        const std::wstring& path,
        const wchar_t* key,
        media::MediaControlAction fallback)
    {
        const auto value = ReadProfileValue(path, kInputSection, key);
        return value ? media::ParseConfigValue(*value, fallback) : fallback;
    }

    void WriteProfileValue(
        const std::wstring& path,
        const wchar_t* section,
        const wchar_t* key,
        std::wstring_view value)
    {
        const std::wstring terminated_value(value);
        WritePrivateProfileStringW(section, key, terminated_value.c_str(), path.c_str());
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
CTrafficMonitorMedia CTrafficMonitorMedia::m_instance;

CTrafficMonitorMedia::CTrafficMonitorMedia()
{
}

CTrafficMonitorMedia::~CTrafficMonitorMedia()
{
    m_media_service.Stop();
    SaveConfig(GetSettingsSnapshot());
}

CTrafficMonitorMedia& CTrafficMonitorMedia::Instance()
{
    return m_instance;
}

IPluginItem* CTrafficMonitorMedia::GetItem(int index)
{
    switch (index)
    {
    case 0:
        return &m_item;
    default:
        return nullptr;
    }
}

const wchar_t* CTrafficMonitorMedia::GetTooltipInfo()
{
    m_tooltip_info = m_media_service.GetTooltipText();
    return m_tooltip_info.c_str();
}

void CTrafficMonitorMedia::DataRequired()
{
    // 此函数由 TrafficMonitor 定时调用；只通知后台线程刷新，绝不在 UI 线程阻塞等待 GSMTC。
    m_media_service.RequestRefresh();
}

ITMPlugin::OptionReturn CTrafficMonitorMedia::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd* pParent = CWnd::FromHandle((HWND)hParent);
    COptionsDlg dlg(pParent);
    const media::SettingData old_settings = GetSettingsSnapshot();
    dlg.m_data = old_settings;
    if (dlg.DoModal() != IDOK)
    {
        return ITMPlugin::OR_OPTION_UNCHANGED;
    }

    const media::SettingData new_settings = media::NormalizeSettings(dlg.m_data);
    if (new_settings == old_settings)
    {
        return ITMPlugin::OR_OPTION_UNCHANGED;
    }

    PublishSettings(new_settings);
    SaveConfig(new_settings);
    return ITMPlugin::OR_OPTION_CHANGED;
}

const wchar_t* CTrafficMonitorMedia::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return StringRes(IDS_PLUGIN_NAME).GetString();
    case TMI_DESCRIPTION:
        return StringRes(IDS_PLUGIN_DESCRIPTION).GetString();
    case TMI_AUTHOR:
        return L"TMP-media";
    case TMI_COPYRIGHT:
        return L"Copyright (C) TMP-media";
    case TMI_URL:
        return L"https://github.com/srsng/TMP-media";
    case TMI_VERSION:
        return L"1.0.0";
    default:
        return L"";
    }
}

void CTrafficMonitorMedia::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
    LoadConfig(pApp->GetPluginConfigDir());
    m_media_service.Start();
}

void* CTrafficMonitorMedia::GetPluginIcon()
{
    return GetIcon(IDI_ICON1);
}

void CTrafficMonitorMedia::LoadConfig(const std::wstring& config_dir)
{
    m_config_path = BuildConfigPath(config_dir);

    media::SettingData settings;
    if (!m_config_path.empty())
    {
        settings.show_progress = ReadProfileBool(
            m_config_path,
            kDisplaySection,
            L"show_progress",
            settings.show_progress);
        settings.show_status_icon = ReadProfileBool(
            m_config_path,
            kDisplaySection,
            L"show_status_icon",
            settings.show_status_icon);
        settings.show_artist_on_second_line = ReadProfileBool(
            m_config_path,
            kDisplaySection,
            L"show_artist_on_second_line",
            settings.show_artist_on_second_line);
        settings.max_title_width = ReadProfileInt(
            m_config_path,
            kDisplaySection,
            L"max_title_width",
            settings.max_title_width);
        settings.input.left_click = ReadProfileAction(
            m_config_path,
            L"left_click",
            settings.input.left_click);
        settings.input.left_double_click = ReadProfileAction(
            m_config_path,
            L"left_double_click",
            settings.input.left_double_click);
        settings.input.right_click = ReadProfileAction(
            m_config_path,
            L"right_click",
            settings.input.right_click);
        settings.input.wheel_up = ReadProfileAction(
            m_config_path,
            L"wheel_up",
            settings.input.wheel_up);
        settings.input.wheel_down = ReadProfileAction(
            m_config_path,
            L"wheel_down",
            settings.input.wheel_down);
    }

    PublishSettings(media::NormalizeSettings(settings));
}

void CTrafficMonitorMedia::SaveConfig(const media::SettingData& settings) const
{
    if (m_config_path.empty())
    {
        return;
    }

    const media::SettingData normalized = media::NormalizeSettings(settings);
    WriteProfileValue(
        m_config_path,
        kDisplaySection,
        L"show_progress",
        normalized.show_progress ? L"1" : L"0");
    WriteProfileValue(
        m_config_path,
        kDisplaySection,
        L"show_status_icon",
        normalized.show_status_icon ? L"1" : L"0");
    WriteProfileValue(
        m_config_path,
        kDisplaySection,
        L"show_artist_on_second_line",
        normalized.show_artist_on_second_line ? L"1" : L"0");
    WriteProfileValue(
        m_config_path,
        kDisplaySection,
        L"max_title_width",
        std::to_wstring(normalized.max_title_width));
    WriteProfileValue(
        m_config_path,
        kInputSection,
        L"left_click",
        media::ToConfigValue(normalized.input.left_click));
    WriteProfileValue(
        m_config_path,
        kInputSection,
        L"left_double_click",
        media::ToConfigValue(normalized.input.left_double_click));
    WriteProfileValue(
        m_config_path,
        kInputSection,
        L"right_click",
        media::ToConfigValue(normalized.input.right_click));
    WriteProfileValue(
        m_config_path,
        kInputSection,
        L"wheel_up",
        media::ToConfigValue(normalized.input.wheel_up));
    WriteProfileValue(
        m_config_path,
        kInputSection,
        L"wheel_down",
        media::ToConfigValue(normalized.input.wheel_down));
}

media::SettingData CTrafficMonitorMedia::GetSettingsSnapshot() const
{
    std::lock_guard lock(m_settings_mutex);
    return m_setting_data;
}

void CTrafficMonitorMedia::PublishSettings(const media::SettingData& settings)
{
    std::lock_guard lock(m_settings_mutex);
    m_setting_data = media::NormalizeSettings(settings);
}

const CString& CTrafficMonitorMedia::StringRes(UINT id)
{
    auto iter = m_string_table.find(id);
    if (iter != m_string_table.end())
    {
        return iter->second;
    }

    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    m_string_table[id].LoadString(id);
    return m_string_table[id];
}

HICON CTrafficMonitorMedia::GetIcon(UINT id)
{
    auto iter = m_icons.find(id);
    if (iter != m_icons.end())
    {
        return iter->second;
    }

    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    HICON hIcon = static_cast<HICON>(
        LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0));
    m_icons[id] = hIcon;
    return hIcon;
}

int CTrafficMonitorMedia::DPI(int pixel)
{
    if (m_app != nullptr)
    {
        int dpi = m_app->GetDPI(ITrafficMonitor::DPI_TASKBAR);
        return dpi * pixel / 96;
    }
    return pixel;
}

MediaTitleSnapshot CTrafficMonitorMedia::GetMediaSnapshot() const
{
    return m_media_service.GetSnapshot();
}

void CTrafficMonitorMedia::RequestSwitchSession(media::SessionSwitchDirection direction)
{
    m_media_service.RequestSwitchSession(direction);
}

void CTrafficMonitorMedia::RequestImmediateAction(media::MediaControlAction action)
{
    m_media_service.RequestImmediateAction(action);
}

void CTrafficMonitorMedia::RequestSingleClick(media::MediaControlAction action)
{
    m_media_service.RequestSingleClick(action);
}

void CTrafficMonitorMedia::RequestDoubleClick(media::MediaControlAction action)
{
    m_media_service.RequestDoubleClick(action);
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CTrafficMonitorMedia::Instance();
}
