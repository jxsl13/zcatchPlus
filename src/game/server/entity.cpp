/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entity.h"
#include "gamecontext.h"
// jxsl13 added
#include <engine/shared/config.h>

//////////////////////////////////////////////////
// Entity
//////////////////////////////////////////////////
CEntity::CEntity(CGameWorld *pGameWorld, int ObjType)
{
	m_pGameWorld = pGameWorld;

	m_ObjType = ObjType;
	m_Pos = vec2(0, 0);
	m_ProximityRadius = 0;

	m_MarkedForDestroy = false;
	m_ID = Server()->SnapNewID();

	m_pPrevTypeEntity = 0;
	m_pNextTypeEntity = 0;
}

CEntity::~CEntity()
{
	GameWorld()->RemoveEntity(this);
	Server()->SnapFreeID(m_ID);
}

/**
	Calls below function using my position as vector (0,0)
*/
int CEntity::NetworkClipped(int SnappingClient)
{
	return NetworkClipped(SnappingClient, m_Pos);
}

/**	
// jxsl13 was here.
This function sends player information from the server to the client based on the distance of
two entities. Based on the server config file, dyn cam clipping is either enabled or disabled.
*/
int CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos)
{
	if (SnappingClient == -1)
		return 0;

	float dx = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.x - CheckPos.x;
	float dy = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.y - CheckPos.y;

	if (g_Config.m_SvAllowDynamicCam == 1) {
		if (absolute(dx) > 1000.0f || absolute(dy) > 800.0f)
			return 1;

		if (distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, CheckPos) > 1100.0f)
			return 1;
	} else if (g_Config.m_SvAllowDynamicCam == 0) {
		// Tested on a 16:9 aspect ratio screen.
		if (absolute(dx) > (float)g_Config.m_SvStaticCamAbsolutexDistanceX || absolute(dy) > (float)g_Config.m_SvStaticCamAbsolutexDistanceY) {
			return 1;
		}
	}


	return 0;
}

bool CEntity::GameLayerClipped(vec2 CheckPos)
{
	return round_to_int(CheckPos.x) / 32 < -200 || round_to_int(CheckPos.x) / 32 > GameServer()->Collision()->GetWidth() + 200 ||
	       round_to_int(CheckPos.y) / 32 < -200 || round_to_int(CheckPos.y) / 32 > GameServer()->Collision()->GetHeight() + 200 ? true : false;
}

void CEntity::InitAffectedCharacters()
{
	CCharacter *c;
	for (int i = 0; i < MAX_CLIENTS; ++i)
		if ((c = GameServer()->GetPlayerChar(i)))
			m_AffectedCharacters[i] = (absolute(c->m_Pos.x - m_Pos.x) <= 900.0f && absolute(c->m_Pos.y - m_Pos.y) <= 700.0f && distance(c->m_Pos, m_Pos) <= 900.0f);
	m_AffectedCharactersInitialized = true;
}
