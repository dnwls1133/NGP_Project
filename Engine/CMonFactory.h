#pragma once

class CMonster;


class CMonFactory
{
public:
    static CMonster* CreateMonster(MON_TYPE _eType, MISSILE_PTRN _eMType,Vec2 _vPos,Vec2 _vDPos,Vec2 _vEPos);


private:
    CMonFactory();
    ~CMonFactory();
};

