#pragma once

#include "pch.h"
// 싱글톤 패턴
// 객체의 생성을 1개로 제한
// 어디서든 쉽게 접근 가능

// 1. 생성자 private에 숨기기
// * 객체를 만들수가 없어짐
// * 객체를 만들어주는 멤버 함수를 따로 만들어야함
// 멤버 함수를 부르려면 객체를 통해 불러야함
// -> 이는 satic 멤버함수로 해결(static 키워드가 붙은 멤버함수는 객체가 없어도 클래스 이름으로 호출가능)
// 그런데 static 멤버함수는 멤버에 접근이 안됌 (this 가 없다)
// *단 static 멤버함수는 static 멤버 변수는 접근이 가능
// 또한 클래스 밖에서 정적멤버변수는 항상 초기화를 해주어야한다.

class CTexture;

class CCore
{

	SINGLE(CCore);
private:
	HWND	    m_hWnd;			// 메인 위도우 핸들
	POINT	    m_ptResolution;	// 메인 윈도우 해상도
	HDC		    m_hDC;			// 메인 윈도우에 Draw 할 DC
	
    CTexture*   m_pMemTex;    // 백버퍼 텍스쳐

	// 자주 사용하는 GDI Object
	HBRUSH	m_arrBrush[(UINT)BRUSH_TYPE::END];
	HPEN	m_arrPen[(UINT)PEN_TYPE::END];


    // 메뉴
    HMENU   m_hMenu; // TOOL SCENE에서만 사용
public:
	int init(HWND _hWnd, POINT _ptresolution);
	void progress();

public:
    void DockMenu();
    void DivideMenu();
    void ChangeWindowSize(Vec2 _vResolution, bool _bMenu);

private:
    void CreateBrushPen();
    void Clear();

public:
	HWND GetMainHwnd() { return m_hWnd; }
	HDC GetMainDC() { return m_hDC; }
	POINT GetResolution() { return m_ptResolution; }
 
public:

	HBRUSH GetBrush(BRUSH_TYPE _eType) { return m_arrBrush[(UINT)_eType]; }
	HPEN GetPen(PEN_TYPE _eType) { return m_arrPen[(UINT)_eType]; }
};
