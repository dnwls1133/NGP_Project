#pragma once
#include "CResource.h"
class CTexture :
    public CResource
{
private:
    HDC         m_hdc;
    HBITMAP     m_hBit;
  
    BITMAP      m_bitInfo;
public:
    void Load(const wstring& _strFilePath);
    void Create(UINT _iWidth, UINT _iHeight);

    UINT Width() { return m_bitInfo.bmWidth; }
    UINT Height() { return m_bitInfo.bmHeight; }

    HDC GetDC() { return m_hdc; }
    HBITMAP GetBitmap() { return m_hBit; }
  
public:
    CTexture();
   virtual  ~CTexture();
};

