#include "pch.h"
#include "TrafficMonitorMediaItem.h"
#include "TrafficMonitorMedia.h"

const wchar_t* CTrafficMonitorMediaItem::GetItemName() const
{
    return g_plugin.StringRes(IDS_PLUGIN_ITEM_NAME);
}

const wchar_t* CTrafficMonitorMediaItem::GetItemId() const
{
	//TODO: 在此返回插件的唯一ID，建议只包含字母和数字
    return L"";
}

const wchar_t* CTrafficMonitorMediaItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* CTrafficMonitorMediaItem::GetItemValueText() const
{
    return L"";
}

const wchar_t* CTrafficMonitorMediaItem::GetItemValueSampleText() const
{
    return L"";
}

bool CTrafficMonitorMediaItem::IsCustomDraw() const
{
	//TODO: 根据是否由插件自绘返回对应的值
    return true;
}

int CTrafficMonitorMediaItem::GetItemWidthEx(void * hDC) const
{
    //绘图句柄
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    //TODO: 如果插件需要自绘，则在此修改显示区域的宽度
    return 60;
}

void CTrafficMonitorMediaItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    //绘图句柄
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    //矩形区域
    CRect rect(CPoint(x, y), CSize(w, h));
    //TODO: 在此添加绘图代码
}
