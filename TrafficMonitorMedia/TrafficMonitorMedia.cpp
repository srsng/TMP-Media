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
        break;
    }
    return nullptr;
}

const wchar_t* CTrafficMonitorMedia::GetTooltipInfo()
{
    return m_tooltip_info.c_str();
}

void CTrafficMonitorMedia::DataRequired()
{
    //TODO: 在此添加获取监控数据的代码
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
    static CString str;
    switch (index)
    {
    case TMI_NAME:
        return StringRes(IDS_PLUGIN_NAME).GetString();
    case TMI_DESCRIPTION:
        return StringRes(IDS_PLUGIN_DESCRIPTION).GetString();
    case TMI_AUTHOR:
        //TODO: 在此返回作者的名字
        return L"";
    case TMI_COPYRIGHT:
        //TODO: 在此返回版权信息
        return L"Copyright (C) by XXX 2021";
    case ITMPlugin::TMI_URL:
        //TODO: 在此返回URL
        return L"";
        break;
    case TMI_VERSION:
        //TODO: 在此修改插件的版本
        return L"1.00";
    default:
        break;
    }
    return L"";
}

void CTrafficMonitorMedia::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
    std::wstring config_dir = pApp->GetPluginConfigDir();
    LoadConfig(config_dir);
}

void* CTrafficMonitorMedia::GetPluginIcon()
{
    return GetIcon(IDI_ICON1);
}

void CTrafficMonitorMedia::LoadConfig(const std::wstring& config_dir)
{
    //TODO: 更改配置文件的文件名
    m_config_path = config_dir + L"KeyboardIndicator.ini";
    //TODO: 在此添加载入配置的代码
}

void CTrafficMonitorMedia::SaveConfig() const
{
    //TODO: 在此添加保存设置的代码
}

const CString& CTrafficMonitorMedia::StringRes(UINT id)
{
    auto iter = m_string_table.find(id);
    if (iter != m_string_table.end())
    {
        return iter->second;
    }
    else
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        m_string_table[id].LoadString(id);
        return m_string_table[id];
    }
}

HICON CTrafficMonitorMedia::GetIcon(UINT id)
{
    auto iter = m_icons.find(id);
    if (iter != m_icons.end())
    {
        return iter->second;
    }
    else
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0);
        m_icons[id] = hIcon;
        return hIcon;
    }
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

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CTrafficMonitorMedia::Instance();
}
