#pragma once

#include <cstddef>
#include <cstdint>

using PlayerId = std::uint8_t;
constexpr std::size_t MAX_PLAYERS = 2;

#define SINGLE(type) public: static type* GetInst()\
						{\
							static type mgr;\
							return &mgr;\
						}\
						private:\
							type();\
							~type();

#define fDT CTimeMgr::GetInst()->GetfDT()
#define DT CTimeMgr::GetInst()->GetDT()

#define CLONE(type) type* Clone() {return new type(*this);}

#define KEY_CHECK(key,state) CKeyMgr::GetInst()->GetKeyState(key) == state
#define KEY_HOLD(key) KEY_CHECK(key,KEY_STATE::HOLD)
#define KEY_TAP(key) KEY_CHECK(key,KEY_STATE::TAP)
#define KEY_AWAY(key) KEY_CHECK(key,KEY_STATE::AWAY)
#define KEY_NONE(key) KEY_CHECK(key,KEY_STATE::NONE)
#define MOUSE_POS CKeyMgr::GetInst()->GetMousePos()

#define PI 3.1415926535f

#define TILE_SIZE 64

enum class GROUP_TYPE
{
	DEFAULT, //0
	BACKGROUND,
    TILE,
	PLAYER, // 1
	MONSTER, // 3
	PROJ_PLAYER,// 4
	PROJ_MONSTER,
    ITEM,
	BOSS,
    BACKBUFFER,
	DEAD_PLAYER,


	//UI는 가장 끝에
	UI = 31,
	END = 32,
};

enum class SCENE_TYPE
{
	TOOL,
	START,
    TITLE,
    MAINMENU,
    MANUAL,
    RANKING,
	STAGE_01,
	STAGE_02,
    CREDITS,
	END,
};

enum class BRUSH_TYPE
{
	HOLLOW,
    BLACK,
	END,
};

enum class PEN_TYPE
{
	RED,
	GREEN,
	BLUE,
	END,
};

enum class EVENT_TYPE
{
	CREATE_OBJECT,
	DELETE_OBJECT,
	SCENE_CHANGE,
	CHANGE_AI_STATE,



	END,
};

enum class MON_STATE
{
    IDLE,
    PATROL1,
    PATROL2,
    PATROL3,
    PATROL4,
    PATROL5,
    PATROL6,
    PATROL7,
    MBOSS0,
    MBOSS1,
    MBOSS2,
    MBOSS3,
    TRACE,
    ATT,
    RUN,
    DEAD,

};

enum class MISSILE_PTRN
{
    NONE,
    PTRN1,
    PTRN2,
    PTRN3,
    PTRN4,
    PTRN5,
    PTRN6,


    END,
};

enum class MISSILE_TYPE
{
    SMALL,
    SPIN,
    MIDDLE,
    BSMALL,
    BIG,
    RECTANGLE,
    LVL1,
    LVL2,
    LVL3,
    LVL4,
    LVL5,
    LVL6,
    END,
};

enum class MON_TYPE
{
    NORMAL1,
    NORMAL2,
    NORMAL3,
    NORMAL4,
    NORMAL5,
    NORMAL6,
    BOSS,
    EPIC,
    EPIC2,
};
