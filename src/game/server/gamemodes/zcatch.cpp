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
#include <future>

// Global stuff
static int m_OldGameMode;

CGameController_zCatch::CGameController_zCatch(class CGameContext *pGameServer) :
	IGameController(pGameServer)
{
	m_pGameType = "zCatch+";
	m_OldGameMode = g_Config.m_SvMode;
	m_OldSvReleaseGame = g_Config.m_SvLastStandingDeathmatch;
	// jxsl13 added to save old server config. Needed for last man
	// standing deathmatch feature to reset to previous state
	// if treshold of players is reached.
	m_OldAllowJoin = g_Config.m_SvAllowJoin;
	if (g_Config.m_SvLastStandingDeathmatch)
	{
		g_Config.m_SvAllowJoin = 1;
	}
}

CGameController_zCatch::~CGameController_zCatch() {
	/* save all players */
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (GameServer()->m_apPlayers[i])
			SaveRanking(GameServer()->m_apPlayers[i]);
	}

}

void CGameController_zCatch::CheckGameConfigStatus() {
	// every 5 seconds check if >= half of the players needed to finish a round are playing
	// if noboody has a rainbow, give someone a rainbow.
	if (g_Config.m_SvAllowRainbow && g_Config.m_SvLastStandingDeathmatch && Server()->Tick() % (Server()->TickSpeed() * 5) == 0) {
		if (m_PlayersPlaying >= static_cast<int>(g_Config.m_SvLastStandingPlayers / 2)) {
			// -1 meaning that everyone can get the rainbow, otherwise the ID passed would not
			// be part of the possible players to get the rainbow.
			GiveRainbowToRandomPlayer(-1, !m_SomoneHasRainbow);
		}
	} else if (g_Config.m_SvAllowRainbow && !g_Config.m_SvLastStandingDeathmatch)
		{
			g_Config.m_SvAllowRainbow = 0;
			GameServer()->SendChatTarget(-1, "Could not enable the rainbow, because the Release Game is not being played.");
		}
	// we are not resetting any given rainbows othersise!


	// checks rls game stuff, whether something changed...
	if (m_OldSvReleaseGame != g_Config.m_SvLastStandingDeathmatch) {

		// rls game was enabled
		if (g_Config.m_SvLastStandingDeathmatch == 1) {

			// if this condition is not met,
			// reset everything back to before the rls game
			if (static_cast<double>(m_PlayerMostCaughtPlayers) > m_PlayersPlaying * 0.45)
			{
				char aBuf[64];
				str_format(aBuf, 64, "Could not enable the Release Game");
				GameServer()->SendChatTarget(-1, aBuf);

				str_format(aBuf, 64, "%s is dominating with %d caught players!",
				           GameServer()->Server()->ClientName(m_PlayerIdWithMostCaughtPlayers), m_PlayerMostCaughtPlayers);
				GameServer()->SendChatTarget(-1, aBuf);

				// reset global rls game config.
				g_Config.m_SvLastStandingDeathmatch = m_OldSvReleaseGame;
				return;
			}
			m_OldSvReleaseGame = g_Config.m_SvLastStandingDeathmatch;

			GameServer()->SendBroadcast("Release Game was enabled.", -1);
			m_OldAllowJoin = g_Config.m_SvAllowJoin;
			g_Config.m_SvAllowJoin = 1;

			// go through all players and release all of their caught victims, if rls game is enabled.
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (GameServer()->m_apPlayers[i])
				{
					GameServer()->m_apPlayers[i]->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				}
			}

			// rls game was disabled
		} else if (g_Config.m_SvLastStandingDeathmatch == 0) {
			m_OldSvReleaseGame = g_Config.m_SvLastStandingDeathmatch;

			if (g_Config.m_SvAllowJoin != m_OldAllowJoin) {
				g_Config.m_SvAllowJoin = m_OldAllowJoin;
			}
		}
	}

}

