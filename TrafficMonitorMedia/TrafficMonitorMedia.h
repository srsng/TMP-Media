#pragma once
#include "..\include\PluginInterface.h"
#include "MediaSessionService.h"
#include "MediaCardWindowManager.h"
#include "MediaSettings.h"
#include "SystemVolumeService.h"
#include "TrafficMonitorMediaItem.h"
#include <map>
#include <mutex>
#include <string>

#define g_plugin CTrafficMonitorMedia::Instance()

class CTrafficMonitorMedia : public ITMPlugin
{
private:
    CTrafficMonitorMedia();
    ~CTrafficMonitorMedia();

public:
    static CTrafficMonitorMedia& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual void OnInitialize(ITrafficMonitor* pApp) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void* GetPluginIcon() override;

    const CString& StringRes(UINT id);
    HICON GetIcon(UINT id);
    int DPI(int pixel);
    [[nodiscard]] media::SettingData GetSettingsSnapshot() const;
    [[nodiscard]] MediaTitleSnapshot GetMediaSnapshot() const;
    void RequestSwitchSession(media::SessionSwitchDirection direction);
    void RequestImmediateAction(media::MediaControlAction action);
    void RequestSingleClick(
        media::MediaControlAction action,
        media::MediaItemHitRegion hit_region,
        unsigned int confirmation_delay_milliseconds);
    void RequestDoubleClick(
        media::MediaControlAction action,
        media::MediaItemHitRegion hit_region);
    void RequestAdjustSystemVolume(float delta);
    void ScheduleOpenMediaCard(
        HWND anchor_window,
        int client_x,
        int client_y,
        unsigned int confirmation_delay_milliseconds,
        media::MediaItemHitRegion hit_region);
    void OpenMediaCard(HWND anchor_window, int client_x, int client_y);
    void SuppressScheduledMediaCardOpenAfterDoubleClick(media::MediaItemHitRegion hit_region);
    void CloseMediaCard();
    void SetMediaCardVisible(bool visible);
    void RequestSeekToPosition(media::SessionIdentity session_identity, std::int64_t position_ticks);

private:
    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig(const media::SettingData& settings) const;
    void PublishSettings(const media::SettingData& settings);

    static CTrafficMonitorMedia m_instance;
    CTrafficMonitorMediaItem m_item;
    CMediaSessionService m_media_service;
    CMediaCardWindowManager m_media_card_manager;
    CSystemVolumeService m_system_volume_service;
    std::wstring m_tooltip_info;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    ITrafficMonitor* m_app{};
    mutable std::mutex m_settings_mutex;
    media::SettingData m_setting_data;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
