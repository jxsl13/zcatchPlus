/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* zCatch by erd and Teetime */

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include "zcatch.h"

CGameController_zCatch::CGameController_zCatch(class CGameContext *pGameServer) 
: IGameController(pGameServer)
{
	m_pGameType = "zCatch";
	m_OldMode = g_Config.m_SvMode;
}

void CGameController_zCatch::Tick()
{
	DoWincheck();
	IGameController::Tick();

	if(m_OldMode != g_Config.m_SvMode)
	{
		Server()->MapReload();
		m_OldMode = g_Config.m_SvMode;
	}
}

bool CGameController_zCatch::IsZCatch()
{
	return true;
}

void CGameController_zCatch::DoWincheck()
{
	int Players = 0, Players_Spec = 0, Players_SpecExplicit = 0;	

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			Players++;
			if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
				Players_Spec++;
			if(GameServer()->m_apPlayers[i]->m_SpecExplicit == 1)
				Players_SpecExplicit++;
		}
	}

	if(Players == 1)
	{
		//Do nothing
	}
	else if((Players - Players_Spec == 1) && (Players != Players_Spec) && (Players - Players_SpecExplicit != 1)) 
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				GameServer()->m_apPlayers[i]->m_Score += g_Config.m_SvBonus;
		}
		EndRound();
	}

	IGameController::DoWincheck(); //do also usual wincheck
}

int CGameController_zCatch::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	int VictimID =  pVictim->GetPlayer()->GetCID();
	char aBuf[256];
	if(pKiller !=  pVictim->GetPlayer())
	{
		pKiller->m_Kills++;
		pVictim->GetPlayer()->m_Deaths++; 

		pKiller->m_Score++;

		/* Check if the killer is already killed and in spectator (victim may died through wallshot) */
		if(pKiller->GetTeam() != TEAM_SPECTATORS)
		{
			pVictim->GetPlayer()->m_CaughtBy = pKiller->GetCID();
			pVictim->GetPlayer()->SetTeamDirect(TEAM_SPECTATORS);

			if(pVictim->GetPlayer()->m_PlayerWantToFollowCatcher)
				pVictim->GetPlayer()->m_SpectatorID = pKiller->GetCID(); // Let the victim follow his catcher

			str_format(aBuf, sizeof(aBuf), "Caught by \"%s\". You will join the game automatically when \"%s\" dies.", Server()->ClientName(pKiller->GetCID()), Server()->ClientName(pKiller->GetCID()));
			GameServer()->SendChatTarget(VictimID, aBuf);
		}
	}
	else
	{
		//Punish selfkill/death
		if(WeaponID == WEAPON_SELF || WeaponID == WEAPON_WORLD)
			pVictim->GetPlayer()->m_Score -= g_Config.m_SvKillPenalty;
	}

	for(int i=0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			if(GameServer()->m_apPlayers[i]->m_CaughtBy == VictimID)
			{
				GameServer()->m_apPlayers[i]->m_CaughtBy = ZCATCH_NOT_CAUGHT;
				GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));
				
				if(pKiller != pVictim->GetPlayer())
					pKiller->m_Score++;
			}
		}
	}

	// Update colors
	OnPlayerInfoChange(pKiller);
	OnPlayerInfoChange(pVictim->GetPlayer());

	return 0;
}

void CGameController_zCatch::OnPlayerInfoChange(class CPlayer *pP)
{
	if(g_Config.m_SvColorIndicator)
	{
		int Num = 161;
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_CaughtBy == pP->GetCID())
				Num -= 10;
		pP->m_TeeInfos.m_ColorBody = Num * 0x010000 + 0xff00;
		pP->m_TeeInfos.m_ColorFeet = Num * 0x010000 + 0xff00;
		pP->m_TeeInfos.m_UseCustomColor = 1;
	}
}

void CGameController_zCatch::StartRound()
{
	ResetGame();

	m_RoundStartTick = Server()->Tick();
	m_SuddenDeath = 0;
	m_GameOverTick = -1;
	GameServer()->m_World.m_Paused = false;
	m_aTeamscore[TEAM_RED] = 0;
	m_aTeamscore[TEAM_BLUE] = 0;
	m_ForceBalanced = false;
	Server()->DemoRecorder_HandleAutoStart();
	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{		
			GameServer()->m_apPlayers[i]->m_CaughtBy = ZCATCH_NOT_CAUGHT;
			GameServer()->m_apPlayers[i]->m_Kills = 0;
			GameServer()->m_apPlayers[i]->m_Deaths = 0;
			GameServer()->m_apPlayers[i]->m_TicksSpec = 0;
			GameServer()->m_apPlayers[i]->m_TicksIngame = 0;
		}
	}
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "start round type='%s' teamplay='%d'", m_pGameType, m_GameFlags&GAMEFLAG_TEAMS);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
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
				pChr->GiveWeapon(WEAPON_GUN, 6);
				pChr->GiveWeapon(WEAPON_GRENADE, 6);
				pChr->GiveWeapon(WEAPON_SHOTGUN, 6);
				pChr->GiveWeapon(WEAPON_RIFLE, 6);
				break;
			case 3: /* Hammer */
				pChr->GiveWeapon(WEAPON_HAMMER, -1);
				break;
			case 4: /* Grenade */
				pChr->GiveWeapon(WEAPON_GRENADE, g_Config.m_SvGrenadeBullets);
				break;
			case 5: /* Ninja */
				pChr->GiveNinja();
				break;
		}

	//Update color of spawning tees
	OnPlayerInfoChange(pChr->GetPlayer());
}

void CGameController_zCatch::EndRound()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{

			if(GameServer()->m_apPlayers[i]->m_SpecExplicit == 0)
			{
				GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));
				
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Kills: %d | Deaths: %d", GameServer()->m_apPlayers[i]->m_Kills, GameServer()->m_apPlayers[i]->m_Deaths);
				GameServer()->SendChatTarget(i, aBuf);

				if(GameServer()->m_apPlayers[i]->m_TicksSpec != 0 || GameServer()->m_apPlayers[i]->m_TicksIngame != 0)
				{
					double TimeInSpec = (GameServer()->m_apPlayers[i]->m_TicksSpec*100.0) / (GameServer()->m_apPlayers[i]->m_TicksIngame + GameServer()->m_apPlayers[i]->m_TicksSpec);
					str_format(aBuf, sizeof(aBuf), "Spec: %.2f%% | Ingame: %.2f%%", (double)TimeInSpec, (double)(100.0 - TimeInSpec));
					GameServer()->SendChatTarget(i, aBuf);
				}
				GameServer()->m_apPlayers[i]->m_CaughtBy = ZCATCH_NOT_CAUGHT; //Set all players in server as non-caught
			}
		}
	}

	if(m_Warmup) // game can't end when we are running warmup
		return;

	GameServer()->m_World.m_Paused = true;
	m_GameOverTick = Server()->Tick();
	m_SuddenDeath = 0;
}
