#include "pch.h"
#include "MissileMgr.h"

#include "CSceneMgr.h"
#include "CScene.h"

#include "CPlayer.h"
#include "CMonster.h"
#include "CMissile.h"

#include "CTimeMgr.h"

void CreateStraightMissile(Vec2 vMonPos, Vec2 vMonScale, float _speed, MISSILE_TYPE _eType)
{
    Vec2 vMissilePos = vMonPos;
    vMissilePos.y += vMonScale.y / 2.f;

    CScene* CurScene = CSceneMgr::GetInst()->GetCurScene();
    Vec2 vMDPos = Vec2(0, -100);
    vMDPos.x += 2 * PI / 30.f;
   
    vMDPos.Normalize();
    // Missile Object
    CMissile* pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vMDPos);
    pMissile->SetVec(_speed);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
    vMDPos.x -= 2 * PI / 30.f;
    pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vMDPos);
    pMissile->SetVec(_speed);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
    vMDPos.x += 4 * PI / 30.f;
    pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vMDPos);
    pMissile->SetVec(_speed);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
    vMDPos.x -= 6 * PI / 30.f;
    pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vMDPos);
    pMissile->SetVec(_speed);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
    vMDPos.x += 8 * PI / 30.f;
    pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vMDPos);
    pMissile->SetVec(_speed);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
}

void CreateStraightMissile1(Vec2 vMonPos, Vec2 vMonScale, MISSILE_TYPE _eType)
{
    Vec2 vMissilePos = vMonPos;
    vMissilePos.y += vMonScale.y / 2.f;

    CScene* CurScene = CSceneMgr::GetInst()->GetCurScene();
    Vec2 vMDPos = Vec2(0, -100);
    

    vMDPos.Normalize();
    // Missile Object
    CMissile* pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vMDPos);
    pMissile->SetVec(550);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);

}

void CreateMissile(Vec2 vMonPos, Vec2 vMonScale,int _speedtype, MISSILE_TYPE _eType)
{
    Vec2 vMissilePos = vMonPos;
    vMissilePos.y += vMonScale.y / 2.f;

    CScene* CurScene = CSceneMgr::GetInst()->GetCurScene();
    CObject* pInfo = CurScene->GetPlayer(0);
    Vec2 vpPos = pInfo->GetPos();
    Vec2 vMDPos = Vec2(0, 10);
    vMDPos.x += 2 * PI / 6.f;
    double dx = vMissilePos.x;
    double dy = vMissilePos.y;
    double distance = sqrt(dx * dx + dy * dy);
    if (vMissilePos.x - vpPos.x < 0)
    {
        vpPos.x = -(vMissilePos.x - vpPos.x);
    }
    else if (vMissilePos.x - vpPos.x > 0)
    {
        vpPos.x = -(vMissilePos.x - vpPos.x);
    }
    vpPos.y = vMissilePos.y - vpPos.y;
    vpPos.Normalize();
    vMDPos.Normalize();
    // Missile Object
    CMissile* pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    switch (_eType)
    {
    case MISSILE_TYPE::SMALL:
    {
        pMissile->SetScale(Vec2(20.f, 20.f));
    }
    break;
    case MISSILE_TYPE::MIDDLE:
    {
        pMissile->SetScale(Vec2(40.f, 40.f));
    }
    break;
    case MISSILE_TYPE::SPIN:
    {
        pMissile->SetScale(Vec2(20.f, 20.f));
    }
    break;
    }
    
    pMissile->SetDir(vpPos);
    pMissile->SetVec(400);
    pMissile->SetType(_speedtype);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
}