/* ranking system: create zcatch score table */
void CGameController_zCatch::OnInitRanking(sqlite3 *rankingDb) {
	for (int i = 1; i <= 5; ++i)
	{

		char *zErrMsg = 0;

		/* lock database access in this process */
		GameServer()->LockRankingDb();

		/* when another process uses the database, wait up to 10 seconds */
		sqlite3_busy_timeout(GameServer()->GetRankingDb(), 10000);


		char aMode[16];
		str_format(aMode, sizeof(aMode), "%s", GetGameModeTableName(i)) ;

		char aQuery[2048];
		str_format(aQuery, sizeof(aQuery), "\
			BEGIN; \
			CREATE TABLE IF NOT EXISTS %s ( \
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
			CREATE INDEX IF NOT EXISTS %s_score_index ON %s (score); \
			CREATE INDEX IF NOT EXISTS %s_numWins_index ON %s (numWins); \
			CREATE INDEX IF NOT EXISTS %s_numKills_index ON %s (numKills); \
			CREATE INDEX IF NOT EXISTS %s_numKillsWallshot_index ON %s (numKillsWallshot); \
			CREATE INDEX IF NOT EXISTS %s_numDeaths_index ON %s (numDeaths); \
			CREATE INDEX IF NOT EXISTS %s_numShots_index ON %s (numShots); \
			CREATE INDEX IF NOT EXISTS %s_highestSpree_index ON %s (highestSpree); \
			CREATE INDEX IF NOT EXISTS %s_timePlayed_index ON %s (timePlayed); \
			CREATE VIEW IF NOT EXISTS %sView (username, kd) \
			AS \
			SELECT username, (t.numKills + t.numKillsWallshot)/(t.numDeaths) as kd FROM %s as t \
			ORDER BY kd DESC LIMIT 5; \
			COMMIT; \
		", aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode, aMode);

		int rc = sqlite3_exec(GameServer()->GetRankingDb(), aQuery, NULL, 0, &zErrMsg);

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
}

void CGameController_zCatch::Tick()
{
	IGameController::Tick();
	CheckGameConfigStatus();
	GameServer()->CleanFutures();

	if (m_OldGameMode != g_Config.m_SvMode && !GameServer()->m_World.m_Paused)
	{
		EndRound();
	}

}

// Also checks if last standing player deathmatch is played before player treshhold is reached
void CGameController_zCatch::DoWincheck()
{
	// if game is running
	if (m_GameOverTick == -1)
	{
		int Players = 0, Players_Spec = 0, Players_SpecExplicit = 0;
		int winnerId = -1;
		CPlayer *winner = NULL;

		int caughtPlayers = 0; // in total

		int playerIdWithMostCaughtPlayers = -1;
		int currentlyMaxCaughtPlayers = 0; // of one player
		int rainbowCount = 0;


		// go through all players
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			// if player exists, count that player
			if (GameServer()->m_apPlayers[i])
			{
				Players++;

				// count players in spec, explicidly in spec and in spec because they were caught
				if (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS) {

					// if people ae playing the rainbow rls game and someone joins spec
					// then remove his rainbow and give it to someone else.
					// if ppl do not play the rainbow rls game, do nothing.
					if (g_Config.m_SvAllowRainbow && GameServer()->m_apPlayers[i]->IsRainbowTee())
					{
						bool playerHadRainbowBody = GameServer()->m_apPlayers[i]->m_IsRainbowBodyTee;
						bool playerHadRainbowFeet = GameServer()->m_apPlayers[i]->m_IsRainbowFeetTee;
						GameServer()->m_apPlayers[i]->ResetRainbowTee();
						GiveRainbowToRandomPlayer(i, true, playerHadRainbowBody, playerHadRainbowFeet);
					}
					Players_Spec++;
				} else {
					// if not in spec, set winner
					winnerId = i;
					winner = GameServer()->m_apPlayers[i];

					// update dominating player if he has at least one enemy caught
					if (GameServer()->m_apPlayers[i]->m_zCatchNumVictims > 0
					        && GameServer()->m_apPlayers[i]->m_zCatchNumVictims > currentlyMaxCaughtPlayers)
					{
						currentlyMaxCaughtPlayers = GameServer()->m_apPlayers[i]->m_zCatchNumVictims;
						playerIdWithMostCaughtPlayers = i;
					}


					// rainbow stuff
					if (GameServer()->m_apPlayers[i]->IsRainbowTee()) {
						rainbowCount++;
					}

					// if player has someone caught, add caught players to count of all caught players
					caughtPlayers += GameServer()->m_apPlayers[i]->m_zCatchNumVictims;
				}

				if (GameServer()->m_apPlayers[i]->m_SpecExplicit)
					Players_SpecExplicit++;
			}
		}

		int Players_Ingame = Players - Players_SpecExplicit;

		// Update member variables
		m_PlayersOnline = Players;
		m_PlayersPlaying = Players_Ingame;
		m_PlayersExplicitlySpectating = Players_SpecExplicit;
		m_PlayersCaughtSpectating = Players_Spec - m_PlayersExplicitlySpectating;
		m_PlayersNotCaught = m_PlayersPlaying - m_PlayersCaughtSpectating;
		m_PlayerIdWithMostCaughtPlayers = playerIdWithMostCaughtPlayers;
		m_PlayerMostCaughtPlayers = currentlyMaxCaughtPlayers;
		m_RainbowCount = rainbowCount;
		m_SomoneHasRainbow = rainbowCount > 0;


		// players without explicit spectators
		if (Players_Ingame <= 1)
		{
			//do nothing
		}
		// last man standing ingame without all the players, who are caught and/or explicidly spectating
		else if ((Players - Players_Spec) == 1)
		{
			// go through all players and give the actual winner the score he/she earned
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				{
					// give score
					GameServer()->m_apPlayers[i]->m_Score += g_Config.m_SvBonus;

					// release all of his/her victims
					if (Players_Ingame < g_Config.m_SvLastStandingPlayers)
						GameServer()->m_apPlayers[i]->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				}
			}

			// if winner exists but failed har dmode
			if (winner && winner->m_HardMode.m_Active && winner->m_HardMode.m_ModeTotalFails.m_Active && winner->m_HardMode.m_ModeTotalFails.m_Fails > winner->m_HardMode.m_ModeTotalFails.m_Max)
			{
				winner->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				winner->m_HardMode.m_ModeTotalFails.m_Fails = 0;
				GameServer()->SendChatTarget(-1, "The winner failed the hard mode.");
				GameServer()->SendBroadcast("The winner failed the hard mode.", -1);
			}
			// winner exists and not enough players to end the round
			else if (winner && Players_Ingame < g_Config.m_SvLastStandingPlayers)
			{
				winner->HardModeRestart();


				if (g_Config.m_SvLastStandingDeathmatch == 1) {
					GameServer()->SendBroadcast("Cannot end the round, because we are playing the Release Game.", -1);
					GameServer()->SendChatTarget(-1, "Cannot end the round, because we are too few players.");
				} else {
					GameServer()->SendBroadcast("Too few players to end round.", -1);
					GameServer()->SendChatTarget(-1, "Too few players to end round.");
				}

			}
			// if simply a winner exists
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
			// if ingame players > 1
			ToggleLastStandingDeathmatchAndRelease(Players_Ingame, caughtPlayers);
		}

		IGameController::DoWincheck(); //do also usual wincheck
	}
}

