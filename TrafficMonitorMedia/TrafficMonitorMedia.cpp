#include "pch.h"
#include "TrafficMonitorMedia.h"
#include "OptionsDlg.h"

/////////////////////////////////////////////////////////////////////////////////////////////////
CTrafficMonitorMedia CTrafficMonitorMedia::m_instance;

CTrafficMonitorMedia::CTrafficMonitorMedia()
{
}

CTrafficMonitorMedia::~CTrafficMonitorMedia()
{
    m_media_service.Stop();
    SaveConfig();
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
    dlg.m_data = m_setting_data;
    if (dlg.DoModal() == IDOK)
    {
        m_setting_data = dlg.m_data;
        SaveConfig();
        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
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
        return L"https://github.com/zhongyang219/TrafficMonitor";
    case TMI_VERSION:
        return L"0.1.0";
    default:
        return L"";
    }
}

void CTrafficMonitorMedia::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
    std::wstring config_dir = pApp->GetPluginConfigDir();
    LoadConfig(config_dir);
    m_media_service.Start();
}

void* CTrafficMonitorMedia::GetPluginIcon()
{
    return GetIcon(IDI_ICON1);
}

void CTrafficMonitorMedia::LoadConfig(const std::wstring& config_dir)
{
    m_config_path = config_dir + L"TrafficMonitorMedia.ini";
}

void CTrafficMonitorMedia::SaveConfig() const
{
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

std::wstring CTrafficMonitorMedia::GetMediaDisplayText() const
{
    return m_media_service.GetDisplayText();
}

bool CTrafficMonitorMedia::HasMediaTimeline() const
{
    return m_media_service.HasTimeline();
}

double CTrafficMonitorMedia::GetMediaProgressFraction() const
{
    return m_media_service.GetProgressFraction();
}

void CTrafficMonitorMedia::RequestTogglePlayPause()
{
    m_media_service.RequestTogglePlayPause();
}

void CTrafficMonitorMedia::RequestSkipNext()
{
    m_media_service.RequestSkipNext();
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CTrafficMonitorMedia::Instance();
}