void CreateMissile1(Vec2 vMonPos, Vec2 vMonScale, MISSILE_TYPE _eType)
{
    Vec2 vMissilePos = vMonPos;
    vMissilePos.y += vMonScale.y / 2.f;
    vMissilePos.x += vMonScale.x / 2.f;
    CScene* CurScene = CSceneMgr::GetInst()->GetCurScene();
    CObject* pInfo = CurScene->GetPlayer(0);
    Vec2 vpPos = pInfo->GetPos();
   
    Vec2 vMDPos = Vec2(0, 10);
    vMDPos.x += 2 * PI / 6.f;
    double dx = vMissilePos.x;
    double dy = vMissilePos.y;
    double distance = sqrt(dx * dx + dy * dy);
    if (vMissilePos.x - vpPos.x < 0)
    {
        vpPos.x = -(vMissilePos.x - vpPos.x);
    }
    else if (vMissilePos.x - vpPos.x > 0)
    {
        vpPos.x = -(vMissilePos.x - vpPos.x);
    }
    vpPos.y = vMissilePos.y - vpPos.y;
    vpPos.Normalize();
    vMDPos.Normalize();
    // Missile Object
    CMissile* pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(400);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
    vpPos.x += PI / 18.f;
    vpPos.Normalize();
    pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(400);
    pMissile->SetType(1);
    pMissile->ChangemType(_eType);
    pMissile->SetName(L"MsMissile");
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);

}



void CreateMissile2(Vec2 vMonPos, Vec2 vMonScale, MISSILE_TYPE _eType)
{
    Vec2 vMissilePos = vMonPos;
    vMissilePos.y += vMonScale.y / 2.f;
    vMissilePos.x -= vMonScale.x / 2.f;
    CScene* CurScene = CSceneMgr::GetInst()->GetCurScene();
    CObject* pInfo = CurScene->GetPlayer(0);
    Vec2 vpPos = pInfo->GetPos();
   
    Vec2 vMDPos = Vec2(0, 10);
    vMDPos.x += 2 * PI / 6.f;
    double dx = vMissilePos.x;
    double dy = vMissilePos.y;
    double distance = sqrt(dx * dx + dy * dy);
    if (vMissilePos.x - vpPos.x < 0)
    {
        vpPos.x = -(vMissilePos.x - vpPos.x);
    }
    else if (vMissilePos.x - vpPos.x > 0)
    {
        vpPos.x = -(vMissilePos.x - vpPos.x);
    }
    vpPos.y = vMissilePos.y - vpPos.y;
    vpPos.Normalize();
    vMDPos.Normalize();
    // Missile Object
    CMissile* pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(400);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
    vpPos.x += PI / 18.f;
    vpPos.Normalize();
    pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(400);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);

}

void CreateMissile3(Vec2 vMonPos, Vec2 vMonScale,float fXDir, MISSILE_TYPE _eType)
{
    Vec2 vMissilePos = vMonPos;
    vMissilePos.y += 50.f;

    CScene* CurScene = CSceneMgr::GetInst()->GetCurScene();
    CObject* pInfo = CurScene->GetPlayer(0);
    Vec2 vpPos = Vec2(fXDir, -3*PI / 2.f);
   
    vpPos.Normalize();
    
    // Missile Object
    CMissile* pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(400);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
    vpPos.x *= -1;
    pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(400);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
   
    vMissilePos.x -= vMonScale.x / 2.f;
    vpPos.x *= -1;
    pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(400);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
    vpPos.x *= -1;
    pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(400);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
    vpPos.x *= -1;
    vMissilePos.x += vMonScale.x;
    pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(400);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
    vpPos.x *= -1;
    pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(400);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
}

