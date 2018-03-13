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
	m_pGameType = "zCatch+";
	m_OldMode = g_Config.m_SvMode;

	// jxsl13 added to save old server config. Needed for last man
	// standing deathmatch feature to reset to previous state 
	// if treshold of players is reached.
	m_OldAllowJoin = g_Config.m_SvAllowJoin;
}

CGameController_zCatch::~CGameController_zCatch() {
	/* save all players */
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (GameServer()->m_apPlayers[i])
			SaveRanking(GameServer()->m_apPlayers[i]);
}

/* ranking system: create zcatch score table */
void CGameController_zCatch::OnInitRanking(sqlite3 *rankingDb) {
	char *zErrMsg = 0;

	/* lock database access in this process */
	GameServer()->LockRankingDb();

	/* when another process uses the database, wait up to 10 seconds */
	sqlite3_busy_timeout(GameServer()->GetRankingDb(), 10000);

	int rc = sqlite3_exec(GameServer()->GetRankingDb(), "\
			BEGIN; \
			CREATE TABLE IF NOT EXISTS zCatch( \
				username TEXT PRIMARY KEY, \
				score UNSIGNED INTEGER DEFAULT 0, \
				numWins UNSIGNED INTEGER DEFAULT 0, \
				numKills UNSIGNED INTEGER DEFAULT 0, \
				numKillsWallshot UNSIGNED INTEGER DEFAULT 0, \
				numDeaths UNSIGNED INTEGER DEFAULT 0, \
				numShots UNSIGNED INTEGER DEFAULT 0, \
				highestSpree UNSIGNED INTEGER DEFAULT 0, \
				timePlayed UNSIGNED INTEGER DEFAULT 0 \
			); \
			CREATE INDEX IF NOT EXISTS zCatch_score_index ON zCatch (score); \
			CREATE INDEX IF NOT EXISTS zCatch_numWins_index ON zCatch (numWins); \
			CREATE INDEX IF NOT EXISTS zCatch_numKills_index ON zCatch (numKills); \
			CREATE INDEX IF NOT EXISTS zCatch_numKillsWallshot_index ON zCatch (numKillsWallshot); \
			CREATE INDEX IF NOT EXISTS zCatch_numDeaths_index ON zCatch (numDeaths); \
			CREATE INDEX IF NOT EXISTS zCatch_numShots_index ON zCatch (numShots); \
			CREATE INDEX IF NOT EXISTS zCatch_highestSpree_index ON zCatch (highestSpree); \
			CREATE INDEX IF NOT EXISTS zCatch_timePlayed_index ON zCatch (timePlayed); \
			COMMIT; \
		", NULL, 0, &zErrMsg);

	/* unlock database access */
	GameServer()->UnlockRankingDb();

	/* check for error */
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

	if (m_OldMode != g_Config.m_SvMode && !GameServer()->m_World.m_Paused)
	{
		EndRound();
	}

}

// Also checks if last standing player deathmatch is played before player treshhold is reached
void CGameController_zCatch::DoWincheck()
{
	if (m_GameOverTick == -1)
	{
		int Players = 0, Players_Spec = 0, Players_SpecExplicit = 0;
		int winnerId = -1;
		CPlayer *winner = NULL;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (GameServer()->m_apPlayers[i])
			{
				Players++;
				if (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
					Players_Spec++;
				else
				{
					winnerId = i;
					winner = GameServer()->m_apPlayers[i];
				}
				if (GameServer()->m_apPlayers[i]->m_SpecExplicit)
					Players_SpecExplicit++;
			}
		}
		int Players_Ingame = Players - Players_SpecExplicit;

		if (Players_Ingame <= 1)
		{
			//Do nothing
		}
		else if ((Players - Players_Spec) == 1)
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				{
					GameServer()->m_apPlayers[i]->m_Score += g_Config.m_SvBonus;
					if (Players_Ingame < g_Config.m_SvLastStandingPlayers)
						GameServer()->m_apPlayers[i]->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				}
			}
			if (winner && winner->m_HardMode.m_Active && winner->m_HardMode.m_ModeTotalFails.m_Active && winner->m_HardMode.m_ModeTotalFails.m_Fails > winner->m_HardMode.m_ModeTotalFails.m_Max)
			{
				winner->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				winner->m_HardMode.m_ModeTotalFails.m_Fails = 0;
				GameServer()->SendChatTarget(-1, "The winner failed the hard mode.");
				GameServer()->SendBroadcast("The winner failed the hard mode.", -1);
			}
			else if (Players_Ingame < g_Config.m_SvLastStandingPlayers)
			{
				winner->HardModeRestart();
				GameServer()->SendChatTarget(-1, "Too few players to end round.");
				GameServer()->SendBroadcast("Too few players to end round.", -1);
			}
			else
			{

				// give the winner points
				if (winnerId > -1)
				{
					RewardWinner(winnerId);
				}

				// announce if winner is in hard mode
				if (winner->m_HardMode.m_Active) {
					char aBuf[256];
					auto name = GameServer()->Server()->ClientName(winnerId);
					str_format(aBuf, sizeof(aBuf), "Player '%s' won in hard mode (%s).", name, winner->m_HardMode.m_Description);
					GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
				}

				EndRound();
			}
			// checks release game stuff before the last man standing treshold is reached.
		} else {
			ToggleLastStandingDeathmatchAndRelease(Players_Ingame);
		}

		IGameController::DoWincheck(); //do also usual wincheck
	}
}

