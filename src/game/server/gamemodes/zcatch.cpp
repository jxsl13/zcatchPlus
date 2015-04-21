/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* zCatch by erd and Teetime                                                                 */
/* Modified by Teelevision for zCatch/TeeVi, see readme.txt and license.txt.                 */

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include "zcatch.h"
#include <string.h>

CGameController_zCatch::CGameController_zCatch(class CGameContext *pGameServer) :
		IGameController(pGameServer)
{
	m_pGameType = "zCatch/TeeVi";
	m_OldMode = g_Config.m_SvMode;
}

CGameController_zCatch::~CGameController_zCatch() {
	/* wait for all threads */
	for (auto &thread: rankingThreads)
		thread.join();
}

/* ranking system: create zcatch score table */
void CGameController_zCatch::OnInitRanking(sqlite3 *rankingDb) {
	char *zErrMsg = 0;
	int rc = sqlite3_exec(GameServer()->GetRankingDb(), "\
			BEGIN; \
			CREATE TABLE IF NOT EXISTS zCatchScore( \
				username TEXT PRIMARY KEY, \
				score UNSIGNED INTEGER DEFAULT 0, \
				numWins UNSIGNED INTEGER DEFAULT 0, \
				numKills UNSIGNED INTEGER DEFAULT 0, \
				numDeaths UNSIGNED INTEGER DEFAULT 0, \
				numShots UNSIGNED INTEGER DEFAULT 0, \
				highestSpree UNSIGNED INTEGER DEFAULT 0, \
				timePlayed UNSIGNED INTEGER DEFAULT 0 \
			); \
			CREATE INDEX IF NOT EXISTS zCatchScore_score_index ON zCatchScore (score); \
			COMMIT; \
		", NULL, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s\n", rc, zErrMsg);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		sqlite3_free(zErrMsg);
		exit(1);
	}
}

void CGameController_zCatch::Tick()
{
	IGameController::Tick();

	if(m_OldMode != g_Config.m_SvMode)
	{
		Server()->MapReload();
		m_OldMode = g_Config.m_SvMode;
	}
	
	// save rankings every minute
	if (Server()->Tick() % (Server()->TickSpeed() * 60) == 0)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i])
			{
				SaveRanking(GameServer()->m_apPlayers[i]);
			}
		}
	}
	
}

void CGameController_zCatch::DoWincheck()
{
	if(m_GameOverTick == -1)
	{
		int Players = 0, Players_Spec = 0, Players_SpecExplicit = 0;
		int winnerId = -1;

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i])
			{
				Players++;
				if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
					Players_Spec++;
				else
					winnerId = i;
				if(GameServer()->m_apPlayers[i]->m_SpecExplicit)
					Players_SpecExplicit++;
			}
		}
		int Players_Ingame = Players - Players_SpecExplicit;

		if(Players_Ingame <= 1)
		{
			//Do nothing
		}
		else if((Players - Players_Spec) == 1)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				{
					GameServer()->m_apPlayers[i]->m_Score += g_Config.m_SvBonus;
					if(Players_Ingame < g_Config.m_SvLastStandingPlayers)
						GameServer()->m_apPlayers[i]->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				}
			}
			if(Players_Ingame < g_Config.m_SvLastStandingPlayers)
			{
				GameServer()->SendChatTarget(-1, "Too few players to end round. All players have been released.");
			}
			else
			{
				
				// give the winner points
				if (winnerId > -1)
				{
					RewardWinner(winnerId, Players_Ingame - 1);
				}
				
				EndRound();
			}
		}

		IGameController::DoWincheck(); //do also usual wincheck
	}
}

int CGameController_zCatch::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	if(!pKiller)
		return 0;

	CPlayer *victim = pVictim->GetPlayer();
	if(pKiller != victim)
	{
		/* count players playing */
		int numPlayers = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(GameServer()->m_apPlayers[i] && !GameServer()->m_apPlayers[i]->m_SpecExplicit)
				++numPlayers;
		/* you can at max get that many points as there are players playing */
		pKiller->m_Score += min(victim->m_zCatchNumKillsInARow + 1, numPlayers);
		++pKiller->m_Kills;
		++victim->m_Deaths;
		/* Check if the killer has been already killed and is in spectator (victim may died through wallshot) */
		if(pKiller->GetTeam() != TEAM_SPECTATORS && (!pVictim->m_KillerLastDieTickBeforceFiring || pVictim->m_KillerLastDieTickBeforceFiring == pKiller->m_DieTick))
		{
			++pKiller->m_zCatchNumKillsInARow;
			pKiller->AddZCatchVictim(victim->GetCID(), CPlayer::ZCATCH_CAUGHT_REASON_KILLED);
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "You are caught until '%s' dies.", Server()->ClientName(pKiller->GetCID()));
			GameServer()->SendChatTarget(victim->GetCID(), aBuf);
		}
	}
	else
	{
		// selfkill/death
		if(WeaponID == WEAPON_SELF || WeaponID == WEAPON_WORLD)
		{
			victim->m_Score -= g_Config.m_SvKillPenalty;
			++victim->m_Deaths;
		}
	}

	// release all the victim's victims
	victim->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
	victim->m_zCatchNumKillsInARow = 0;

	// Update colours
	OnPlayerInfoChange(victim);
	OnPlayerInfoChange(pKiller);
	
	// ranking
	++victim->m_RankCache.m_NumDeaths;
	++pKiller->m_RankCache.m_NumKills;

	return 0;
}

