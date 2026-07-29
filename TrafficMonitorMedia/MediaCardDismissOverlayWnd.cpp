#include "pch.h"
#include "MediaCardDismissOverlayWnd.h"
#include "MediaCardWindowManager.h"

BEGIN_MESSAGE_MAP(CMediaCardDismissOverlayWnd, CWnd)
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_RBUTTONDOWN()
    ON_WM_MBUTTONDOWN()
END_MESSAGE_MAP()

BOOL CMediaCardDismissOverlayWnd::Create(CMediaCardWindowManager* manager)
{
    m_manager = manager;
    const CRect virtual_screen(
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN));

    const CString class_name = AfxRegisterWndClass(
        CS_DBLCLKS,
        LoadCursor(nullptr, IDC_ARROW),
        nullptr,
        nullptr);
    if (!CreateEx(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE,
            class_name,
            L"",
            WS_POPUP,
            virtual_screen,
            nullptr,
            0))
    {
        m_manager = nullptr;
        return FALSE;
    }

    SetLayeredWindowAttributes(0, 1, LWA_ALPHA);
    ShowWindow(SW_SHOWNOACTIVATE);
    return TRUE;
}

BOOL CMediaCardDismissOverlayWnd::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CMediaCardDismissOverlayWnd::OnLButtonDown(UINT, CPoint)
{
    Dismiss();
}

void CMediaCardDismissOverlayWnd::OnRButtonDown(UINT, CPoint)
{
    Dismiss();
}

void CMediaCardDismissOverlayWnd::OnMButtonDown(UINT, CPoint)
{
    Dismiss();
}

void CMediaCardDismissOverlayWnd::Dismiss()
{
    if (m_manager != nullptr)
    {
        m_manager->Close();
    }
}
