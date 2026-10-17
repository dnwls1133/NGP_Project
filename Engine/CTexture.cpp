#include "pch.h"
#include "CTexture.h"
#include "CCore.h"

CTexture::CTexture()
	: m_hBit(0)
	, m_hdc(0)
	, m_bitInfo{}
{
}

CTexture::~CTexture()
{
	
	DeleteDC(m_hdc);
	DeleteObject(m_hBit);
}

void CTexture::Load(const wstring& _strFilePath)
{
	CImage image;
	HRESULT hr = image.Load(_strFilePath.c_str());
	if (SUCCEEDED(hr)) {
		m_hBit = image.Detach(); // CImage 내부의 HBITMAP을 넘긴다
	}
	//m_hBit = (HBITMAP)LoadImage(nullptr, _strFilePath.c_str(), IMAGE_BITMAP
	//	, 0, 0, LR_CREATEDIBSECTION | LR_LOADFROMFILE);
	// 비트맵과 연결할 DC
	m_hdc = CreateCompatibleDC(CCore::GetInst()->GetMainDC());
	
	// 비트맵과 DC 연결

	HBITMAP hPrevBit = (HBITMAP)SelectObject(m_hdc, m_hBit);
	DeleteObject(hPrevBit);
	
	// 비트맵 정보
	GetObject(m_hBit, sizeof(BITMAP), &m_bitInfo);

	
	assert(m_hBit);
}

void CTexture::Create(UINT _iWidth, UINT _iHeight)
{
   HDC maindc = CCore::GetInst()->GetMainDC();
   m_hBit = CreateCompatibleBitmap(maindc, _iWidth, _iHeight);
   m_hdc = CreateCompatibleDC(maindc);

   // Select the new bitmap into the DC and store the old bitmap
   HBITMAP hOldBit = (HBITMAP)SelectObject(m_hdc, m_hBit);

   // Ensure the old bitmap is deleted only if it exists
   if (hOldBit != nullptr)
   {
       DeleteObject(hOldBit);
   }

   // Retrieve bitmap information
   GetObject(m_hBit, sizeof(BITMAP), &m_bitInfo);

   // Assert to ensure the bitmap creation was successful
   assert(m_hBit);
}

