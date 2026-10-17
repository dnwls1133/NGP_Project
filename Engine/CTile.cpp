#include "pch.h"
#include "CTile.h"
#include "CTexture.h"

CTile::CTile()
	: m_pTileTex(nullptr)
	, m_iImgidx(0)
{
	SetScale(Vec2(TILE_SIZE, TILE_SIZE));
}

CTile::~CTile()
{
}

void CTile::render(HDC _dc)
{
	if (nullptr == m_pTileTex || 0 > m_iImgidx)
	{
		return;
	}

	UINT iWidit = m_pTileTex->Width();
	UINT iHeight = m_pTileTex->Height();

	UINT iMaxCol = iWidit / TILE_SIZE;
	UINT iMaxRaw = iHeight / TILE_SIZE;
	UINT iCurRow = (UINT)m_iImgidx / iMaxCol;
	UINT iCurCol = (UINT)m_iImgidx % iMaxCol;


	// 이미지 범위를 벗어난 인덱스
	if (iMaxRaw <= iCurRow)
	{
		//assert(nullptr);
	}



	Vec2 vRenderPos = CCamera::GetInst()->GetRenderPos(GetPos());
	Vec2 vScale = GetScale();
	BitBlt(_dc
		, (int)vRenderPos.x
		, (int)vRenderPos.y
		, (int)vScale.x
		, (int)vScale.y
		, m_pTileTex->GetDC()
		, iCurCol * TILE_SIZE
		, iCurRow * TILE_SIZE
		, SRCCOPY);
}

void CTile::update()
{
}

void CTile::Save(FILE* _pFile)
{
    fwrite(&m_iImgidx, sizeof(int), 1, _pFile);
}

void CTile::Load(FILE* _pFile)
{
    fread(&m_iImgidx, sizeof(int), 1, _pFile);
}