int CGameController_zCatch::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	if (!pKiller)
		return 0;

	CPlayer *victim = pVictim->GetPlayer();

	if (pKiller != victim)
	{
		/* count players playing */
		int numPlayers = 0;
		for (int i = 0; i < MAX_CLIENTS; i++)
			if (GameServer()->m_apPlayers[i] && !GameServer()->m_apPlayers[i]->m_SpecExplicit)
				++numPlayers;


		/* you can at max get that many points as there are players playing */
		pKiller->m_Score += min(victim->m_zCatchNumKillsInARow + 1, numPlayers);
		++pKiller->m_Kills;
		++victim->m_Deaths;

		/* Check if the killer has been already killed and is in spectator (victim may died through wallshot) */
		if (pKiller->GetTeam() != TEAM_SPECTATORS && (!pVictim->m_KillerLastDieTickBeforceFiring || pVictim->m_KillerLastDieTickBeforceFiring == pKiller->m_DieTick))
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
		if (WeaponID == WEAPON_SELF || WeaponID == WEAPON_WORLD)
		{
			victim->m_Score -= g_Config.m_SvKillPenalty;
			++victim->m_Deaths;
		}
	}

	// release all the victim's victims
	victim->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
	victim->m_zCatchNumKillsInARow = 0;
	victim->m_zCatchNumKillsReleased = 0;

	// Update colours
	OnPlayerInfoChange(victim);
	OnPlayerInfoChange(pKiller);

	// ranking
	++victim->m_RankCache.m_NumDeaths;
	if (pKiller != victim && WeaponID != WEAPON_GAME)
	{
		++pKiller->m_RankCache.m_NumKills;
		if (WeaponID == WEAPON_RIFLE && pVictim->m_TookBouncedWallshotDamage)
		{
			++pKiller->m_RankCache.m_NumKillsWallshot;
		}
	}

	// zCatch/TeeVi: hard mode
	if (pKiller->m_HardMode.m_Active && pKiller->m_HardMode.m_ModeKillTimelimit.m_Active)
	{
		pKiller->m_HardMode.m_ModeKillTimelimit.m_LastKillTick = Server()->Tick();
	}

	return 0;
}

void CGameController_zCatch::OnPlayerInfoChange(class CPlayer *pP)
{
	if (g_Config.m_SvColorIndicator && pP->m_zCatchNumKillsInARow <= 20)
	{
		int Num = max(0, 160 - pP->m_zCatchNumKillsInARow * 10);
		pP->m_TeeInfos.m_ColorBody = Num * 0x010000 + 0xff00;
		if (pP->m_HardMode.m_Active)
			pP->m_TeeInfos.m_ColorFeet = 0xffff00; // red
		else
			pP->m_TeeInfos.m_ColorFeet = pP->m_zCatchNumKillsInARow == 20 ? 0x40ff00 : pP->m_TeeInfos.m_ColorBody;
		pP->m_TeeInfos.m_UseCustomColor = 1;
	}
}

void CGameController_zCatch::StartRound()
{

	// if sv_mode changed: restart map (with new mode then)
	if (m_OldMode != g_Config.m_SvMode)
	{
		m_OldMode = g_Config.m_SvMode;
		Server()->MapReload();
	}

	IGameController::StartRound();

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_Kills = 0;
			GameServer()->m_apPlayers[i]->m_Deaths = 0;
			GameServer()->m_apPlayers[i]->m_TicksSpec = 0;
			GameServer()->m_apPlayers[i]->m_TicksIngame = 0;
			GameServer()->m_apPlayers[i]->RankCacheStartPlaying();
		}
	}
}

