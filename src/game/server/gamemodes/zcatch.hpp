/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* zCatch by erd and Teetime */

#ifndef GAME_SERVER_GAMEMODES_ZCATCH_H
#define GAME_SERVER_GAMEMODES_ZCATCH_H

#include <game/server/gamecontroller.h>

class CGameController_zCatch : public IGameController
{
	public:
	CGameController_zCatch(class CGameContext *pGameServer);
	virtual void Tick();
	virtual bool IsZCatch();
	
	enum
	{
		ZCATCH_JOINED_NEW = -2,
		ZCATCH_NOT_CATCHED = -1,
	}; 

	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID);
	virtual void StartRound();
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual void EndRound();
};

#endif