void CGameController_zCatch::OnPlayerInfoChange(class CPlayer *pP)
{
	if(g_Config.m_SvColorIndicator && pP->m_zCatchNumKillsInARow <= 20)
	{
		int Num = max(0, 160 - pP->m_zCatchNumKillsInARow * 10);
		pP->m_TeeInfos.m_ColorBody = Num * 0x010000 + 0xff00;
		pP->m_TeeInfos.m_ColorFeet = pP->m_zCatchNumKillsInARow == 20 ? 0x40ff00 : pP->m_TeeInfos.m_ColorBody;
		pP->m_TeeInfos.m_UseCustomColor = 1;
	}
}

void CGameController_zCatch::StartRound()
{
	IGameController::StartRound();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_Kills = 0;
			GameServer()->m_apPlayers[i]->m_Deaths = 0;
			GameServer()->m_apPlayers[i]->m_TicksSpec = 0;
			GameServer()->m_apPlayers[i]->m_TicksIngame = 0;
		}
	}
}

void CGameController_zCatch::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health and armor
	pChr->IncreaseHealth(10);
	if(g_Config.m_SvMode == 2)
		pChr->IncreaseArmor(10);

	// give default weapons
	switch(g_Config.m_SvMode)
	{
	case 1: /* Instagib - Only Riffle */
		pChr->GiveWeapon(WEAPON_RIFLE, -1);
		break;
	case 2: /* All Weapons */
		pChr->GiveWeapon(WEAPON_HAMMER, -1);
		pChr->GiveWeapon(WEAPON_GUN, g_Config.m_SvWeaponsAmmo);
		pChr->GiveWeapon(WEAPON_GRENADE, g_Config.m_SvWeaponsAmmo);
		pChr->GiveWeapon(WEAPON_SHOTGUN, g_Config.m_SvWeaponsAmmo);
		pChr->GiveWeapon(WEAPON_RIFLE, g_Config.m_SvWeaponsAmmo);
		break;
	case 3: /* Hammer */
		pChr->GiveWeapon(WEAPON_HAMMER, -1);
		break;
	case 4: /* Grenade */
		pChr->GiveWeapon(WEAPON_GRENADE, g_Config.m_SvGrenadeEndlessAmmo ? -1 : g_Config.m_SvWeaponsAmmo);
		break;
	case 5: /* Ninja */
		pChr->GiveNinja();
		break;
	}

	//Update colour of spawning tees
	OnPlayerInfoChange(pChr->GetPlayer());
}

void CGameController_zCatch::EndRound()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			
			// save ranking stats
			SaveRanking(GameServer()->m_apPlayers[i]);

			if(!GameServer()->m_apPlayers[i]->m_SpecExplicit)
			{
				GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));

				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Kills: %d | Deaths: %d", GameServer()->m_apPlayers[i]->m_Kills, GameServer()->m_apPlayers[i]->m_Deaths);
				GameServer()->SendChatTarget(i, aBuf);

				if(GameServer()->m_apPlayers[i]->m_TicksSpec != 0 || GameServer()->m_apPlayers[i]->m_TicksIngame != 0)
				{
					double TimeInSpec = (GameServer()->m_apPlayers[i]->m_TicksSpec * 100.0) / (GameServer()->m_apPlayers[i]->m_TicksIngame + GameServer()->m_apPlayers[i]->m_TicksSpec);
					str_format(aBuf, sizeof(aBuf), "Spec: %.2f%% | Ingame: %.2f%%", (double) TimeInSpec, (double) (100.0 - TimeInSpec));
					GameServer()->SendChatTarget(i, aBuf);
				}
				// release all players
				GameServer()->m_apPlayers[i]->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				GameServer()->m_apPlayers[i]->m_zCatchNumKillsInARow = 0;
			}
		}
	}

	if(m_Warmup) // game can't end when we are running warmup
		return;

	GameServer()->m_World.m_Paused = true;
	m_GameOverTick = Server()->Tick();
	m_SuddenDeath = 0;
}

bool CGameController_zCatch::CanChangeTeam(CPlayer *pPlayer, int JoinTeam)
{
	if(pPlayer->m_CaughtBy >= 0)
		return false;
	return true;
}