void CGameController_zCatch::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health and armor
	pChr->IncreaseHealth(10);
	if (g_Config.m_SvMode == 2)
		pChr->IncreaseArmor(10);

	// give default weapons
	switch (g_Config.m_SvMode)
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
	if (m_Warmup) // game can't end when we are running warmup
		return;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		auto player = GameServer()->m_apPlayers[i];
		if (player)
		{

			// save ranking stats
			SaveRanking(player);
			player->RankCacheStopPlaying();

			if (!player->m_SpecExplicit)
			{
				player->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));

				if (player->m_TicksSpec != 0 || player->m_TicksIngame != 0)
				{
					char aBuf[128];
					double TimeIngame = (player->m_TicksIngame * 100.0) / (player->m_TicksIngame + player->m_TicksSpec);
					str_format(aBuf, sizeof(aBuf), "K/D: %d/%d, ingame: %.2f%%", player->m_Kills, player->m_Deaths, TimeIngame);
					GameServer()->SendChatTarget(i, aBuf);
				}
				// release all players
				player->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				player->m_zCatchNumKillsInARow = 0;
			}

			// zCatch/TeeVi: hard mode
			// reset hard mode
			player->ResetHardMode();

		}
	}

	GameServer()->m_World.m_Paused = true;
	m_GameOverTick = Server()->Tick();
	m_SuddenDeath = 0;
}

bool CGameController_zCatch::CanChangeTeam(CPlayer *pPlayer, int JoinTeam)
{
	if (pPlayer->m_CaughtBy >= 0)
		return false;
	return true;
}

bool CGameController_zCatch::OnEntity(int Index, vec2 Pos)
{
	if (Index == ENTITY_SPAWN)
		m_aaSpawnPoints[0][m_aNumSpawnPoints[0]++] = Pos;
	else if (Index == ENTITY_SPAWN_RED)
		m_aaSpawnPoints[1][m_aNumSpawnPoints[1]++] = Pos;
	else if (Index == ENTITY_SPAWN_BLUE)
		m_aaSpawnPoints[2][m_aNumSpawnPoints[2]++] = Pos;

	return false;
}

/* celebration and scoring */
void CGameController_zCatch::RewardWinner(int winnerId) {

	CPlayer *winner = GameServer()->m_apPlayers[winnerId];
	int numEnemies = min(15, winner->m_zCatchNumKillsInARow - winner->m_zCatchNumKillsReleased);

	/* calculate points (multiplied with 100) */
	int points = 100 * numEnemies * numEnemies * numEnemies / 225;

	/* set winner's ranking stats */
	++winner->m_RankCache.m_NumWins;

	/* abort if no points */
	if (points <= 0)
	{
		return;
	}
	winner->m_RankCache.m_Points += points;

	/* saving is done in EndRound() */

	/* the winner's name */
	const char *name = GameServer()->Server()->ClientName(winnerId);

	/* announce in chat */
	char aBuf[96];
	str_format(aBuf, sizeof(aBuf), "Winner '%s' gets %.2f points.", name, points / 100.0);
	GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

}

/* save a player's ranking stats */
void CGameController_zCatch::SaveRanking(CPlayer *player)
{

	if (!GameServer()->RankingEnabled())
		return;

	/* prepare */
	player->RankCacheStopPlaying(); // so that m_RankCache.m_TimePlayed is updated

	/* check if saving is needed */
	/* because compared to simply the & operator, the && operator does not conpinue to check all the conditions
	of the if statement if one of them does not meet the criteria, so the order of the conditions decides how fast
	those are checked
	I wonder if the numKillsWallshot is actually needed due to every wallshot being a shot, meaning that the wallshot
	doesn't need to be checked*/
	if (player->m_RankCache.m_NumShots == 0 &&
	        player->m_RankCache.m_TimePlayed == 0 &&
	        player->m_RankCache.m_NumKills == 0 &&
	        player->m_RankCache.m_NumDeaths == 0 &&
	        player->m_zCatchNumKillsInARow == 0 &&
	        player->m_RankCache.m_NumWins == 0 &&
	        player->m_RankCache.m_Points == 0 &&
	        player->m_RankCache.m_NumKillsWallshot == 0)
		return;

	/* player's name */
	char *name = (char*)malloc(MAX_NAME_LENGTH);
	str_copy(name, GameServer()->Server()->ClientName(player->GetCID()), MAX_NAME_LENGTH);

	/* give the points */
	std::thread *t = new std::thread(&CGameController_zCatch::SaveScore,
	                                 GameServer(), // gamecontext
	                                 name, // username
	                                 player->m_RankCache.m_Points, // score
	                                 player->m_RankCache.m_NumWins, // numWins
	                                 player->m_RankCache.m_NumKills, // numKills
	                                 player->m_RankCache.m_NumKillsWallshot, // numKillsWallshot
	                                 player->m_RankCache.m_NumDeaths, // numDeaths
	                                 player->m_RankCache.m_NumShots, // numShots
	                                 player->m_zCatchNumKillsInARow, // highestSpree
	                                 player->m_RankCache.m_TimePlayed / Server()->TickSpeed() // timePlayed
	                                );
	t->detach();
	delete t;

	/* clean rank cache */
	player->m_RankCache.m_Points = 0;
	player->m_RankCache.m_NumWins = 0;
	player->m_RankCache.m_NumKills = 0;
	player->m_RankCache.m_NumKillsWallshot = 0;
	player->m_RankCache.m_NumDeaths = 0;
	player->m_RankCache.m_NumShots = 0;
	player->m_RankCache.m_TimePlayed = 0;
	player->RankCacheStartPlaying();
}