void CreateMissile4(Vec2 vMonPos, Vec2 vMonScale,int _itype,  MISSILE_TYPE _eType)
{
    Vec2 vMissilePos = vMonPos;
    vMissilePos.y += 100.f;

    CScene* CurScene = CSceneMgr::GetInst()->GetCurScene();
    CObject* pInfo = CurScene->GetPlayer(0);
    Vec2 vpPos = Vec2(-2*PI, -3 * PI / 2.f);

    
    for (int i = 0; i < 9; ++i)
    {
        
        vpPos.Normalize();

        // Missile Object
        CMissile* pMissile = new CMissile;
        pMissile->init(GROUP_TYPE::PROJ_MONSTER);
        pMissile->SetPos(vMissilePos);
        pMissile->SetScale(Vec2(20.f, 20.f));
        pMissile->SetDir(vpPos);
        pMissile->SetVec(400);
        pMissile->SetType(_itype);
        pMissile->SetName(L"MsMissile");
        pMissile->ChangemType(_eType);
        CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
        vpPos.x += 2 * PI / 18.f;
    }

    vMissilePos.x -= vMonScale.x / 2.f;
    vpPos = Vec2(-2 * PI, -3 * PI / 2.f);
    for (int i = 0; i < 9; ++i)
    {

        vpPos.Normalize();

        // Missile Object
        CMissile* pMissile = new CMissile;
        pMissile->init(GROUP_TYPE::PROJ_MONSTER);
        pMissile->SetPos(vMissilePos);
        pMissile->SetScale(Vec2(20.f, 20.f));
        pMissile->SetDir(vpPos);
        pMissile->SetVec(400);
        pMissile->SetType(_itype);
        pMissile->SetName(L"MsMissile");
        pMissile->ChangemType(_eType);
        CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
        vpPos.x += 2 * PI / 18.f;
    }
    vMissilePos.x += vMonScale.x;
    vpPos = Vec2(-2 * PI, -3 * PI / 2.f);
    for (int i = 0; i < 9; ++i)
    {

        vpPos.Normalize();

        // Missile Objec
        CMissile* pMissile = new CMissile;
        pMissile->init(GROUP_TYPE::PROJ_MONSTER);
        pMissile->SetPos(vMissilePos);
        pMissile->SetScale(Vec2(20.f, 20.f));
        pMissile->SetDir(vpPos);
        pMissile->SetVec(400);
        pMissile->SetType(_itype);
        pMissile->SetName(L"MsMissile");
        pMissile->ChangemType(_eType);
        CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
        vpPos.x += 2 * PI / 18.f;
    }
}

void CreateMissile5(Vec2 vMonPos, Vec2 vMonScale, MISSILE_TYPE _eType)
{
    Vec2 vMissilePos = vMonPos;
    vMissilePos.y += 100.f;

    CScene* CurScene = CSceneMgr::GetInst()->GetCurScene();
    CObject* pInfo = CurScene->GetPlayer(0);
    Vec2 vpPos = Vec2(-2 * PI, -3 * PI / 2.f);


    for (int i = 0; i < 20; ++i)
    {

        vpPos.Normalize();

        // Missile Object
        CMissile* pMissile = new CMissile;
        pMissile->init(GROUP_TYPE::PROJ_MONSTER);
        pMissile->SetPos(vMissilePos);
        pMissile->SetScale(Vec2(20.f, 20.f));
        pMissile->SetDir(vpPos);
        pMissile->SetVec(400);
        pMissile->SetType(3);
        pMissile->SetName(L"MsMissile");
        pMissile->ChangemType(_eType);
        CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
        vpPos.x += 2 * PI / 18.f;
        if (i == 8)
        {
            vpPos.x = -2 * PI;
            vpPos.y *= -1;
        }
    }
}

void CreatePenMissile(Vec2 vMonPos, Vec2 vMonScale, float fXdir, MISSILE_TYPE _eType)
{
    Vec2 vMissilePos = vMonPos;
    vMissilePos.y += 50.f;

    CScene* CurScene = CSceneMgr::GetInst()->GetCurScene();
    CObject* pInfo = CurScene->GetPlayer(0);
    Vec2 vpPos = Vec2(fXdir, -3 * PI / 2.f);

    vpPos.Normalize();

    // Missile Object
    CMissile* pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(300);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
}

void CreateHurricaneMissile(Vec2 vMonPos, Vec2 vMonScale, float fXdir, float fYdir, MISSILE_TYPE _eType)
{
    Vec2 vMissilePos = vMonPos;
    Vec2 vpPos = Vec2(fXdir, fYdir);
    vpPos.Normalize();
    CMissile* pMissile = new CMissile;
    pMissile->init(GROUP_TYPE::PROJ_MONSTER);
    pMissile->SetPos(vMissilePos);
    pMissile->SetScale(Vec2(20.f, 20.f));
    pMissile->SetDir(vpPos);
    pMissile->SetVec(300);
    pMissile->SetType(1);
    pMissile->SetName(L"MsMissile");
    pMissile->ChangemType(_eType);
    CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
}