bool CGameController_zCatch::OnEntity(int Index, vec2 Pos)
{
	if(Index == ENTITY_SPAWN)
		m_aaSpawnPoints[0][m_aNumSpawnPoints[0]++] = Pos;
	else if(Index == ENTITY_SPAWN_RED)
		m_aaSpawnPoints[1][m_aNumSpawnPoints[1]++] = Pos;
	else if(Index == ENTITY_SPAWN_BLUE)
		m_aaSpawnPoints[2][m_aNumSpawnPoints[2]++] = Pos;

	return false;
}

/* celebration and scoring */
void CGameController_zCatch::RewardWinner(int winnerId, int numEnemies) {
	
	CPlayer *winner = GameServer()->m_apPlayers[winnerId];
	
	/* calculate points (multiplied with 100) */
	int points = 100 * numEnemies * numEnemies * numEnemies / 225;
	
	/* set winner's ranking stats */
	winner->m_RankCache.m_Points += points;
	++winner->m_RankCache.m_NumWins;
	/* saving is done in EndRound() */
	
	/* abort if no points */
	if (points == 0)
	{
		return;
	}

	/* the winner's name */
	const char *name = GameServer()->Server()->ClientName(winnerId);
	
	/* announce in chat */
	char aBuf[96];
	str_format(aBuf, sizeof(aBuf), "Winner '%s' gets %.2f points.", name, points/100.0);
	GameServer()->SendChatTarget(-1, aBuf);
	
}

/* save a player's ranking stats */
void CGameController_zCatch::SaveRanking(CPlayer *player) {
	
	/* prepare */
	player->RankCacheStopPlaying(); // so that m_RankCache.m_TimePlayed is updated
	
	/* give the points */
	rankingThreads.push_back(std::thread(&CGameController_zCatch::SaveScore, this,
		GameServer()->Server()->ClientName(player->GetCID()), // username
		player->m_RankCache.m_Points, // score
		player->m_RankCache.m_NumWins, // numWins
		player->m_RankCache.m_NumKills, // numKills
		player->m_RankCache.m_NumDeaths, // numDeaths
		player->m_RankCache.m_NumShots, // numShots
		player->m_zCatchNumKillsInARow, // highestSpree
		player->m_RankCache.m_TimePlayed / Server()->TickSpeed() // timePlayed
	));
	
	/* clean rank cache */
	player->m_RankCache.m_Points = 0;
	player->m_RankCache.m_NumWins = 0;
	player->m_RankCache.m_NumKills = 0;
	player->m_RankCache.m_NumDeaths = 0;
	player->m_RankCache.m_NumShots = 0;
	player->m_RankCache.m_TimePlayed = 0;
	player->RankCacheStartPlaying();
	
}

/* adds the score to the player */
void CGameController_zCatch::SaveScore(const char *name, int score, int numWins, int numKills, int numDeaths, int numShots, int highestSpree, int timePlayed) {

	/* prepare */
	const char *zTail;
	const char *zSql = "\
		INSERT OR REPLACE INTO zCatchScore ( \
			username, \
			score, \
			numWins, \
			numKills, \
			numDeaths, \
			numShots, \
			highestSpree, \
			timePlayed \
		) VALUES ( \
			?1, \
			COALESCE((SELECT score FROM zCatchScore WHERE username = ?1), 0) + ?2, \
			COALESCE((SELECT numWins FROM zCatchScore WHERE username = ?1), 0) + ?3, \
			COALESCE((SELECT numKills FROM zCatchScore WHERE username = ?1), 0) + ?4, \
			COALESCE((SELECT numDeaths FROM zCatchScore WHERE username = ?1), 0) + ?5, \
			COALESCE((SELECT numShots FROM zCatchScore WHERE username = ?1), 0) + ?6, \
			MAX(COALESCE((SELECT highestSpree FROM zCatchScore WHERE username = ?1), 0), ?7), \
			COALESCE((SELECT timePlayed FROM zCatchScore WHERE username = ?1), 0) + ?8 \
		);";
	sqlite3_stmt *pStmt;
	int rc = sqlite3_prepare_v2(GameServer()->GetRankingDb(), zSql, strlen(zSql), &pStmt, &zTail);
	
	if (rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite3_bind_text(pStmt, 1, name, strlen(name), 0);
		sqlite3_bind_int(pStmt, 2, score);
		sqlite3_bind_int(pStmt, 3, numWins);
		sqlite3_bind_int(pStmt, 4, numKills);
		sqlite3_bind_int(pStmt, 5, numDeaths);
		sqlite3_bind_int(pStmt, 6, numShots);
		sqlite3_bind_int(pStmt, 7, highestSpree);
		sqlite3_bind_int(pStmt, 8, timePlayed);
		
		/* save to database */
		sqlite3_step(pStmt);
		sqlite3_finalize(pStmt);
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer()->GetRankingDb()));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
	}
	
}