/* adds the score to the player */
/**
 * @brief Saves given score for given unique name to the database.
 * @details Saves given score for given unique name to the database.
 *
 * @param GameServer CGameContext Object needed for most game related information.
 * @param name This is the unique primary key which identifies a ranking record. 
 * 			   Because this function is executed within a thread, the free(name) is used at the end of this function.
 * @param score Score which is calculated with a specific and deterministic algorithm.
 * @param numWins Number of wins
 * @param numKills Number of kills
 * @param numKillsWallshot Number of wallshot kills
 * @param numDeaths Number of deaths
 * @param numShots Number of shots
 * @param highestSpree highest continuous killing spree.
 * @param timePlayed Time played on this server.
 */
void CGameController_zCatch::SaveScore(CGameContext* GameServer, char *name, int score, int numWins, int numKills, int numKillsWallshot, int numDeaths, int numShots, int highestSpree, int timePlayed) {

	/* debug */
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Saving user stats of '%s'", name);
	GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);

	/* prepare */
	const char *zTail;
	const char *zSql = "\
		INSERT OR REPLACE INTO zCatch ( \
			username, score, numWins, numKills, numKillsWallshot, numDeaths, numShots, highestSpree, timePlayed \
		) \
		SELECT \
			new.username, \
			COALESCE(old.score, 0) + ?2, \
			COALESCE(old.numWins, 0) + ?3, \
			COALESCE(old.numKills, 0) + ?4, \
			COALESCE(old.numKillsWallshot, 0) + ?5, \
			COALESCE(old.numDeaths, 0) + ?6, \
			COALESCE(old.numShots, 0) + ?7, \
			MAX(COALESCE(old.highestSpree, 0), ?8), \
			COALESCE(old.timePlayed, 0) + ?9 \
		FROM ( \
			SELECT trim(?1) as username \
		) new \
		LEFT JOIN ( \
			SELECT * \
			FROM zCatch \
		) old ON old.username = new.username; \
		";
	sqlite3_stmt *pStmt;
	int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), zSql, strlen(zSql), &pStmt, &zTail);

	if (rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite3_bind_text(pStmt, 1, name, strlen(name), 0);
		sqlite3_bind_int(pStmt, 2, score);
		sqlite3_bind_int(pStmt, 3, numWins);
		sqlite3_bind_int(pStmt, 4, numKills);
		sqlite3_bind_int(pStmt, 5, numKillsWallshot);
		sqlite3_bind_int(pStmt, 6, numDeaths);
		sqlite3_bind_int(pStmt, 7, numShots);
		sqlite3_bind_int(pStmt, 8, highestSpree);
		sqlite3_bind_int(pStmt, 9, timePlayed);

		/* lock database access in this process */
		GameServer->LockRankingDb();

		/* when another process uses the database, wait up to 1 minute */
		sqlite3_busy_timeout(GameServer->GetRankingDb(), 60000);

		/* save to database */
		switch (sqlite3_step(pStmt))
		{
		case SQLITE_DONE:
			/* nothing */
			break;
		case SQLITE_BUSY:
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", "Error: could not save records (timeout).");
			break;
		default:
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		}

		/* unlock database access */
		GameServer->UnlockRankingDb();
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
		GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
	}

	sqlite3_finalize(pStmt);
	free(name);
}

