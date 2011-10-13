/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* zCatch by erd and Teetime */

#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include "zcatch.hpp"

CGameController_zCatch::CGameController_zCatch(class CGameContext *pGameServer) 
: IGameController(pGameServer)
{
	m_pGameType = "zCatch";
}

void CGameController_zCatch::Tick()
{
	DoWincheck();
	IGameController::Tick();
}
bool CGameController_zCatch::IsZCatch()
{
	return true;
}

int CGameController_zCatch::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	int client_id =  pVictim->GetPlayer()->GetCID();
	char buf[256];
	if(pKiller !=  pVictim->GetPlayer())
	{
		pKiller->m_Kills++;
		pVictim->GetPlayer()->m_Deaths++; 
		
		pKiller->m_Score++;
		/* Check if the killer is already killed and in spectator (victim may died through wallshot) */
		if(pKiller->GetTeam() != TEAM_SPECTATORS)
		{
			pVictim->GetPlayer()->m_CatchedBy = pKiller->GetCID();
			pVictim->GetPlayer()->SetTeamDirect(TEAM_SPECTATORS);
		
			if(pVictim->GetPlayer()->m_PlayerWantToFollowCatcher)
				pVictim->GetPlayer()->m_SpectatorID = pKiller->GetCID(); // Let the victim follow his catcher
		
			str_format(buf, sizeof(buf), "Caught by \"%s\". You will join the game automatically when \"%s\" dies.", Server()->ClientName(pKiller->GetCID()), Server()->ClientName(pKiller->GetCID()));	
			GameServer()->SendChatTarget(client_id, buf);
		}
	}
	
	for(int i=0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			if(GameServer()->m_apPlayers[i]->m_CatchedBy == client_id)
			{
				GameServer()->m_apPlayers[i]->m_CatchedBy = ZCATCH_NOT_CATCHED;
				GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));
				
				GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[i]);
				if(pKiller != pVictim->GetPlayer())
					pKiller->m_Score++;
			}
		}
	}
	return 0;
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
	m_UnbalancedTick = -1;
	m_ForceBalanced = false;
	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{		
			GameServer()->m_apPlayers[i]->m_CatchedBy = ZCATCH_NOT_CATCHED;
			GameServer()->m_apPlayers[i]->m_Kills = 0;
			GameServer()->m_apPlayers[i]->m_Deaths = 0;
			GameServer()->m_apPlayers[i]->m_TicksSpec = 0;
			GameServer()->m_apPlayers[i]->m_TicksIngame = 0;
			GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[i]);
		}
	}
	char aBufMsg[256];
	str_format(aBufMsg, sizeof(aBufMsg), "start round type='%s' teamplay='%d'", m_pGameType, m_GameFlags&GAMEFLAG_TEAMS);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBufMsg);
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
			case 0:
				pChr->GiveWeapon(WEAPON_HAMMER, -1);
				pChr->GiveWeapon(WEAPON_GUN, 10);
				break;
			case 1:
				pChr->GiveWeapon(WEAPON_RIFLE, -1);
				break;
			case 2:
				pChr->GiveWeapon(WEAPON_HAMMER, -1);
				pChr->GiveWeapon(WEAPON_GUN, -1);
				pChr->GiveWeapon(WEAPON_GRENADE, -1);
				pChr->GiveWeapon(WEAPON_SHOTGUN, -1);
				pChr->GiveWeapon(WEAPON_RIFLE, -1);
				break;
			case 3:
				pChr->GiveWeapon(WEAPON_HAMMER, -1);
				break;
			case 4:
				pChr->GiveWeapon(WEAPON_GRENADE, -1);
				break;
			}
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
				GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[i]);
				
				char abuf[128];
				str_format(abuf, sizeof(abuf), "Kills: %d | Deaths: %d", GameServer()->m_apPlayers[i]->m_Kills, GameServer()->m_apPlayers[i]->m_Deaths);				
				GameServer()->SendChatTarget(i, abuf);
				
				if(GameServer()->m_apPlayers[i]->m_TicksSpec != 0 || GameServer()->m_apPlayers[i]->m_TicksIngame != 0)
				{
					double TimeInSpec = (GameServer()->m_apPlayers[i]->m_TicksSpec*100.0) / (GameServer()->m_apPlayers[i]->m_TicksIngame + GameServer()->m_apPlayers[i]->m_TicksSpec);
					str_format(abuf, sizeof(abuf), "Spec: %.2f%% | Ingame: %.2f%%", (double)TimeInSpec, (double)(100.0 - TimeInSpec));
					GameServer()->SendChatTarget(i, abuf);	
				}
				GameServer()->m_apPlayers[i]->m_CatchedBy = ZCATCH_NOT_CATCHED; //Set all players in server as non-catched
			}
		}
	}

	if(m_Warmup) // game can't end when we are running warmup
		return;

	GameServer()->m_World.m_Paused = true;
	m_GameOverTick = Server()->Tick();
	m_SuddenDeath = 0;
}
