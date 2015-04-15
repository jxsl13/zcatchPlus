/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* zCatch by erd and Teetime                                                                 */

#ifndef GAME_SERVER_GAMEMODES_ZCATCH_H
#define GAME_SERVER_GAMEMODES_ZCATCH_H

#include <game/server/gamecontroller.h>
#include <thread>
#include <vector>

class CGameController_zCatch: public IGameController
{
	int m_OldMode;
	
	void RewardWinner(int winnerId, int numEnemies);
	
	/* ranking system */
	struct ChatCommandTopContainer {
		CGameContext *gameServer;
		int clientId;
	};
	static int ChatCommandTopPrint(void *data, int argc, char **argv, char **azColName);
	std::vector<std::thread> saveScoreThreads;
	void SaveScore(const char *name, int score);

public:
	CGameController_zCatch(class CGameContext *pGameServer);
	~CGameController_zCatch();
	virtual void Tick();
	virtual void DoWincheck();

	virtual void StartRound();
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual void OnPlayerInfoChange(class CPlayer *pP);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID);
	virtual bool OnEntity(int Index, vec2 Pos);
	virtual bool CanChangeTeam(CPlayer *pPlayer, int JoinTeam);
	virtual void EndRound();
	
	/* ranking system */
	virtual void OnChatCommandTop(CPlayer *pPlayer);
};

#endif