/* when a player typed /top into the chat */
void CGameController_zCatch::OnChatCommandTop(CPlayer *pPlayer, const char *category)
{
	const char *column;

	if (!str_comp_nocase("score", category) || !str_comp_nocase("", category))
	{
		column = "score";
	}
	else if (!str_comp_nocase("wins", category))
	{
		column = "numWins";
	}
	else if (!str_comp_nocase("kills", category))
	{
		column = "numKills";
	}
	else if (!str_comp_nocase("wallshotkills", category))
	{
		column = "numKillsWallshot";
	}
	else if (!str_comp_nocase("deaths", category))
	{
		column = "numDeaths";
	}
	else if (!str_comp_nocase("shots", category))
	{
		column = "numShots";
	}
	else if (!str_comp_nocase("spree", category))
	{
		column = "highestSpree";
	}
	else if (!str_comp_nocase("time", category))
	{
		column = "timePlayed";
	}

	else
	{
		GameServer()->SendChatTarget(pPlayer->GetCID(), "Usage: /top [score|wins|kills|wallshotkills|deaths|shots|spree|time]");
		return;
	}

	std::thread *t = new std::thread(&CGameController_zCatch::ChatCommandTopFetchDataAndPrint, GameServer(), pPlayer->GetCID(), column);
	t->detach();
	delete t;
}

/* get the top players */
void CGameController_zCatch::ChatCommandTopFetchDataAndPrint(CGameContext* GameServer, int clientId, const char *column)
{

	/* prepare */
	const char *zTail;
	char sqlBuf[128];
	str_format(sqlBuf, sizeof(sqlBuf), "SELECT username, %s FROM zCatch ORDER BY %s DESC LIMIT 5;", column, column);
	const char *zSql = sqlBuf;
	sqlite3_stmt *pStmt;
	int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), zSql, strlen(zSql), &pStmt, &zTail);

	if (rc == SQLITE_OK)
	{

		/* lock database access in this process, but wait maximum 1 second */
		if (GameServer->LockRankingDb(1000))
		{

			/* when another process uses the database, wait up to 1 second */
			sqlite3_busy_timeout(GameServer->GetRankingDb(), 1000);

			/* fetch from database */
			int numRows = 0;
			int rc;
			while ((rc = sqlite3_step(pStmt)) == SQLITE_ROW)
			{
				const unsigned char* name = sqlite3_column_text(pStmt, 0);
				int value = sqlite3_column_int(pStmt, 1);
				char aBuf[64], bBuf[32];
				FormatRankingColumn(column, bBuf, value);
				str_format(aBuf, sizeof(aBuf), "[%s] %s", bBuf, name);
				/* if the player left and the client id is unused, nothing will happen */
				/* if another player joined, there is no big harm that he receives it */
				/* maybe later i have a good idea how to prevent this */
				GameServer->SendChatTarget(clientId, aBuf);
				++numRows;
			}

			/* unlock database access */
			GameServer->UnlockRankingDb();

			if (numRows == 0)
			{
				if (rc == SQLITE_BUSY)
					GameServer->SendChatTarget(clientId, "Could not load top ranks. Try again later.");
				else
					GameServer->SendChatTarget(clientId, "There are no ranks");
			}

		}
		else
		{
			GameServer->SendChatTarget(clientId, "Could not load top ranks. Try again later.");
		}
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
		GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
	}

	sqlite3_finalize(pStmt);
}

/* when a player typed /top into the chat */
void CGameController_zCatch::OnChatCommandOwnRank(CPlayer *pPlayer)
{
	OnChatCommandRank(pPlayer, GameServer()->Server()->ClientName(pPlayer->GetCID()));
}

/* when a player typed /top into the chat */
void CGameController_zCatch::OnChatCommandRank(CPlayer *pPlayer, const char *name)
{
	char *queryName = (char*)malloc(MAX_NAME_LENGTH);
	str_copy(queryName, name, MAX_NAME_LENGTH);
	
	// executes function with the parameters given afterwards
	std::thread *t = new std::thread(&CGameController_zCatch::ChatCommandRankFetchDataAndPrint, GameServer(), pPlayer->GetCID(), queryName);
	// detaches thread, meaning that the thread can outlive the main thread.
	t->detach();
	// free thread handle
	delete t;
}

/**
 * @brief Fetch ranking records of player "name" and send to requesting player with ID clientId 
 * @details Fetch ranking records of player "name" and send to requesting player with ID clientId 
 * 
 * @param GameServer CGameContext is needed in order to fetch most player info, e.g. ID, name, etc.
 * @param clientId Requesting player's clientId
 * @param name Unique name of the player whose rank is being requested by the player "clientId"
 */