int CGameController_zCatch::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	if (!pKiller)
		return 0;

	CPlayer *victim = pVictim->GetPlayer();

	// disable rainbow always when someone is killed.
	bool victimHadRainbow = victim->IsRainbowTee();
	bool victimHadRainbowBody = victim->m_IsRainbowBodyTee;
	bool victimHadRainbowFeet = victim->m_IsRainbowFeetTee;

	// rainbow is removed if the player is killed after the rls game end.
	// do not change this, because it seems funny that players could end a round
	// while having the rainbow after the rls game.
	if (victimHadRainbow) {
		victim->ResetRainbowTee();
	}

	if (pKiller != victim)
	{
		// only pass the rainbow property if still in rls game mode.
		if (g_Config.m_SvAllowRainbow && g_Config.m_SvLastStandingDeathmatch && victimHadRainbow) {
			if (victimHadRainbowBody)
			{
				pKiller->GiveBodyRainbow();
			}
			if (victimHadRainbowFeet)
			{
				pKiller->GiveFeetRainbow();
			}
		}

		// todo: check if counting globally has any negative influence on this.

		/* you can at max get that many points as there are players playing */
		pKiller->m_Score += min(victim->m_zCatchNumKillsInARow + 1, m_PlayersOnline);
		// pre increment otherwise copies instance and other crap.
		++pKiller->m_Kills;
		++victim->m_Deaths;

		/* Check if the killer has been already killed and is in spectator (victim may died through wallshot) */
		if (pKiller->GetTeam() != TEAM_SPECTATORS && (!pVictim->m_KillerLastDieTickBeforceFiring || pVictim->m_KillerLastDieTickBeforceFiring == pKiller->m_DieTick))
		{
			++pKiller->m_zCatchNumKillsInARow;
			//release game only add victims if these conditions are met.
			if (!g_Config.m_SvLastStandingDeathmatch || m_PlayersPlaying >= g_Config.m_SvLastStandingPlayers ||
			        (g_Config.m_SvLastStandingDeathmatch && m_PlayerMostCaughtPlayers >= (g_Config.m_SvLastStandingPlayers - 1)))
			{
				pKiller->AddZCatchVictim(victim->GetCID(), CPlayer::ZCATCH_CAUGHT_REASON_KILLED);
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "You are caught until '%s' dies.", Server()->ClientName(pKiller->GetCID()));
				GameServer()->SendChatTarget(victim->GetCID(), aBuf);
			}

		}

	}
	else
	{
		// selfkill/death
		if (WeaponID == WEAPON_SELF || WeaponID == WEAPON_WORLD)
		{
			victim->m_Score -= g_Config.m_SvKillPenalty;
			++victim->m_Deaths;

			// give rainbow to random player if rls game is enabled.
			// m_SomeoneHasRainbow is also no needed here, because it has not been updated yet.
			// the person has been killed by a non player entity
			// and he had the rainbow property, give a random person that rainbow
			GiveRainbowToRandomPlayer(victim->GetCID(),
			                          g_Config.m_SvAllowRainbow && g_Config.m_SvLastStandingDeathmatch && victimHadRainbow, // condition
			                          victimHadRainbowBody, // whether victim had rainbow body
			                          victimHadRainbowFeet); // whether victim had rainbow feet.
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

void CGameController_zCatch::GiveRainbowToRandomPlayer(int VictimID, bool condition, bool body, bool feet) {
// give RAINBOW to a random player that is ingame
	if (VictimID < -1 || VictimID >= MAX_CLIENTS) {
		dbg_msg("ERROR", "Error in CGameController_zCatch::GiveRainbowToRandomPlayer occurred, ID is invalid.");
		return;
	}

	if (condition) {
		std::vector<int> ingamePlayers;

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (VictimID == i) {
				continue;
			}
			if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			{
				ingamePlayers.push_back(i);
			}
		}

		// gametick modulo size of ingame(non-spec) players, that's quite random
		if (ingamePlayers.size() > 0) {
			int chosenId = ingamePlayers.at(static_cast<int>(Server()->Tick() % ingamePlayers.size()));
			CPlayer *chosenPlayer = GameServer()->m_apPlayers[chosenId];
			// give that person the rainbow body and feet
			// depending on whether the passed variabled were set.
			if (body)
			{
				chosenPlayer->GiveBodyRainbow();
			}
			if (feet)
			{
				chosenPlayer->GiveFeetRainbow();
			}
		}

	}
}

void CGameController_zCatch::OnPlayerInfoChange(class CPlayer *pP)
{
	if (!g_Config.m_SvColorIndicator) {
		return;
	}

	if (pP->m_zCatchNumKillsInARow <= 20) {
		switch (pP->m_zCatchNumKillsInARow) {
		case 0:
			pP->m_TeeInfos.m_ColorBody = 0x1800ff;
			pP->m_TeeInfos.m_ColorFeet = 0x695cff;
			break;
		case 1:
			pP->m_TeeInfos.m_ColorBody = 0x0028ff;
			pP->m_TeeInfos.m_ColorFeet = 0x5c78ff;
			break;
		case 2:
			pP->m_TeeInfos.m_ColorBody = 0x0088ff;
			pP->m_TeeInfos.m_ColorFeet = 0x5cb5ff;
			break;
		case 3:
			pP->m_TeeInfos.m_ColorBody = 0x00fbff;
			pP->m_TeeInfos.m_ColorFeet = 0x5cfeff;
			break;
		case 4:
			pP->m_TeeInfos.m_ColorBody = 0x00ff91;
			pP->m_TeeInfos.m_ColorFeet = 0x5cffb6;
			break;
		case 5:
			pP->m_TeeInfos.m_ColorBody = 0x00ff23;
			pP->m_TeeInfos.m_ColorFeet = 0x5cff71;
			break;
		case 6:
			pP->m_TeeInfos.m_ColorBody = 0x3cff00;
			pP->m_TeeInfos.m_ColorFeet = 0x84ff5c;
			break;
		case 7:
			pP->m_TeeInfos.m_ColorBody = 0x9cff00;
			pP->m_TeeInfos.m_ColorFeet = 0xc1ff5c;
			break;
		case 8:
			pP->m_TeeInfos.m_ColorBody = 0xfbff00;
			pP->m_TeeInfos.m_ColorFeet = 0xffff5c;
			break;
		case 9:
			pP->m_TeeInfos.m_ColorBody = 0xffe000;
			pP->m_TeeInfos.m_ColorFeet = 0xffeb5c;
			break;
		case 10:
			pP->m_TeeInfos.m_ColorBody = 0xffc000;
			pP->m_TeeInfos.m_ColorFeet = 0xffd65c;
			break;
		case 11:
			pP->m_TeeInfos.m_ColorBody = 0xffa100;
			pP->m_TeeInfos.m_ColorFeet = 0xffc25c;
			break;
		case 12:
			pP->m_TeeInfos.m_ColorBody = 0xff7900;
			pP->m_TeeInfos.m_ColorFeet = 0xffa85c;
			break;
		case 13:
			pP->m_TeeInfos.m_ColorBody = 0xff4c00;
			pP->m_TeeInfos.m_ColorFeet = 0xff8c5c;
			break;
		case 14:
			pP->m_TeeInfos.m_ColorBody = 0xff1f00;
			pP->m_TeeInfos.m_ColorFeet = 0xff6f5c;
			break;
		case 15:
			pP->m_TeeInfos.m_ColorBody = 0xff0020;
			pP->m_TeeInfos.m_ColorFeet = 0xff5c74;
			break;
		case 16:
			pP->m_TeeInfos.m_ColorBody = 0x800046;
			pP->m_TeeInfos.m_ColorFeet = 0x802e5d;
			break;
		case 17:
			pP->m_TeeInfos.m_ColorBody = 0xff00f9;
			pP->m_TeeInfos.m_ColorFeet = 0xff5cfe;
			break;
		case 18:
			pP->m_TeeInfos.m_ColorBody = 0x9900ff;
			pP->m_TeeInfos.m_ColorFeet = 0xbb5cff;
			break;
		case 19:
			pP->m_TeeInfos.m_ColorBody = 0x5700ff;
			pP->m_TeeInfos.m_ColorFeet = 0x925cff;
			break;
		default:
			pP->m_IsRainbowBodyTee = true;
			pP->m_IsRainbowFeetTee = true;
			break;

		}
	}

	if (pP->m_HardMode.m_Active) {
		pP->m_TeeInfos.m_ColorFeet = 0x000000;
	}
	pP->m_TeeInfos.m_UseCustomColor = 1;

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
	if (m_OldGameMode != g_Config.m_SvMode)
	{

		EndRound();
		m_OldGameMode = g_Config.m_SvMode;
		//Server()->MapReload();
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
	GameServer()->AddFuture(std::async(std::launch::async, &CGameController_zCatch::SaveScore,
	                                   GameServer(), // gamecontext
	                                   name, // username
	                                   player->m_RankCache.m_Points, // score
	                                   player->m_RankCache.m_NumWins, // numWins
	                                   player->m_RankCache.m_NumKills, // numKills
	                                   player->m_RankCache.m_NumKillsWallshot, // numKillsWallshot
	                                   player->m_RankCache.m_NumDeaths, // numDeaths
	                                   player->m_RankCache.m_NumShots, // numShots
	                                   player->m_zCatchNumKillsInARow, // highestSpree
	                                   player->m_RankCache.m_TimePlayed / Server()->TickSpeed(), // timePlayed
	                                   0,
	                                   0));


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
void CGameController_zCatch::SaveScore(CGameContext* GameServer, char *name, int score, int numWins, int numKills, int numKillsWallshot, int numDeaths, int numShots, int highestSpree, int timePlayed, int GameMode, int Free) {
	// Don't save connecting players
	if (str_comp(name, "(connecting)") == 0 || str_comp(name, "(invalid)") == 0)
	{
		if (!Free)
		{
			free(name);
		}
		return;
	}

	/* debug */
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Saving user stats of '%s' in mode: %s", name, GetGameModeTableName(GameMode));
	GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);

	/* prepare */
	const char *zTail;

	char aMode[16];
	str_format(aMode, sizeof(aMode), "%s", GetGameModeTableName(GameMode)) ;

	char aQuery[1024];
	str_format(aQuery, sizeof(aQuery), "\
		INSERT OR REPLACE INTO %s ( \
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
			FROM %s \
		) old ON old.username = new.username; \
		", aMode, aMode);

	sqlite3_stmt *pStmt = 0;
	int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), aQuery, strlen(aQuery), &pStmt, &zTail);

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

	if (!Free) {
		free(name);
	}

}

