#pragma once

class CMediaCardWindowManager;

class CMediaCardDismissOverlayWnd : public CWnd
{
public:
    BOOL Create(CMediaCardWindowManager* manager);

protected:
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnRButtonDown(UINT flags, CPoint point);
    afx_msg void OnMButtonDown(UINT flags, CPoint point);
    DECLARE_MESSAGE_MAP()

private:
    void Dismiss();

    CMediaCardWindowManager* m_manager{};
};