void CGameController_zCatch::ChatCommandRankFetchDataAndPrint(CGameContext* GameServer, int clientId, char *name)
{

	/* prepare */
	const char *zTail;
	const char *zSql = "\
		SELECT \
			a.score, \
			a.numWins, \
			a.numKills, \
			a.numKillsWallshot, \
			a.numDeaths, \
			a.numShots, \
			a.highestSpree, \
			a.timePlayed, \
			(SELECT COUNT(*) FROM zCatch b WHERE b.score > a.score) + 1, \
			MAX(0, (SELECT MIN(b.score) FROM zCatch b WHERE b.score > a.score) - a.score) \
		FROM zCatch a \
		WHERE username = ?1\
		;";
	sqlite3_stmt *pStmt;
	int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), zSql, strlen(zSql), &pStmt, &zTail);

	if (rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite3_bind_text(pStmt, 1, name, strlen(name), 0);

		/* lock database access in this process, but wait maximum 1 second */
		if (GameServer->LockRankingDb(1000))
		{

			/* when another process uses the database, wait up to 1 second */
			sqlite3_busy_timeout(GameServer->GetRankingDb(), 1000);

			/* fetch from database */
			int row = sqlite3_step(pStmt);

			/* unlock database access */
			GameServer->UnlockRankingDb();

			/* result row was fetched */
			if (row == SQLITE_ROW)
			{

				int score = sqlite3_column_int(pStmt, 0);
				int numWins = sqlite3_column_int(pStmt, 1);
				int numKills = sqlite3_column_int(pStmt, 2);
				int numKillsWallshot = sqlite3_column_int(pStmt, 3);
				int numDeaths = sqlite3_column_int(pStmt, 4);
				int numShots = sqlite3_column_int(pStmt, 5);
				int highestSpree = sqlite3_column_int(pStmt, 6);
				int timePlayed = sqlite3_column_int(pStmt, 7);
				int rank = sqlite3_column_int(pStmt, 8);
				int scoreToNextRank = sqlite3_column_int(pStmt, 9);

				char aBuf[512];
				if (g_Config.m_SvMode == 1) // laser
				{
					str_format(aBuf, sizeof(aBuf), "'%s' is rank %d with a score of %.*f points (%d wins, %d kills (%d wallshot), %d deaths, %d shots, spree of %d, %d:%02dh played, %.*f points for next rank)", name, rank, score % 100 ? 2 : 0, score / 100.0, numWins, numKills, numKillsWallshot, numDeaths, numShots, highestSpree, timePlayed / 3600, timePlayed / 60 % 60, scoreToNextRank % 100 ? 2 : 0, scoreToNextRank / 100.0);
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "'%s' is rank %d with a score of %.*f points (%d wins, %d kills, %d deaths, %d shots, spree of %d, %d:%02dh played, %.*f points for next rank)", name, rank, score % 100 ? 2 : 0, score / 100.0, numWins, numKills, numDeaths, numShots, highestSpree, timePlayed / 3600, timePlayed / 60 % 60, scoreToNextRank % 100 ? 2 : 0, scoreToNextRank / 100.0);
				}
				GameServer->SendChatTarget(clientId, aBuf);
			}

			/* database is locked */
			else if (row == SQLITE_BUSY)
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "Could not get rank of '%s'. Try again later.", name);
				GameServer->SendChatTarget(clientId, aBuf);
			}

			/* no result found */
			else if (row == SQLITE_DONE)
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "'%s' has no rank", name);
				GameServer->SendChatTarget(clientId, aBuf);
			}

		}
		else
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Could not get rank of '%s'. Try again later.", name);
			GameServer->SendChatTarget(clientId, aBuf);
		}
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
		GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
	}

	sqlite3_finalize(pStmt);
	/*name has to be freed here, because this function is executed in a thread.*/
	free(name);
}


void CGameController_zCatch::FormatRankingColumn(const char* column, char buf[32], int value)
{
	if (!str_comp_nocase("score", column))
		str_format(buf, sizeof(buf), "%.*f", value % 100 ? 2 : 0, value / 100.0);
	else if (!str_comp_nocase("timePlayed", column))
		str_format(buf, sizeof(buf), "%d:%02dh", value / 3600, value / 60 % 60);
	else
		str_format(buf, sizeof(buf), "%d", value);
}


/**
 * @brief Toggles the Release game option
 * @details Depending on the SvLastManStandingDeathmatch aka Release Game setting, the AllowJoin settings are changed so that everyone can join a
 * running release game as long as the sv_last_standing_deathmatch option is enabled.
 * This function toggles between the state before the release game and the state while the release game is running, changing SvAllowJoin to 1
 * and changing it back to its previous setting if the SvLastManStandingDeathmatch setting is disabled(see in variables.h).
 *
 * @param Players_Ingame Count of players actually playing without those, which are explicidly spectating.
 */
