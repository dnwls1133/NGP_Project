#pragma once
#include "CObject.h"
class CTexture;
class CTile :
    public CObject
{
private:
    CTexture* m_pTileTex;
    int       m_iImgidx;
public:
    void SetTexture(CTexture* _pTex)
    {
        m_pTileTex = _pTex;
    }

    void AddImgIdx()
    {
        ++m_iImgidx;
        if (m_iImgidx >= 16)
        {
            m_iImgidx = 0;
        }
    }

private:
    virtual void render(HDC _dc);
    virtual void update();
public:

    virtual void Save(FILE* _pFile);
    virtual void Load(FILE* _pFile);

    CLONE(CTile);
public:
    CTile();
    ~CTile();
};