/* when a player typed /top into the chat */
void CGameController_zCatch::OnChatCommandTop(CPlayer *pPlayer)
{
	rankingThreads.push_back(std::thread(&CGameController_zCatch::ChatCommandTopFetchDataAndPrint, this, pPlayer->GetCID()));
}

/* get the top players */
void CGameController_zCatch::ChatCommandTopFetchDataAndPrint(int clientId)
{
	
	/* prepare */
	const char *zTail;
	const char *zSql = "SELECT username, score FROM zCatchScore ORDER BY score DESC LIMIT 5;";
	sqlite3_stmt *pStmt;
	int rc = sqlite3_prepare_v2(GameServer()->GetRankingDb(), zSql, strlen(zSql), &pStmt, &zTail);
	
	if (rc == SQLITE_OK)
	{
		/* fetch from database */
		int numRows = 0;
		while (sqlite3_step(pStmt) == SQLITE_ROW)
		{
			const unsigned char* name = sqlite3_column_text(pStmt, 0);
			int score = sqlite3_column_int(pStmt, 1);
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "[%.*f] %s", score % 100 ? 2 : 0, score/100.0, name);
			/* if the player left and the client id is unused, nothing will happen */
			/* if another player joined, there is no big harm that he receives it */
			/* maybe later i have a good idea how to prevent this */
			GameServer()->SendChatTarget(clientId, aBuf);
			++numRows;
		}
		
		if (numRows == 0)
		{
			GameServer()->SendChatTarget(clientId, "There are no ranks");
		}
		
		sqlite3_finalize(pStmt);
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer()->GetRankingDb()));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
	}
}

/* when a player typed /top into the chat */
void CGameController_zCatch::OnChatCommandOwnRank(CPlayer *pPlayer)
{
	OnChatCommandRank(pPlayer, GameServer()->Server()->ClientName(pPlayer->GetCID()));
}

/* when a player typed /top into the chat */
void CGameController_zCatch::OnChatCommandRank(CPlayer *pPlayer, const char *name)
{
	rankingThreads.push_back(std::thread(&CGameController_zCatch::ChatCommandRankFetchDataAndPrint, this, pPlayer->GetCID(), name));
}

/* get the top players */
void CGameController_zCatch::ChatCommandRankFetchDataAndPrint(int clientId, const char *name)
{
	
	/* prepare */
	const char *zTail;
	const char *zSql = "\
		SELECT \
			a.score, \
			a.numWins, \
			a.numKills, \
			a.numDeaths, \
			a.numShots, \
			a.highestSpree, \
			a.timePlayed, \
			(SELECT COUNT(*) FROM zCatchScore b WHERE b.score > a.score) + 1, \
			MAX(0, (SELECT MIN(b.score) FROM zCatchScore b WHERE b.score > a.score) - a.score) \
		FROM zCatchScore a \
		WHERE username = ?1\
		;";
	sqlite3_stmt *pStmt;
	int rc = sqlite3_prepare_v2(GameServer()->GetRankingDb(), zSql, strlen(zSql), &pStmt, &zTail);
	
	if (rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite3_bind_text(pStmt, 1, name, strlen(name), 0);
		
		/* fetch from database */
		int row = sqlite3_step(pStmt);
		if (row == SQLITE_ROW)
		{
		
			int score = sqlite3_column_int(pStmt, 0);
			int numWins = sqlite3_column_int(pStmt, 1);
			int numKills = sqlite3_column_int(pStmt, 2);
			int numDeaths = sqlite3_column_int(pStmt, 3);
			int numShots = sqlite3_column_int(pStmt, 4);
			int highestSpree = sqlite3_column_int(pStmt, 5);
			int timePlayed = sqlite3_column_int(pStmt, 6);
			int rank = sqlite3_column_int(pStmt, 7);
			int scoreToNextRank = sqlite3_column_int(pStmt, 8);
			
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "'%s' is rank %d with a score of %.*f points (%d wins, %d kills, %d deaths, %d shots, %d spree, %d:%02dh played, %.*f points for next rank)", name, rank, score % 100 ? 2 : 0, score/100.0, numWins, numKills, numDeaths, numShots, highestSpree, timePlayed / 3600, timePlayed / 60 % 60, scoreToNextRank % 100 ? 2 : 0, scoreToNextRank/100.0);
			GameServer()->SendChatTarget(clientId, aBuf);
		}
		else if (row == SQLITE_DONE)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "'%s' has no rank", name);
			GameServer()->SendChatTarget(clientId, aBuf);
		}
		
		sqlite3_finalize(pStmt);
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer()->GetRankingDb()));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
	}
}