void CGameController_zCatch::ToggleLastStandingDeathmatchAndRelease(int Players_Ingame) {
	if (g_Config.m_SvLastStandingDeathmatch == 1 && Players_Ingame < g_Config.m_SvLastStandingPlayers) {

		// old Joining settings are the same as the current ones and not the allow everyone to join option.
		if (m_OldAllowJoin == g_Config.m_SvAllowJoin && m_OldAllowJoin != 1)
		{
			// Allow players to freely join while last man standing treshold is not reached
			g_Config.m_SvAllowJoin = 1;

		} else if (m_OldAllowJoin == g_Config.m_SvAllowJoin && m_OldAllowJoin == 1) {
			// do nothing
		}

		// Go through ingame players and release their victims while last man standing treshold is not met and
		// Last man standig deathmatch is still enabled.
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			{
				GameServer()->m_apPlayers[i]->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
			}

		}
		// Last man standing deathmatch aka release game/ gDM are disabled and the old joining option differs from the
		// current one, so change it back.
	} else if (g_Config.m_SvLastStandingDeathmatch == 0 && m_OldAllowJoin != g_Config.m_SvAllowJoin)
	{
		// reset AllowJoin config if it was changed and the last man standing settings changed.
		g_Config.m_SvAllowJoin = m_OldAllowJoin;
	}
}


/**
 * @brief Merges two rankings into one TARGET ranking and deletes the SOURCE ranking.
 * @details Warning: Don't get confused if highest spree is not merged, because the highest spree is actually the maximum of both values.
 * 			This function can also merge and create into a not existing TARGET, which is then inserted into the database with the given
 * 			trimmed(no whitespaces on both sides) TARGET nickname.
 * 			WARNING: Given parameters: Source and Target are freed after the execution of this function.
 *
 * @param GameServer pointer to the CGameContext /  GameServer, which contains most information about players etc.
 * @param Source char* of the name of the Source player which is deleted at the end of the merge.
 * @param Target Target player, which receives all of the Source player's achievements.
 */
void CGameController_zCatch::MergeRankingIntoTarget(CGameContext* GameServer, char* Source, char* Target)
{

	int target_score;
	int target_numWins;
	int target_numKills;
	int target_numKillsWallshot;
	int target_numDeaths;
	int target_numShots;
	int target_highestSpree;
	int target_timePlayed;
	int target_rank;
	int target_scoreToNextRank;

	/*Sqlite stuff*/
	/*sqlite statement object*/
	sqlite3_stmt *pStmt;
	int target_rc;
	int target_row;

	/*error handling*/
	int err = 0;


	/* prepare */
	const char *zTail;
	const char *zSql = "\
		SELECT \
			a.score, \
			a.numWins, \
			a.numKills, \
			a.numKillsWallshot, \
			a.numDeaths, \
			a.numShots, \
			a.highestSpree, \
			a.timePlayed, \
			(SELECT COUNT(*) FROM zCatch b WHERE b.score > a.score) + 1, \
			MAX(0, (SELECT MIN(b.score) FROM zCatch b WHERE b.score > a.score) - a.score) \
		FROM zCatch a \
		WHERE username = trim(?1)\
		;";


	/* First part: fetch all data from Source player*/
	/*check if query is ok and create statement object pStmt*/
	target_rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), zSql, strlen(zSql), &pStmt, &zTail);

	if (target_rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite3_bind_text(pStmt, 1, Source, strlen(Source), 0);

		/* lock database access in this process, but wait maximum 1 second */
		if (GameServer->LockRankingDb(1000))
		{

			/* when another process uses the database, wait up to 1 second */
			sqlite3_busy_timeout(GameServer->GetRankingDb(), 1000);

			/* fetch from database */
			target_row = sqlite3_step(pStmt);

			/* unlock database access */
			GameServer->UnlockRankingDb();

			/* result row was fetched */
			if (target_row == SQLITE_ROW)
			{
				/*get results from columns*/
				target_score = sqlite3_column_int(pStmt, 0);
				target_numWins = sqlite3_column_int(pStmt, 1);
				target_numKills = sqlite3_column_int(pStmt, 2);
				target_numKillsWallshot = sqlite3_column_int(pStmt, 3);
				target_numDeaths = sqlite3_column_int(pStmt, 4);
				target_numShots = sqlite3_column_int(pStmt, 5);
				target_highestSpree = sqlite3_column_int(pStmt, 6);
				target_timePlayed = sqlite3_column_int(pStmt, 7);
				target_rank = sqlite3_column_int(pStmt, 8);
				target_scoreToNextRank = sqlite3_column_int(pStmt, 9);

				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "Fetched data of '%s'", Source);
				GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);

			}

			/* database is locked */
			else if (target_row == SQLITE_BUSY)
			{
				/* print error */
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "Could not get rank of '%s'. Try again later.", Source);
				GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
				err++;
			}

			/* no result found */
			else if (target_row == SQLITE_DONE)
			{
				/* print information */
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "'%s' has no rank", Source);
				GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
				err++;
			}

		}
		else
		{
			/* print error */
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Could not get rank of '%s'. Try again later.", Source);
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
			err++;
		}
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", target_rc, sqlite3_errmsg(GameServer->GetRankingDb()));
		GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		err++;

	}

	/*if at least one error ocurred, free everything and do nothing.*/
	if (err > 0 ) {
		sqlite3_finalize(pStmt);
		free(Source);
		free(Target);
		return;
	}

	/*Second part: add fetched data to Target player*/
	/* give the points to Target player*/
	CGameController_zCatch::SaveScore(GameServer, // gamecontext
	                                 Target, // username ---> is freed!!
	                                 target_score, // score
	                                 target_numWins, // numWins
	                                 target_numKills, // numKills
	                                 target_numKillsWallshot, // numKillsWallshot
	                                 target_numDeaths, // numDeaths
	                                 target_numShots, // numShots
	                                 target_highestSpree, // highestSpree
	                                 target_timePlayed // timePlayed
	                                 );
	
	/*Cannot use "Target" variable from here on. */
	
	/*Third part: delete Source player records.*/
	
	// Target has been freed in SaveScore already !!
	DeleteRanking(GameServer, Source);
	// Source has been freed in DeleteRanking!
	sqlite3_finalize(pStmt);
	// Freeing allocated memory is done in these functions, because they are executed as detached threads.
}


