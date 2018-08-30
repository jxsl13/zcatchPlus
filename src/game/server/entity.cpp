/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include "entity.h"
#include "gamecontext.h"
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
		return 0; // entity is in the snapshot

	// needed for default dyn cam behaviour and experimental
	float viewPosX = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.x;
	float viewPosY = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.y;

	float viewDistanceX = absolute(viewPosX - CheckPos.x);
	float viewDistanceY = absolute(viewPosY - CheckPos.y);
	// default and experimental dyn cam behaviour


	// default dyn cam behaviour uses viewpos, what seems not to be the player position!
	if (g_Config.m_SvAllowDefaultDynamicCam == 1) {
		if (viewDistanceX > 1000.0f || viewDistanceY > 800.0f)
			return 1; // not in snap

		if (distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, CheckPos) > 1100.0f)
			return 1; // not in snap

	} else if (g_Config.m_SvAllowDefaultDynamicCam == 0) { // experimental dyn cam behaviour
		float playerPosX = GameServer()->m_apPlayers[SnappingClient]->GetCurrentPlayerPositionX();
		float playerPosY = GameServer()->m_apPlayers[SnappingClient]->GetCurrentPlayerPositionY();

		float playerDistanceX = absolute(playerPosX - CheckPos.x);
		float playerDistanceY = absolute(playerPosY - CheckPos.y);

		float cursorPosX = GameServer()->m_apPlayers[SnappingClient]->GetCurrentCursorPositionX();
		float cursorPosY = GameServer()->m_apPlayers[SnappingClient]->GetCurrentCursorPositionY();

		double playerCursorDistance = sqrt(pow(cursorPosX, 2) + pow(cursorPosY, 2));


		// Tested on a 16:9 aspect ratio screen.
		// if not in bounding box around player and not in bounding box around cursor(max cursor distance is 635).
		if (playerCursorDistance < 300.0) {
			// in this area the dynamic cam stays as static, thus only check the static bounding box
			if ((playerDistanceX > (float)g_Config.m_SvStaticCamAbsolutexDistanceX || playerDistanceY > (float)g_Config.m_SvStaticCamAbsolutexDistanceY) )
			{
				return 1;
			}

		} else { // >= 300.0 -> camera follows mouse movement if dyn cam.

			// these calculations are only needed in here.
			// normalize player - cursor distance to 635 if the distance is bigger than 635
			double normalizationFactor = playerCursorDistance > 635.0 ? 635.0 / playerCursorDistance : 1.0;
			// now make it so that the max possible distance can be 635 from the tee body.
			float normalCursorPosX = (float)(cursorPosX * normalizationFactor);
			float normalCursorPosY = (float)(cursorPosY * normalizationFactor);

			// half way between cursor and player position
			float halfWayPosX = playerPosX + (normalCursorPosX / 2.0);
			float halfWayPosY = playerPosY + (normalCursorPosY / 2.0);

			float halfWayDistanceX = absolute(halfWayPosX - CheckPos.x);
			float halfWayDistanceY = absolute(halfWayPosY - CheckPos.y);

			if ( (halfWayDistanceX > g_Config.m_SvStaticCamAbsolutexDistanceX || halfWayDistanceY > g_Config.m_SvStaticCamAbsolutexDistanceY )
			    && (playerDistanceX > g_Config.m_SvStaticCamAbsolutexDistanceX || playerDistanceY > g_Config.m_SvStaticCamAbsolutexDistanceY ) )
			{

				// if the distance from the middle of the aimline between player and cursor
				// to another entity is not in the static bounding box
				// and not within a half-sized bounding box around the player
				return 1; // entity is not in the visible snapshot.
			}
		}
	}


	return 0; // entity is in the snapshot
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