/* when a player typed /top into the chat */
void CGameController_zCatch::OnChatCommandTop(CPlayer *pPlayer, const char *category)
{
	const char *column;
	bool isCalculatedViewData = false;

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
	} else if (!str_comp_nocase("kd", category)) {
		column = "kd";
		isCalculatedViewData = true;
	}
	else
	{
		GameServer()->SendChatTarget(pPlayer->GetCID(), "Usage: /top [score|wins|kills|wallshotkills|deaths|shots|spree|time|kd]");
		return;
	}
	if (!isCalculatedViewData) {
		GameServer()->AddFuture(std::async(std::launch::async, &CGameController_zCatch::ChatCommandTopFetchDataAndPrint, GameServer(), pPlayer->GetCID(), column));
	} else {
		GameServer()->AddFuture(std::async(std::launch::async, &CGameController_zCatch::ChatCommandTopFetchDataFromViewAndPrint, GameServer(), pPlayer->GetCID(), column));
	}

}

/* get the top players */
void CGameController_zCatch::ChatCommandTopFetchDataAndPrint(CGameContext* GameServer, int clientId, const char *column)
{

	/* prepare */
	const char *zTail;
	char sqlBuf[128];
	str_format(sqlBuf, sizeof(sqlBuf), "SELECT username, %s FROM %s ORDER BY %s DESC LIMIT 5;", column, GetGameModeTableName(0), column);
	const char *zSql = sqlBuf;
	sqlite3_stmt *pStmt = 0;
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
				// don't show in top if no score available.
				if (value == 0) {
					continue;
				}
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


/* get the top players */
void CGameController_zCatch::ChatCommandTopFetchDataFromViewAndPrint(CGameContext* GameServer, int clientId, const char *column)
{

	/* prepare */
	const char *zTail;
	char sqlBuf[128];
	str_format(sqlBuf, sizeof(sqlBuf), "SELECT username, %s FROM %sView ORDER BY %s DESC LIMIT 5;", column, GetGameModeTableName(0), column);
	const char *zSql = sqlBuf;
	sqlite3_stmt *pStmt = 0;
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
				// don't show in top if no score available.
				if (value == 0) {
					continue;
				}
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


/* when a player typed /rank into the chat */
void CGameController_zCatch::OnChatCommandOwnRank(CPlayer *pPlayer)
{
	OnChatCommandRank(pPlayer, GameServer()->Server()->ClientName(pPlayer->GetCID()));
}

/* when a player typed /rank nickname into the chat */
void CGameController_zCatch::OnChatCommandRank(CPlayer *pPlayer, const char *name)
{
	char *queryName = (char*)malloc(MAX_NAME_LENGTH);
	str_copy(queryName, name, MAX_NAME_LENGTH);

	// executes function with the parameters given afterwards
	//GameServer()->AddThread(new std::thread(&CGameController_zCatch::ChatCommandRankFetchDataAndPrint, GameServer(), pPlayer->GetCID(), queryName));
	GameServer()->AddFuture(std::async(std::launch::async, &CGameController_zCatch::ChatCommandRankFetchDataAndPrint, GameServer(), pPlayer->GetCID(), queryName));
	// detaches thread, meaning that the thread can outlive the main thread.
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

	char aMode[16];
	str_format(aMode, sizeof(aMode), "%s", GetGameModeTableName(0)) ;

	char aQuery[512];
	str_format(aQuery, sizeof(aQuery), "\
		SELECT \
			a.score, \
			a.numWins, \
			a.numKills, \
			a.numKillsWallshot, \
			a.numDeaths, \
			a.numShots, \
			a.highestSpree, \
			a.timePlayed, \
			(SELECT COUNT(*) FROM %s b WHERE b.score > a.score) + 1, \
			MAX(0, (SELECT MIN(b.score) FROM %s b WHERE b.score > a.score) - a.score) \
		FROM %s a \
		WHERE username = ?1\
		;", aMode, aMode, aMode);

	sqlite3_stmt *pStmt = 0;
	int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), aQuery, strlen(aQuery), &pStmt, &zTail);

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
				// needs at least one win to be ranked
				if (score > 0)
				{
					if (g_Config.m_SvMode == 1) // laser
					{
						str_format(aBuf, sizeof(aBuf), "'%s' is rank %d with a score of %.*f points and %d wins (%d kills (%d wallshot), %d deaths, %d shots, spree of %d, %d:%02dh played, %.*f points for next rank)", name, rank, score % 100 ? 2 : 0, score / 100.0, numWins, numKills, numKillsWallshot, numDeaths, numShots, highestSpree, timePlayed / 3600, timePlayed / 60 % 60, scoreToNextRank % 100 ? 2 : 0, scoreToNextRank / 100.0);
					}
					else
					{
						str_format(aBuf, sizeof(aBuf), "'%s' is rank %d with a score of %.*f points and %d wins (%d kills, %d deaths, %d shots, spree of %d, %d:%02dh played, %.*f points for next rank)", name, rank, score % 100 ? 2 : 0, score / 100.0, numWins, numKills, numDeaths, numShots, highestSpree, timePlayed / 3600, timePlayed / 60 % 60, scoreToNextRank % 100 ? 2 : 0, scoreToNextRank / 100.0);
					}
				} else {
					str_format(aBuf, sizeof(aBuf), "'%s' has no rank", name);
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
	size_t size = 32;
	if (!str_comp_nocase("score", column)) {
		str_format(buf, size, "%.2f", value / 100.0);
	}
	else if (!str_comp_nocase("timePlayed", column))
		str_format(buf, size, "%d:%02dh", value / 3600, value / 60 % 60);
	else
		str_format(buf, size, "%d", value);
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
void CGameController_zCatch::ToggleLastStandingDeathmatchAndRelease(int Players_Ingame, int caughtPlayers) {

	// if rls game is enabled
	if (g_Config.m_SvLastStandingDeathmatch) {
		//
		if (Players_Ingame >= g_Config.m_SvLastStandingPlayers && m_OldPlayersIngame < g_Config.m_SvLastStandingPlayers) {
			GameServer()->SendBroadcast("Enough players to end the round. Let the fun begin!", -1);
			GameServer()->SendChatTarget(-1, "Release Game was disabled.");
			// disable release game
			g_Config.m_SvLastStandingDeathmatch = 0;
		}
	}
	m_OldPlayersIngame = Players_Ingame;


}


/**
 * @brief Merges two rankings into one TARGET ranking and deletes the SOURCE ranking.
 * @details Warning: Don't get confused if highest spree is not merged, because the highest spree is actually the maximum of both values.
 * 			This function can also merge and create a not existing TARGET, which is then inserted into the database with the given
 * 			trimmed(no whitespaces on both sides) TARGET nickname.
 * 			WARNING: Given parameters: Source and Target are freed after the execution of this function.
 *
 * @param GameServer pointer to the CGameContext /  GameServer, which contains most information about players etc.
 * @param Source char* of the name of the Source player which is deleted at the end of the merge.
 * @param Target Target player, which receives all of the Source player's achievements.
 */
void CGameController_zCatch::MergeRankingIntoTarget(CGameContext* GameServer, char* Source, char* Target)
{
	for (int i = 1; i <= 5; ++i)
	{

		int source_score = 0;
		int source_numWins = 0;
		int source_numKills = 0;
		int source_numKillsWallshot = 0;
		int source_numDeaths = 0;
		int source_numShots = 0;
		int source_highestSpree = 0;
		int source_timePlayed = 0;

		/*Sqlite stuff*/
		/*sqlite statement object*/
		sqlite3_stmt *pStmt = 0;
		int source_rc;
		int source_row;

		/*error handling*/
		int err = 0;


		/* prepare */
		const char *zTail;
		char aMode[16];
		str_format(aMode, sizeof(aMode), "%s", GetGameModeTableName(i)) ;

		char aQuery[256];
		str_format(aQuery, sizeof(aQuery), "\
		SELECT \
			a.score, \
			a.numWins, \
			a.numKills, \
			a.numKillsWallshot, \
			a.numDeaths, \
			a.numShots, \
			a.highestSpree, \
			a.timePlayed \
		FROM %s a \
		WHERE username = trim(?1)\
		;", aMode);

		/* First part: fetch all data from Source player*/
		/*check if query is ok and create statement object pStmt*/
		source_rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), aQuery, strlen(aQuery), &pStmt, &zTail);

		if (source_rc == SQLITE_OK)
		{
			/* bind parameters in query */
			sqlite3_bind_text(pStmt, 1, Source, strlen(Source), 0);

			/* lock database access in this process, but wait maximum 1 second */
			if (GameServer->LockRankingDb(1000))
			{

				/* when another process uses the database, wait up to 1 second */
				sqlite3_busy_timeout(GameServer->GetRankingDb(), 1000);

				/* fetch from database */
				source_row = sqlite3_step(pStmt);

				/* unlock database access */
				GameServer->UnlockRankingDb();

				/* result row was fetched */
				if (source_row == SQLITE_ROW)
				{
					/*get results from columns*/
					source_score = sqlite3_column_int(pStmt, 0);
					source_numWins = sqlite3_column_int(pStmt, 1);
					source_numKills = sqlite3_column_int(pStmt, 2);
					source_numKillsWallshot = sqlite3_column_int(pStmt, 3);
					source_numDeaths = sqlite3_column_int(pStmt, 4);
					source_numShots = sqlite3_column_int(pStmt, 5);
					source_highestSpree = sqlite3_column_int(pStmt, 6);
					source_timePlayed = sqlite3_column_int(pStmt, 7);

					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "Fetched data of '%s' in mode: %s", Source, GetGameModeTableName(i));
					GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);

				}

				/* database is locked */
				else if (source_row == SQLITE_BUSY)
				{
					/* print error */
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "Could not get rank of '%s'. Try again later.", Source);
					GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
					err++;
				}

				/* no result found */
				else if (source_row == SQLITE_DONE)
				{
					/* print information */
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "'%s' has no rank in mode: %s", Source, GetGameModeTableName(i));
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
			str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", source_rc, sqlite3_errmsg(GameServer->GetRankingDb()));
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
			err++;

		}

		/*if at least one error ocurred, free everything and do nothing.*/
		if (err > 0 ) {
			sqlite3_finalize(pStmt);
			continue;
		}

		/*Second part: add fetched data to Target player*/
		/* give the points to Target player*/
		CGameController_zCatch::SaveScore(GameServer, // gamecontext
		                                  Target, // username ---> is freed!!
		                                  source_score, // score
		                                  source_numWins, // numWins
		                                  source_numKills, // numKills
		                                  source_numKillsWallshot, // numKillsWallshot
		                                  source_numDeaths, // numDeaths
		                                  source_numShots, // numShots
		                                  source_highestSpree, // highestSpree
		                                  source_timePlayed, // timePlayed
		                                  i,
		                                  1); // don't free Target

		/*Cannot use "Target" variable from here on. */

		/*Third part: delete Source player records.*/

		// don't free Source with 1 != 0 (frees)
		DeleteRanking(GameServer, Source, i, 1);
		// Source has been freed in DeleteRanking!
		sqlite3_finalize(pStmt);
		// Freeing allocated memory is done in these functions, because they are executed as detached threads.
	}

	free(Source);
	free(Target);
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
void CGameController_zCatch::DeleteRanking(CGameContext* GameServer, char* Name, int GameMode, int Free) {
	const char *zTail;
	char aMode[16];
	str_format(aMode, sizeof(aMode), "%s", GetGameModeTableName(GameMode)) ;

	char aQuery[128];
	str_format(aQuery, sizeof(aQuery), "DELETE FROM %s WHERE username = trim(?1);", aMode);

	sqlite3_stmt * pStmt;
	int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), aQuery, strlen(aQuery), &pStmt, &zTail);
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
			str_format(aBuf, sizeof(aBuf), "Deleting records of '%s' in mode: %s" , Name, GetGameModeTableName(GameMode));
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

	if (!Free) {
		free(Name);
	}
}

/**
 * @brief Returns the mode specific name. WARNING: Do not forget to free the returned pointer after usage.
 * @details Depending on the current game mode e.g. Laser, Grenade, etc. this function returns a pointer to a
 * 			string with the name of specified mode. This is needed to constuct sqlite queries with the mode specific tables
 * 			in the database.
 * @return name of current game mode.
 */
const char* CGameController_zCatch::GetGameModeTableName(int GameMode) {
	const char* aBuf;

	if (!GameMode) {
		switch (m_OldGameMode) {
		case 1: return aBuf = "Laser"; break;
		case 2: return aBuf = "Everything"; break;
		case 3: return aBuf = "Hammer"; break;
		case 4: return aBuf = "Grenade"; break;
		case 5: return aBuf = "Ninja"; break;
		default: return aBuf = "zCatch"; break;
			return aBuf;
		}
	} else {
		switch (GameMode) {
		case 1: return aBuf = "Laser"; break;
		case 2: return aBuf = "Everything"; break;
		case 3: return aBuf = "Hammer"; break;
		case 4: return aBuf = "Grenade"; break;
		case 5: return aBuf = "Ninja"; break;
		default: return aBuf = "zCatch"; break;
			return aBuf;
		}
	}
}

