/**
 * @brief Delete the ranking of given player name.
 * @details This function deletes the ranking of the given player represented by his/her nickname.
 * 			WARNING: This function frees the allocated memory of the "Name" parameter.
 *
 * @param GameServer Is a CGameContext Object, which hold the information about out sqlite3 database handle.
 * 					That handle is needed here to execute queries on the database. 
 * @param Name Is the name of the player whose ranking score should be deleted. The name is trimmed from both sides, 
 * 			   in order to have a consistent ranking and no faking or faulty deletions.
 */
void CGameController_zCatch::DeleteRanking(CGameContext* GameServer, char* Name) {
	const char *zTail;
	const char *zSql = "DELETE FROM zCatch WHERE username = trim(?1);";
	sqlite3_stmt * pStmt;
	int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), zSql, strlen(zSql), &pStmt, &zTail);
	if (rc == SQLITE_OK) {

		/* bind parameters in query */
		sqlite3_bind_text(pStmt, 1, Name, strlen(Name), 0);

		/* lock database access in this process */
		GameServer->LockRankingDb();

		/* when another process uses the database, wait up to 1 minute */
		sqlite3_busy_timeout(GameServer->GetRankingDb(), 60000);

		char aBuf[512];

		/* save to database */
		switch (sqlite3_step(pStmt))
		{
		case SQLITE_DONE:
			/* Print deletion success message to rcon */
			str_format(aBuf, sizeof(aBuf), "Deleting records of '%s'" , Name);
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
			break;
		case SQLITE_BUSY:

			str_format(aBuf, sizeof(aBuf), "Error: could not delete records of '%s'(timeout)." , Name);
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
			break;
		default:
			str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		}

		/* unlock database access */
		GameServer->UnlockRankingDb();
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
		GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
	}

	sqlite3_finalize(pStmt);
	free(Name);
}

/**
 * @brief Returns the mode specific name. WARNING: Do not forget to free the returned pointer after usage.
 * @details Depending on the current game mode e.g. Laser, Grenade, etc. this function returns a pointer to a
 * 			string with the name of specified mode. This is needed to constuct sqlite queries with the mode specific tables
 * 			in the database.
 * @return name of current game mode.
 */
char* CGameController_zCatch::GetGameModeTableName(){
	char* aBuf = (char*)malloc(sizeof(char)*16);
	
	switch(g_Config.m_SvMode){
		case 1: return strncpy(aBuf,"Laser", 4); break;
		case 2: return strncpy(aBuf,"Everything", 10); break;
		case 3: return strncpy(aBuf,"Hammer", 6); break;
		case 4: return strncpy(aBuf,"Grenade", 7); break;
		case 5: return strncpy(aBuf,"Ninja", 5); break;
		default: return strncpy(aBuf,"zCatch", 6); break;

		return aBuf;

	}
}

















