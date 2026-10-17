#pragma once
#include "CObject.h"
class CUI :
    public CObject
{
private:
    vector<CUI*>        m_veChildUI;
    CUI*                m_pParentUI;
    Vec2                m_vFinalPos;

    bool                m_bCamAffected; // UI 카메라 영향유무
    bool                m_bMouseOn;     // UI 위에 마우스가 있는지
    bool                m_bPMouseOn;    // UI 위에 마우스가 있는지 과거 정보
    bool                m_bLbtnDown;    // UI 위에서 LBTN이 눌렸는지
public:
   
    Vec2 GetFinalPos() { return m_vFinalPos; }
    CUI* GetParent() { return m_pParentUI; }
    bool IsMouseOn() { return m_bMouseOn; }
    bool IsLbtnDown() { return m_bLbtnDown; }

    void AddChild(CUI* _pUI) { m_veChildUI.push_back(_pUI); _pUI->m_pParentUI = this; }
    const vector<CUI*>& GetChildUI() { return m_veChildUI; }


public:
    virtual void update();
    virtual void finalupdate();
    virtual void render(HDC _dc);

private:
    void update_child();
    void finalupdate_child();
    void render_child(HDC _dc);

    void MouseOnCheck();

public:
    virtual void MouseOn();
    virtual void MouseOff();
    virtual void MouseLbtnDown();
    virtual void MouseLbtnUP();
    virtual void MouseLbtnClicked();
    virtual void MouseRbtnDown();
    virtual void MouseRbtnUP();
    virtual void MouseRbtnClicked();
    
    virtual CUI* Clone() = 0;
public:
    CUI(bool _bCamAff);
    CUI(const CUI& _origin);
    virtual ~CUI();

    friend class CUIMgr;
};

