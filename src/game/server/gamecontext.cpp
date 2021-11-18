/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* Modified by Teelevision for zCatch/TeeVi, see readme.txt and license.txt.                 */
#include <new>
#include <iomanip>
#include <sstream>
#include <bitset>
#include <base/math.h>
#include <engine/shared/config.h>
#include <engine/map.h>
#include <engine/console.h>
#include <engine/storage.h>
#include "gamecontext.h"
#include <game/version.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/server/player.h>
/*#include "gamemodes/dm.h"
#include "gamemodes/tdm.h"
#include "gamemodes/ctf.h"
#include "gamemodes/mod.h"*/
#include "gamemodes/zcatch.h"


// needed for server.h include to access
// server stuff like admin status etc..
#include <engine/masterserver.h>
#include <engine/shared/demo.h>
#include <engine/shared/econ.h>
#include <engine/shared/mapchecker.h>
#include <engine/shared/netban.h>
#include <engine/shared/network.h>
#include <engine/shared/snapshot.h>
#include <engine/server/register.h>
#include <engine/server/server.h>
#include <algorithm>
#include <cmath>





enum
{
	RESET,
	NO_RESET
};

void CGameContext::Construct(int Resetting)
{
	m_Resetting = 0;
	m_pServer = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
		m_apPlayers[i] = 0;

	m_pController = 0;
	m_VoteCloseTime = 0;
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;
	m_LockTeams = 0;

	if (Resetting == NO_RESET)
	{
		m_pVoteOptionHeap = new CHeap();

		/* ranking system */
		m_RankingDb = NULL;
	}

	for (int i = 0; i < MAX_MUTES; i++)
		m_aMutes[i].m_aIP[0] = 0;

	// zCatch/TeeVi: hard mode
	m_HardModes.clear();
	m_HardModes.push_back({"ammo210", false, true});
	m_HardModes.push_back({"ammo15", false, true});
	m_HardModes.push_back({"overheat", true, true});
	m_HardModes.push_back({"hookkill", true, true});
	m_HardModes.push_back({"fail0", false, true});
	m_HardModes.push_back({"fail3", false, true});
	m_HardModes.push_back({"5s", true, true});
	m_HardModes.push_back({"10s", true, true});
	m_HardModes.push_back({"20s", true, true});
	m_HardModes.push_back({"double", true, true});
}

CGameContext::CGameContext(int Resetting)
{
	Construct(Resetting);
}

CGameContext::CGameContext()
{
	Construct(NO_RESET);
}

CGameContext::~CGameContext()
{


	for (int i = 0; i < MAX_CLIENTS; i++)
		delete m_apPlayers[i];
	delete m_BotDetection;
	WaitForFutures();
	if (!m_Resetting)
	{
		delete m_pVoteOptionHeap;

		/* close ranking db */
		if (RankingEnabled())
		{
			sqlite3_close(m_RankingDb);
		}
	}
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOptionServer *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOptionServer *pVoteOptionLast = m_pVoteOptionLast;
	int NumVoteOptions = m_NumVoteOptions;
	CTuningParams Tuning = m_Tuning;
	sqlite3 *rankingDb = m_RankingDb;

	m_Resetting = true;
	this->~CGameContext();
	mem_zero(this, sizeof(*this));
	new (this) CGameContext(RESET);

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_NumVoteOptions = NumVoteOptions;
	m_Tuning = Tuning;
	m_RankingDb = rankingDb;
}


/**
 * @brief teehistorian write function.
 * @details [long description]
 *
 * @param pData [description]
 * @param DataSize [description]
 * @param pUser [description]
 */
void CGameContext::TeeHistorianWrite(const void *pData, int DataSize, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;
	aio_write(pSelf->m_TeeHistorian.GetHistorianFile(), pData, DataSize);

}

void CGameContext::CommandCallback(int ClientID, int FlagMask, const char *pCmd, IConsole::IResult *pResult, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;

	// teehistorian stuff
	pSelf->m_TeeHistorian.RecordConsoleCommand(pSelf->Server()->ClientName(ClientID), ClientID, FlagMask, pCmd, pResult);
}


class CCharacter *CGameContext::GetPlayerChar(int ClientID)
{
	if (ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return 0;
	return m_apPlayers[ClientID]->GetCharacter();
}

void CGameContext::CreateDamageInd(vec2 Pos, float Angle, int Amount)
{
	float a = 3 * 3.14159f / 2 + Angle;
	//float a = get_angle(dir);
	float s = a - pi / 3;
	float e = a + pi / 3;
	for (int i = 0; i < Amount; i++)
	{
		float f = mix(s, e, float(i + 1) / float(Amount + 2));
		CNetEvent_DamageInd *pEvent = (CNetEvent_DamageInd *)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(CNetEvent_DamageInd));
		if (pEvent)
		{
			pEvent->m_X = (int)Pos.x;
			pEvent->m_Y = (int)Pos.y;
			pEvent->m_Angle = (int)(f * 256.0f);
		}
	}
}

void CGameContext::CreateHammerHit(vec2 Pos)
{
	// create the event
	CNetEvent_HammerHit *pEvent = (CNetEvent_HammerHit *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit));
	if (pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}


void CGameContext::CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, bool limitVictims, const bool *victims, int ownerLastDieTickBeforceFiring)
{
	// create the event
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion));
	if (pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	if (!NoDamage)
	{
		// deal damage
		CCharacter *apEnts[MAX_CLIENTS];
		float Radius = 135.0f;
		float InnerRadius = 48.0f;
		int Num = m_World.FindEntities(Pos, Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		bool someoneWasHit = false;
		for (int i = 0; i < Num; i++)
		{
			if (!limitVictims || victims[apEnts[i]->GetPlayer()->GetCID()])
			{
				vec2 Diff = apEnts[i]->m_Pos - Pos;
				vec2 ForceDir(0, 1);
				float l = length(Diff);
				if (l)
					ForceDir = normalize(Diff);
				l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
				float Dmg = 6 * l;
				if ((int)Dmg)
				{
					apEnts[i]->m_KillerLastDieTickBeforceFiring = ownerLastDieTickBeforceFiring;
					apEnts[i]->TakeDamage(ForceDir * Dmg * 2, (int)Dmg, Owner, Weapon);
					someoneWasHit = true;
				}
			}
		}
		// give the owner the ammo back if he hit someone else or himself (like rocketjump)
		if (someoneWasHit)
		{
			CCharacter *ownerChar = GetPlayerChar(Owner);
			if (ownerChar)
				ownerChar->GiveAmmo(Weapon, 1);
		}
		else
		{
			// zCatch/TeeVi: hard mode
			auto ownerChar = GetPlayerChar(Owner);
			if (ownerChar && ownerLastDieTickBeforceFiring == ownerChar->GetPlayer()->m_DieTick)
				ownerChar->GetPlayer()->HardModeFailedShot();
		}
	}
}

/*
void create_smoke(vec2 Pos)
{
	// create the event
	EV_EXPLOSION *pEvent = (EV_EXPLOSION *)events.create(EVENT_SMOKE, sizeof(EV_EXPLOSION));
	if(pEvent)
	{
		pEvent->x = (int)Pos.x;
		pEvent->y = (int)Pos.y;
	}
}*/

void CGameContext::CreatePlayerSpawn(vec2 Pos)
{
	// create the event
	CNetEvent_Spawn *ev = (CNetEvent_Spawn *)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn));
	if (ev)
	{
		ev->m_X = (int)Pos.x;
		ev->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateDeath(vec2 Pos, int ClientID)
{
	// create the event
	CNetEvent_Death *pEvent = (CNetEvent_Death *)m_Events.Create(NETEVENTTYPE_DEATH, sizeof(CNetEvent_Death));
	if (pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientID = ClientID;
	}
}

void CGameContext::CreateSound(vec2 Pos, int Sound, int Mask)
{
	if (Sound < 0)
		return;

	// create a sound
	CNetEvent_SoundWorld *pEvent = (CNetEvent_SoundWorld *)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), Mask);
	if (pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_SoundID = Sound;
	}
}

void CGameContext::CreateSoundGlobal(int Sound, int Target)
{
	if (Sound < 0)
		return;

	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundID = Sound;
	if (Target == -2)
		Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, -1);
	else
	{
		int Flag = MSGFLAG_VITAL;
		if (Target != -1)
			Flag |= MSGFLAG_NORECORD;
		Server()->SendPackMsg(&Msg, Flag, Target);
	}
}


void CGameContext::SendChatTarget(int To, const char *pText)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_ClientID = -1;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
}


void CGameContext::SendPrivateMessage(int From, int To, const char *pText)
{
	// prepare message
	CNetMsg_Sv_Chat M;
	M.m_Team = CHAT_SPEC;
	M.m_ClientID = From;
	M.m_pMessage = pText;

	Server()->SendPackMsg(&M, MSGFLAG_VITAL, From);
	Server()->SendPackMsg(&M, MSGFLAG_VITAL, To);
}


void CGameContext::SendChat(int ChatterClientID, int Team, const char *pText)
{
	char aBuf[256];
	if (ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, Team, Server()->ClientName(ChatterClientID), pText);
	else
		str_format(aBuf, sizeof(aBuf), "*** %s", pText);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, Team != CHAT_ALL ? "teamchat" : "chat", aBuf);

	if (Team == CHAT_ALL)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = pText;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	}
	else
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 1;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = pText;

		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);

		// send to the clients
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			//if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() == Team)
			if (m_apPlayers[i] && ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS && m_apPlayers[ChatterClientID] && m_apPlayers[ChatterClientID]->m_SpecExplicit == m_apPlayers[i]->m_SpecExplicit)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
}

void CGameContext::SendEmoticon(int ClientID, int Emoticon)
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Emoticon = Emoticon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::SendWeaponPickup(int ClientID, int Weapon)
{
	CNetMsg_Sv_WeaponPickup Msg;
	Msg.m_Weapon = Weapon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}


void CGameContext::SendBroadcast(const char *pText, int ClientID)
{
	CNetMsg_Sv_Broadcast Msg;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

//
void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason)
{
	// check if a vote is already running
	if (m_VoteCloseTime)
		return;

	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i])
		{
			m_apPlayers[i]->m_Vote = 0;
			m_apPlayers[i]->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq() * 25;
	str_copy(m_aVoteDescription, pDesc, sizeof(m_aVoteDescription));
	str_copy(m_aVoteCommand, pCommand, sizeof(m_aVoteCommand));
	str_copy(m_aVoteReason, pReason, sizeof(m_aVoteReason));
	SendVoteSet(-1);
	m_VoteUpdate = true;
}


void CGameContext::EndVote()
{
	m_VoteCloseTime = 0;
	SendVoteSet(-1);
}

void CGameContext::SendVoteSet(int ClientID)
{
	CNetMsg_Sv_VoteSet Msg;
	if (m_VoteCloseTime)
	{
		Msg.m_Timeout = (m_VoteCloseTime - time_get()) / time_freq();
		Msg.m_pDescription = m_aVoteDescription;
		Msg.m_pReason = m_aVoteReason;
	}
	else
	{
		Msg.m_Timeout = 0;
		Msg.m_pDescription = "";
		Msg.m_pReason = "";
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendVoteStatus(int ClientID, int Total, int Yes, int No)
{
	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes + No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

}

void CGameContext::AbortVoteKickOnDisconnect(int ClientID)
{
	if (m_VoteCloseTime && ((!str_comp_num(m_aVoteCommand, "kick ", 5) && str_toint(&m_aVoteCommand[5]) == ClientID) ||
	                        (!str_comp_num(m_aVoteCommand, "set_team ", 9) && str_toint(&m_aVoteCommand[9]) == ClientID)))
		m_VoteCloseTime = -1;
}


void CGameContext::CheckPureTuning()
{
	// might not be created yet during start up
	if (!m_pController)
		return;

	if (	str_comp(m_pController->m_pGameType, "DM") == 0 ||
	        str_comp(m_pController->m_pGameType, "TDM") == 0 ||
	        str_comp(m_pController->m_pGameType, "CTF") == 0)
	{
		CTuningParams p;
		if (mem_comp(&p, &m_Tuning, sizeof(p)) != 0)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "resetting tuning due to pure server");
			m_Tuning = p;
		}
	}
}

void CGameContext::SendTuningParams(int ClientID)
{
	CheckPureTuning();

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int *pParams = (int *)&m_Tuning;
	for (unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
		Msg.AddInt(pParams[i]);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SwapTeams()
{
	if (!m_pController->IsTeamplay())
		return;

	SendChat(-1, CGameContext::CHAT_ALL, "Teams were swapped");

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			m_apPlayers[i]->SetTeam(m_apPlayers[i]->GetTeam() ^ 1, false);
	}

	(void)m_pController->CheckTeamBalance();
}

void CGameContext::OnTick()
{
	// check tuning
	CheckPureTuning();


	/*teehistorian*/
	if (m_TeeHistorian.GetMode())
	{

		if (m_TeeHistorian.GetMode() == CTeeHistorian::MODE_SQLITE)
		{

		} else if (m_TeeHistorian.GetMode() == CTeeHistorian::MODE_TEE_HISTORIAN) {
			int Error = aio_error(m_TeeHistorian.GetHistorianFile());
			if (Error)
			{
				dbg_msg("teehistorian", "error writing to file, err=%d", Error);
				Server()->SetErrorShutdown("teehistorian io error");
			}
		}

		if (!m_TeeHistorian.Starting())
		{
			m_TeeHistorian.EndInputs();
			m_TeeHistorian.EndTick();
		}
		m_TeeHistorian.BeginTick(Server()->Tick());
		m_TeeHistorian.BeginPlayers();
	}


	/*teehistorian end*/

	// copy tuning
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.Tick();

	//if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	/*teehistorian*/
	if (m_TeeHistorian.GetMode())
	{
		int players = 0;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{

			if (m_apPlayers[i] && m_apPlayers[i]->GetCharacter())
			{
				players++;
				CNetObj_CharacterCore Char;
				m_apPlayers[i]->GetCharacter()->GetCore().Write(&Char);
				m_TeeHistorian.RecordPlayer(Server()->ClientJoinHash(i), Server()->ClientName(i), i, &Char);
			}
			else
			{
				if (m_TeeHistorian.GetMode() == CTeeHistorian::MODE_TEE_HISTORIAN)
				{
					m_TeeHistorian.RecordDeadPlayer(i);
				}

			}
		}


		m_TeeHistorian.EndPlayers();
		m_TeeHistorian.BeginInputs();
	}
	m_TeeHistorian.CheckHistorianModeToggled();
	/*teehistorian end*/


	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i])
		{
			m_apPlayers[i]->Tick();
			m_apPlayers[i]->PostTick();

			/*bot detection*/
			if (g_Config.m_SvBotDetection && m_apPlayers[i]->GetCharacter())
			{
				CNetObj_CharacterCore Char;
				m_apPlayers[i]->GetCharacter()->GetCore().Write(&Char);
				m_BotDetection->AddPlayerCore(i, Server()->ClientJoinHash(i), &Char);
			}
			/*bot detection*/

			// ########## CPlayer & CClient sync
			m_apPlayers[i]->SetAfkTime(Server()->Tick() - GetBanServer()->Server()->m_aClients[i].m_LastInputTick);
			// ##########
		}
	}

	/*bot detection*/
	if (g_Config.m_SvBotDetection)
	{
		m_BotDetection->OnTick();
	}
	/*bot detection*/

	// update voting
	if (m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if (m_VoteCloseTime == -1)
		{
			SendChat(-1, CGameContext::CHAT_ALL, "Vote aborted");
			EndVote();
		}
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if (m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
				for (int i = 0; i < MAX_CLIENTS; i++)
					if (m_apPlayers[i])
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
				bool aVoteChecked[MAX_CLIENTS] = {0};

				for (int i = 0; i < MAX_CLIENTS; i++)
				{
					/* zCatch - Allow voting from players in spectators (needed or the last 2 players ingame can kick the whole server),
					 * but deny votes from players who are explicit in spec
					*/
					if (!m_apPlayers[i] || m_apPlayers[i]->m_SpecExplicit || aVoteChecked[i])	// don't count in votes by spectators
						continue;

					int ActVote = m_apPlayers[i]->m_Vote;
					int ActVotePos = m_apPlayers[i]->m_VotePos;

					// check for more players with the same ip (only use the vote of the one who voted first)
					for (int j = i + 1; j < MAX_CLIENTS; ++j)
					{
						if (!m_apPlayers[j] || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]))
							continue;

						aVoteChecked[j] = true;
						if (m_apPlayers[j]->m_Vote && (!ActVote || ActVotePos > m_apPlayers[j]->m_VotePos))
						{
							ActVote = m_apPlayers[j]->m_Vote;
							ActVotePos = m_apPlayers[j]->m_VotePos;
						}
					}

					Total++;
					if (ActVote > 0)
						Yes++;
					else if (ActVote < 0)
						No++;
				}

				if (Yes >= Total / 2 + 1)
					m_VoteEnforce = VOTE_ENFORCE_YES;
				else if (No >= (Total + 1) / 2)
					m_VoteEnforce = VOTE_ENFORCE_NO;
			}

			if (m_VoteEnforce == VOTE_ENFORCE_YES)
			{
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				EndVote();
				SendChat(-1, CGameContext::CHAT_ALL, "Vote passed");

				if (m_apPlayers[m_VoteCreator])
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;
			}
			else if (m_VoteEnforce == VOTE_ENFORCE_NO || time_get() > m_VoteCloseTime)
			{
				EndVote();
				SendChat(-1, CGameContext::CHAT_ALL, "Vote failed");
			}
			else if (m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}

	// info messages
	// execute if interval is given and message interval is due, respecting the pause
	if (Server()->GetInfoTextInterval() > 0
	        && ((Server()->Tick() % Server()->GetInfoTextInterval()) - Server()->GetInfoTextIntervalPause()) % Server()->GetInfoTextMsgInterval() == 0)
	{
		SendChat(-1, CGameContext::CHAT_ALL, Server()->GetNextInfoText().c_str());
	}


	// may cause performance issues otherwise.
	// check once every second.
	if (Server()->Tick() % Server()->TickSpeed() == 0) {

		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (m_apPlayers[i]) {

				CPlayer *p = m_apPlayers[i];
				// these three are directly linkes to the ban urgency levels,
				// thus need to be executed first before checking the ban urgency.
				p->GeneralClientCheck();
				p->IsBot(); // update bot stuff
				p->IsZoom(); // update zoom stuff
				// update or player stuff.

				std::string clientDescription =  p->GetCurrentClientDescription();
				int clientBanUrgency = p->GetClientBanUrgency();

				// players are always informed about a player using a weird client,
				// if the interval is set to a valid value > 0
				if (g_Config.m_SvCheatClientMentionInterval > 0 && // interval has got to be positive otherwise mentioning is turned off
				clientBanUrgency >= CPlayer::URGENCY_LEVEL_CHEAT_CLIENT &&
				(p->GetLastClientMentionedTick() < 0 	// if the server does not run as long as the mention interval needed to pass before
														// the message is sent, this case is used to directly write the message.
				|| Server()->Tick() >= ( p->GetLastClientMentionedTick() + (g_Config.m_SvCheatClientMentionInterval * Server()->TickSpeed()) )
				))
				{
					// send client description to all the players on the server.
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "Player '%s' uses an unusual client: %s", Server()->ClientName(i), clientDescription.c_str());
					SendChat(-1, CGameContext::CHAT_ALL, aBuf);

					// do not forget to update the last mention tick,
					// otherwise you will get lots of spam!
					p->UpdateLastClientMentionedTick();

				}
				// if urgency level is set to 0 = CPlayer::URGENCY_LEVEL_NORMAL_PLAYER,
				// banning is being disabled
				if (g_Config.m_SvAutomaticBanMinUrgencyLevel > CPlayer::URGENCY_LEVEL_NORMAL_PLAYER)
				{
					BanIf(g_Config.m_SvAutomaticBanMinUrgencyLevel <= clientBanUrgency, // if ban urgency at least what the level stated in the server config
						i,
						g_Config.m_SvAutomaticBanTime,
						clientDescription);
				}
			}
		}

	}

	// basic bot banning stuff

	// bot detection
	// it is based on the behaviour of some bots to shoot at a player's _exact_ position
	// check each player, check only if an admin is online
	// old bot detection
	/*
	if(g_Config.m_SvBotDetection && Server()->GetNumLoggedInAdmins())
	{
		char aBuf[128], bBuf[64];
		const vec2 *pos, *posVictim;
		float d, precision;
		CCharacter *ci, *cj;
		CPlayer *p;

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{

			// abort if player is not ingame or already detected as a bot
			if(!(p = m_apPlayers[i]) || p->m_IsAimBot || !(ci = GetPlayerChar(i)))
				continue;

			// check against every other player
			for(int j = 0; j < MAX_CLIENTS; ++j)
			{

				if(j != i && (cj = GetPlayerChar(j)))
				{
					int indexAdd = 0;
					vec2 target(p->m_LatestActivity.m_TargetX, p->m_LatestActivity.m_TargetY);

					// fast aiming bot detection
					if(g_Config.m_SvBotDetection&BOT_DETECTION_FAST_AIM
						&& p->m_AimBotTargetSpeed > 300.0 // only fast movements
						&& (d = cj->HowCloseToXRecently(ci->m_Pos + target, posVictim, p->m_AimBotLastDetection)) < 16.0
						&& (precision = p->m_AimBotTargetSpeed * (256.0 - d * d)) >= 50000.0
						&& !( // don't detect same constellation twice
							ci->m_Pos == p->m_AimBotLastDetectionPos
							&& *posVictim == p->m_AimBotLastDetectionPosVictim
						)
					)//if
					{
						indexAdd = min(3, (int)(precision / 50000));
						p->m_AimBotLastDetectionPos = ci->m_Pos;
						// prepare console output
						str_format(bBuf, sizeof(bBuf), "precision=%d speed=%d distance=%d", (int)precision, (int)p->m_AimBotTargetSpeed, (int)d);
					}

					// follow bot detection
					else if(g_Config.m_SvBotDetection&BOT_DETECTION_FOLLOW
						&& cj->NetworkClipped(i) == 0 // needs to be in sight
						&& ci->AimedAtCharRecently(target, cj, pos, posVictim, p->m_AimBotLastDetection)
						&& !( // don't detect same constellation twice
							*pos == p->m_AimBotLastDetectionPos
							&& *posVictim == p->m_AimBotLastDetectionPosVictim
						)
						// don't detect horizontal dragging
						&& !(
							pos->y == p->m_AimBotLastDetectionPos.y
							&& posVictim->y == p->m_AimBotLastDetectionPosVictim.y
						)
					)//if
					{
						indexAdd = 1;
						p->m_AimBotLastDetectionPos = *pos;
						// prepare console output
						bBuf[0] = 0;
					}

					// detected
					if(indexAdd > 0)
					{
						p->m_AimBotLastDetection = Server()->Tick();
						p->m_AimBotLastDetectionPosVictim = *posVictim;
						p->m_AimBotIndex += indexAdd;
						p->m_AimBotRange = max(p->m_AimBotRange, (int)length(target));
						// log to console
						str_format(aBuf, sizeof(aBuf), "player=%d victim=%d index=%d %s", i, j, p->m_AimBotIndex, bBuf);
						Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "botdetect", aBuf);
						// don't check other players
						break;
					}

				}
			}

			// check if threshold is exceeded
			if(p->m_AimBotIndex >= 5)
			{
				p->m_IsAimBot = Server()->Tick();
				// alert the admins
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "+++ '%s' (id=%d,range=%d) might be botting +++", Server()->ClientName(i), i, p->m_AimBotRange);
				for(int j = 0; j < MAX_CLIENTS; ++j)
					if(Server()->IsAuthed(j))
						SendChatTarget(j, aBuf);
			}

			// reduce once every seconds (tolerance)
			if(((Server()->Tick() % Server()->TickSpeed()) == 0) && p->m_AimBotIndex)
			{
				if(!(--p->m_AimBotIndex))
				{
					p->m_AimBotRange = 0;
				}
			}
		}
	}
	*/

#ifdef CONF_DEBUG
	if (g_Config.m_DbgDummies)
	{
		for (int i = 0; i < g_Config.m_DbgDummies ; i++)
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i & 1) ? -1 : 1;
			m_apPlayers[MAX_CLIENTS - i - 1]->OnPredictedInput(&Input);
		}
	}
#endif

}

// Server hooks
void CGameContext::OnClientDirectInput(const char* ClientNick, int ClientID, void *pInput)
{
	if (!m_World.m_Paused)
		m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput *)pInput);

	/*teehistorian*/
	if (m_TeeHistorian.GetMode())
	{
		m_TeeHistorian.RecordPlayerInput(Server()->ClientJoinHash(ClientID), ClientNick, ClientID, (CNetObj_PlayerInput *)pInput);
	}
	if (g_Config.m_SvBotDetection)
	{
		m_BotDetection->AddPlayerInput(ClientID, Server()->ClientJoinHash(ClientID), (CNetObj_PlayerInput *)pInput);
	}
}

void CGameContext::OnClientPredictedInput(int ClientID, void *pInput)
{
	if (!m_World.m_Paused)
		m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput *)pInput);
}


void CGameContext::OnClientEnter(int ClientID)
{

	CPlayer *p = m_apPlayers[ClientID];
	//world.insert_entity(&players[client_id]);

	p->Respawn();

	/* begin zCatch */
	CPlayer *leader = NULL;

	int NumReady = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (IsClientReady(i))
			NumReady++;
	}

	// sv_allow_join 1: Allow new players to join the game without need to wait for the next round
	if (g_Config.m_SvAllowJoin == 1)
	{
		p->m_SpecExplicit = (NumReady > 2);
		p->SetTeamDirect(p->m_SpecExplicit ? TEAM_SPECTATORS : m_pController->ClampTeam(1));

		if (g_Config.m_SvLastStandingDeathmatch) {
			SendBroadcast("We are currently playing the Release Game. You can join the game!", ClientID);
		} else {
			SendBroadcast("You can join the game", ClientID);
		}
	}
	// sv_allow_join 0: The player will join when the player with the most kills dies
	else if (g_Config.m_SvAllowJoin == 0)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
			if (m_apPlayers[i] && ((leader && m_apPlayers[i]->m_zCatchNumKillsInARow > leader->m_zCatchNumKillsInARow) || (!leader && m_apPlayers[i]->m_zCatchNumKillsInARow)))
				leader = m_apPlayers[i];
		if (leader)
			leader->AddZCatchVictim(ClientID, CPlayer::ZCATCH_CAUGHT_REASON_JOINING);
		else
			p->m_SpecExplicit = false;
	}


	/* end zCatch */

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientID), m_pController->GetTeamName(p->GetTeam()));
	SendChat(-1, CGameContext::CHAT_ALL, aBuf);


	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, Server()->ClientName(ClientID), p->GetTeam());
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	m_VoteUpdate = true;

	/* zCatch begin */
	SendChatTarget(ClientID, "Welcome to zCatch! Type /info for more.");
	if (g_Config.m_SvAllowJoin == 0 && leader)
	{
		char buf[128];
		str_format(buf, sizeof(buf), "You will join the game when '%s' dies", Server()->ClientName(leader->GetCID()));
		SendChatTarget(ClientID, buf);
	}
	/* zCatch end */
}

void CGameContext::OnClientConnected(int ClientID)
{
	// Check which team the player should be on
	const int StartTeam = g_Config.m_SvTournamentMode ? TEAM_SPECTATORS : m_pController->GetAutoTeam(ClientID);

	// Potential cleanup, taken from DDNet, if slot is still not taken (might as well be a double free vulnerability)
	if(m_apPlayers[ClientID])
		delete m_apPlayers[ClientID];

	m_apPlayers[ClientID] = new(ClientID) CPlayer(this, ClientID, StartTeam);
	//players[client_id].init(client_id);
	//players[client_id].client_id = client_id;

	(void)m_pController->CheckTeamBalance();

#ifdef CONF_DEBUG
	if (g_Config.m_DbgDummies)
	{
		if (ClientID >= MAX_CLIENTS - g_Config.m_DbgDummies)
			return;
	}
#endif
	m_BotDetection->OnPlayerConnect(ClientID);
	// send active vote
	if (m_VoteCloseTime)
		SendVoteSet(ClientID);

	// send motd
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = g_Config.m_SvMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

	// access CServer through ban server
	if (!GetBanServer()->Server()->m_NetServer.HasSecurityToken(ClientID)) {
		SendChatTarget(ClientID, "Warning: You are not securely connected to this server.");
		SendChatTarget(ClientID, "You may have a worse experience playing on this server.");
		SendChatTarget(ClientID, "Please update your client to one that is protected against IP spoofing.");
		SendChatTarget(ClientID, "One possible client could be the DDNet Client.");
	}
}

void CGameContext::OnClientDrop(int ClientID, const char *pReason)
{

	if (m_apPlayers[ClientID]->m_CaughtBy > CPlayer::ZCATCH_NOT_CAUGHT)
		m_apPlayers[m_apPlayers[ClientID]->m_CaughtBy]->ReleaseZCatchVictim(ClientID);

	m_BotDetection->OnPlayerDisconnect(ClientID);

	// if player voted something/someone and it did not pass
	// before leaving the server, voteban him for the remaining time.
	int64 Now = Server()->Tick();
	int Timeleft = m_apPlayers[ClientID]->m_LastVoteCall + Server()->TickSpeed() * 60 - Now;
	// convert to seconds
	Timeleft = static_cast<int>(Timeleft / Server()->TickSpeed());
	if (m_apPlayers[ClientID]->m_LastVoteCall && Timeleft > 0)
	{
		GetBanServer()->Server()->AddVoteban(ClientID, Timeleft);
	}

	AbortVoteKickOnDisconnect(ClientID);
	m_apPlayers[ClientID]->OnDisconnect(pReason);

	// player leaves or gets banned, print all of his data
	// only visible in log
	PrintIrregularFlags(ClientID);
	PrintLongTermData(ClientID);

	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = 0;

	(void)m_pController->CheckTeamBalance();
	m_VoteUpdate = true;

	// update spectator modes
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (m_apPlayers[i] && m_apPlayers[i]->m_SpectatorID == ClientID)
			m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
	}

}

/**
 * @brief Teehistorian
 * @details [long description]
 *
 * @param ClientID [description]
 */
void CGameContext::OnClientEngineJoin(int ClientID)
{
	m_TeeHistorian.RecordPlayerJoin(Server()->ClientJoinHash(ClientID), Server()->ClientName(ClientID), ClientID, Server()->Tick());
}

void CGameContext::OnClientEngineDrop(int ClientID, const char *pReason)
{
	if (m_TeeHistorian.GetMode())
	{
		m_TeeHistorian.RecordPlayerDrop(Server()->ClientJoinHash(ClientID), Server()->ClientName(ClientID), ClientID, Server()->Tick(), pReason);
	}
}


// returns whether the player is allowed to chat, informs the player and mutes him if needed
bool CGameContext::MuteValidation(CPlayer *player)
{
	int i, ClientID = player->GetCID();
	if ((i = Muted(ClientID)) > -1)
	{
		char aBuf[48];
		int Expires = (m_aMutes[i].m_Expires - Server()->Tick()) / Server()->TickSpeed();
		str_format(aBuf, sizeof(aBuf), "You are muted for %d:%02d min.", Expires / 60, Expires % 60);
		SendChatTarget(ClientID, aBuf);
		return false;
	}
	//mute the player if he's spamming
	else if (g_Config.m_SvMuteDuration && ((player->m_ChatTicks += g_Config.m_SvChatValue) > g_Config.m_SvChatThreshold))
	{
		AddMute(ClientID, g_Config.m_SvMuteDuration, true);
		player->m_ChatTicks = 0;
		return false;
	}
	return true;
}

void CGameContext::OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID)
{
	void *pRawMsg = m_NetObjHandler.SecureUnpackMsg(MsgID, pUnpacker);
	CPlayer *pPlayer = m_apPlayers[ClientID];

	// sets player's client version.
	if(pPlayer){
		if (MsgID == (NETMSGTYPE_CL_CALLVOTE + 1))
		{
			int Version = pUnpacker->GetInt();
			pPlayer->SetClientVersion(Version);
			return;

		} else if (MsgID != NETMSGTYPE_CL_SAY &&
					MsgID != NETMSGTYPE_CL_CALLVOTE &&
					MsgID != NETMSGTYPE_CL_VOTE &&
					MsgID != NETMSGTYPE_CL_SETTEAM &&
					MsgID != NETMSGTYPE_CL_SETSPECTATORMODE &&
					MsgID != NETMSGTYPE_CL_STARTINFO &&
					MsgID != NETMSGTYPE_CL_CHANGEINFO &&
					MsgID != NETMSGTYPE_CL_EMOTICON &&
					MsgID != NETMSGTYPE_CL_KILL)
		{
			// todo: testing
			pPlayer->AddWeirdMessage(std::string(pUnpacker->GetString()));
			return;
		}
	}

	/*teehistorian*/
	if (m_TeeHistorian.GetMode())
	{
		if (m_NetObjHandler.TeeHistorianRecordMsg(MsgID))
		{
			m_TeeHistorian.RecordPlayerMessage(ClientID, pUnpacker->CompleteData(), pUnpacker->CompleteSize());
		}
	}
	/*teehistorian end*/

	if (!pRawMsg)
	{
		// todo: testing
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Msg '%s'(%d):failed'%s'", m_NetObjHandler.GetMsgName(MsgID), MsgID, m_NetObjHandler.FailedMsgOn());
		pPlayer->AddWeirdMessage(std::string(aBuf));

		if (g_Config.m_Debug)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler.GetMsgName(MsgID), MsgID, m_NetObjHandler.FailedMsgOn());
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
		}
		return;
	}

	if (MsgID == NETMSGTYPE_CL_SAY)
	{
		CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)pRawMsg;
		int Team = pMsg->m_Team;
		if (Team)
			Team = pPlayer->GetTeam();
		else
			Team = CGameContext::CHAT_ALL;

		if (g_Config.m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat + Server()->TickSpeed() > Server()->Tick())
			return;

		pPlayer->m_LastChat = Server()->Tick();

		// check for invalid chars
		unsigned char *pMessage = (unsigned char *)pMsg->m_pMessage;
		while (*pMessage)
		{
			if (*pMessage < 32)
				*pMessage = ' ';
			pMessage++;
		}

		/* begin zCatch*/
		if (!str_comp_num("/", pMsg->m_pMessage, 1))
		{
			if (!str_comp_nocase("info", pMsg->m_pMessage + 1))
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "zCatch %s by erd and Teetime, modified by Teelevision and modified again by jxsl13. See /help.", ZCATCH_VERSION);
				SendChatTarget(ClientID, aBuf);
				SendChatTarget(ClientID, "Players you catch (kill) join again when you die. Catch everyone to win.");
				if (g_Config.m_SvLastStandingPlayers > 2)
				{
					str_format(aBuf, sizeof(aBuf), "If there are less than %d players, the round does not end and all players are released instead.", g_Config.m_SvLastStandingPlayers);
					SendChatTarget(ClientID, aBuf);
				}
			}
			else if (!str_comp_nocase("cmdlist", pMsg->m_pMessage + 1))
			{
				SendChatTarget(ClientID, "Use /help to learn about chat commands.");
			}
			else if (!str_comp_nocase("help", pMsg->m_pMessage + 1))
			{
				SendChatTarget(ClientID, "Help topics: [1] game, [2] releasing, [3] PMs, [4] ranking, [5] hard mode");
				SendChatTarget(ClientID, "/help [1-5]: show help topic");
				SendChatTarget(ClientID, "/info: show info about this mod");
			}
			else if (!str_comp_nocase("help 1", pMsg->m_pMessage + 1))
			{
				SendChatTarget(ClientID, "--- Help 1 / 5 ---");
				SendChatTarget(ClientID, "Players you catch (kill) join again when you die. Catch everyone to win.");
				SendChatTarget(ClientID, "/kills: list of players you caught");
				SendChatTarget(ClientID, "/victims: who is waiting for your death");
			}
			else if (!str_comp_nocase("help 2", pMsg->m_pMessage + 1))
			{
				SendChatTarget(ClientID, "--- Help 2 / 5 ---");
				SendChatTarget(ClientID, "On suicide via console the last victim is released instead. You die if there is noone to release. The console command for suicide is 'kill'.");
			}
			else if (!str_comp_nocase("help 3", pMsg->m_pMessage + 1))
			{
				SendChatTarget(ClientID, "--- Help 3 / 5 ---");
				SendChatTarget(ClientID, "You can write private messages:");
				SendChatTarget(ClientID, "/t <name> <msg>: write PM to <name>");
				SendChatTarget(ClientID, "/ti <id> <msg>: write PM via ID");
			}
			else if (!str_comp_nocase("help 4", pMsg->m_pMessage + 1))
			{
				SendChatTarget(ClientID, "--- Help 4 / 5 ---");
				if (RankingEnabled())
				{
					SendChatTarget(ClientID, "The ranking system saves various stats about players. The stats are updated at the end of a round and on leaving the server.");
					SendChatTarget(ClientID, "/top [<category>]: display top 5 players");
					SendChatTarget(ClientID, "/rank [<player>]: display own/players's rank");
				}
				else
					SendChatTarget(ClientID, "The ranking system is disabled on this server.");
			}
			else if (!str_comp_nocase("help 5", pMsg->m_pMessage + 1))
			{
				SendChatTarget(ClientID, "--- Help 5 / 5 ---");
				SendChatTarget(ClientID, "The hard mode is an extra challenge for good players. You cannot leave it.");
				SendChatTarget(ClientID, "/hard: join random mode");
				SendChatTarget(ClientID, "/hard <modes>: join mode(s)");
				SendChatTarget(ClientID, "/hard list: list all available modes");
			}
			else if (!str_comp_nocase("victims", pMsg->m_pMessage + 1))
			{
				if (pPlayer->m_zCatchNumVictims)
				{
					char aBuf[256], bBuf[256];
					CPlayer::CZCatchVictim *v = pPlayer->m_ZCatchVictims;
					str_format(aBuf, sizeof(aBuf), "%d player(s) await your death: ", pPlayer->m_zCatchNumVictims);
					while (v != NULL)
					{
						str_format(bBuf, sizeof(bBuf), (v == pPlayer->m_ZCatchVictims) ? "%s '%s'%s" : "%s, '%s'%s", aBuf, Server()->ClientName(v->ClientID), (v->Reason == CPlayer::ZCATCH_CAUGHT_REASON_JOINING) ? " (joined the game)" : "");
						str_copy(aBuf, bBuf, sizeof(aBuf));
						v = v->prev;
					}
					SendChatTarget(ClientID, aBuf);
				}
				else
				{
					SendChatTarget(ClientID, "No one awaits your death.");
				}
			}
			else if (!str_comp_nocase("kills", pMsg->m_pMessage + 1))
			{
				if (pPlayer->m_zCatchNumKillsInARow)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "You caught %d player(s) since your last death.", pPlayer->m_zCatchNumKillsInARow);
					SendChatTarget(ClientID, aBuf);
				}
				else
				{
					SendChatTarget(ClientID, "You caught no one since your last death.");
				}
			}
			// tell / PM someone privately
			else if (!str_comp_nocase_num("t ", pMsg->m_pMessage + 1, 2) || !str_comp_nocase_num("ti ", pMsg->m_pMessage + 1, 3))
			{
				const char *recipientStart, *msgStart;
				int recipient = -1;

				// by name
				if (!str_comp_nocase_num("t ", pMsg->m_pMessage + 1, 2))
				{
					int recipientNameLength;
					const char *recipientName;
					recipientStart = str_skip_whitespaces((char*)pMsg->m_pMessage + 3);
					// check _all_ players (there might be partly identical names)
					for (int i = 0; i < MAX_CLIENTS; ++i)
					{
						if (m_apPlayers[i]
						        && (recipientName = Server()->ClientName(i))
						        && (recipientNameLength = str_length(recipientName))
						        && !str_comp_num(recipientName, recipientStart, recipientNameLength)
						        && recipientStart[recipientNameLength] == ' '
						   )
						{
							if (recipient >= 0)
							{
								SendChatTarget(ClientID, "Could not deliver private message. More than one player could be addressed.");
								return;
							}
							msgStart = recipientStart + recipientNameLength + 1;
							recipient = i;
						}
					}
				}

				// by id
				else if (!str_comp_nocase_num("ti ", pMsg->m_pMessage + 1, 3))
				{
					recipientStart = str_skip_whitespaces((char*)pMsg->m_pMessage + 4);
					// check if int given
					for (const char *c = recipientStart; *c != ' '; ++c)
					{
						if (*c < '0' || '9' < *c)
						{
							SendChatTarget(ClientID, "No id given, syntax is: /ti id message.");
							return;
						}
					}
					int i = str_toint(recipientStart);
					if (i >= MAX_CLIENTS)
					{
						SendChatTarget(ClientID, "Invalid id, syntax is: /ti id message.");
						return;
					}
					if (m_apPlayers[i])
					{
						recipient = i;
						msgStart = str_skip_whitespaces(str_skip_to_whitespace((char*)recipientStart));
					}
				}

				if (recipient >= 0)
				{
					if (MuteValidation(pPlayer))
					{
						// prepare message
						const char *msgForm = "/PM -> %s / %s";
						int len = 32 + MAX_NAME_LENGTH + str_length(msgStart);
						char *msg = (char*)malloc(len * sizeof(char));
						str_format(msg, len * sizeof(char), msgForm, Server()->ClientName(recipient), msgStart);

						// send message
						SendPrivateMessage(ClientID, recipient, msg);

						// tidy up
						free(msg);
					}
				}
				else
				{
					SendChatTarget(ClientID, "Could not deliver private message. Player not found.");
				}
			}

			/* ranking system */
			else if (RankingEnabled() && (!str_comp_nocase("top", pMsg->m_pMessage + 1) || !str_comp_nocase("top5", pMsg->m_pMessage + 1)))
			{
				m_pController->OnChatCommandTop(pPlayer);
			}
			else if (RankingEnabled() && (!str_comp_nocase_num("top ", pMsg->m_pMessage + 1, 4) || !str_comp_nocase_num("top5 ", pMsg->m_pMessage + 1, 5)))
			{
				char *category = str_skip_whitespaces((char*)pMsg->m_pMessage + 5);
				int length = str_length(category);
				/* trim right */
				while (length > 0 && str_skip_whitespaces(category + length - 1) >= (category + length)) {
					--length;
					category[length] = 0;
				}
				m_pController->OnChatCommandTop(pPlayer, category);
			}
			else if (RankingEnabled() && (!str_comp_nocase("rank", pMsg->m_pMessage + 1)))
			{
				m_pController->OnChatCommandOwnRank(pPlayer);
			}
			else if (RankingEnabled() && (!str_comp_nocase_num("rank ", pMsg->m_pMessage + 1, 5)))
			{
				char *name = str_skip_whitespaces((char*)pMsg->m_pMessage + 6);
				int length = str_length(name);
				/* trim right */
				while (length > 0 && str_skip_whitespaces(name + length - 1) >= (name + length)) {
					--length;
					name[length] = 0;
				}
				m_pController->OnChatCommandRank(pPlayer, name);
			}

			// hard mode
			else if (g_Config.m_SvAllowHardMode == 1 && (!str_comp_nocase_num("hard", pMsg->m_pMessage + 1, 4) || !str_comp_nocase_num("hard ", pMsg->m_pMessage + 1, 5)))
			{
				if (pPlayer->m_HardMode.m_Active)
				{
					SendChatTarget(ClientID, "You already are in hard mode.");
				}
				else
				{
					char *optionStart, *m = (char*)pMsg->m_pMessage + 5;

					pPlayer->ResetHardMode();

					// to print the hard modes in the chat later
					auto addMode = [pPlayer](const char* mode) {
						auto d = &pPlayer->m_HardMode.m_Description;
						if (*d[0] != 0)
							str_append(*d, " ", sizeof(*d));
						str_append(*d, mode, sizeof(*d));
					};

					// read options from the command and assign those hard modes
					while (*m)
					{
						optionStart = str_skip_whitespaces(m);
						m = str_skip_to_whitespace(optionStart);

						// no mode is longer than this
						if ((m - optionStart) > 30)
						{
							SendChatTarget(ClientID, "That mode is bullshit!");
							return;
						}

						// get mode
						char mode[32];
						str_copy(mode, optionStart, m - optionStart + 1);

						// add mode
						if (!pPlayer->AddHardMode(mode))
						{
							// unknown mode
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "'%s' is not a hard mode, dumbass!", mode);
							SendChatTarget(ClientID, aBuf);

							// give out list of hard modes
							char mBuf[256], mBuf2[256];
							mBuf[0] = 0;
							for (auto it = m_HardModes.begin(); it != m_HardModes.end(); ++it)
							{
								if ((g_Config.m_SvMode == 1 && it->laser) || (g_Config.m_SvMode == 4 && it->grenade))
								{
									str_format(mBuf2, sizeof(mBuf2), "%s %s", mBuf, it->name);
									str_copy(mBuf, mBuf2, sizeof(mBuf));
								}
							}
							str_format(aBuf, sizeof(aBuf), "Hard modes:%s", mBuf);
							SendChatTarget(ClientID, aBuf);

							return;
						}

						addMode(mode);
					}

					// none mode was added above
					// assign random
					if (!pPlayer->m_HardMode.m_Active)
					{
						addMode(pPlayer->AddRandomHardMode());
					}

					// player has to start over again
					pPlayer->KillCharacter();

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "'%s' entered hard mode (%s)", Server()->ClientName(ClientID), pPlayer->m_HardMode.m_Description);
					SendChat(-1, CGameContext::CHAT_ALL, aBuf);

				}

				// inform player about the hard modes he got
				char aBuf[256];
				auto mode = &pPlayer->m_HardMode;
				if (mode->m_ModeAmmoLimit > 0 || mode->m_ModeAmmoRegenFactor > 0)
				{
					str_format(aBuf, sizeof(aBuf), "Hard mode: ammo limit of %d and %dx regen time", mode->m_ModeAmmoLimit, mode->m_ModeAmmoRegenFactor);
					SendChatTarget(ClientID, aBuf);
				}
				if (mode->m_ModeHookWhileKilling)
					SendChatTarget(ClientID, "Hard mode: hook players while catching them");
				if (mode->m_ModeWeaponOverheats.m_Active)
					SendChatTarget(ClientID, "Hard mode: your weapon kills you if overheating");
				if (mode->m_ModeTotalFails.m_Active)
				{
					str_format(aBuf, sizeof(aBuf), "Hard mode: you are allowed to fail %d shots", mode->m_ModeTotalFails.m_Max);
					SendChatTarget(ClientID, aBuf);
				}
				if (mode->m_ModeKillTimelimit.m_Active)
				{
					str_format(aBuf, sizeof(aBuf), "Hard mode: you have %d seconds between catching players", mode->m_ModeKillTimelimit.m_TimeSeconds);
					SendChatTarget(ClientID, aBuf);
				}
				if (mode->m_ModeDoubleKill.m_Active)
					SendChatTarget(ClientID, "Hard mode: hit everyone two times in a row");
			}
			else if (g_Config.m_SvAllowHardMode == 0 && (!str_comp_nocase_num("hard", pMsg->m_pMessage + 1, 4) || !str_comp_nocase_num("hard ", pMsg->m_pMessage + 1, 5)))
			{
				SendChatTarget(ClientID, "Hard Mode is disabled on this server.");
			}
			else
			{
				SendChatTarget(ClientID, "Unknown command, try /info");
			}
		}
		else
		{
			// send to chat
			if (MuteValidation(pPlayer))
				SendChat(ClientID, Team, pMsg->m_pMessage);
		}
		/* end zCatch */
	}
	else if (MsgID == NETMSGTYPE_CL_CALLVOTE)
	{
		if (g_Config.m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry + Server()->TickSpeed() * 3 > Server()->Tick())
			return;

		int64 Now = Server()->Tick();
		pPlayer->m_LastVoteTry = Now;
		// zCatch - Only people which are explicit in spectators can't vote!
		if (pPlayer->m_SpecExplicit) //zCatch
		{
			SendChatTarget(ClientID, "Spectators aren't allowed to start a vote.");
			return;
		}

		if (m_VoteCloseTime)
		{
			SendChatTarget(ClientID, "Wait for current vote to end before calling a new one.");
			return;
		}

		char aChatmsg[512];

		// check voteban
		int left = Server()->ClientVotebannedTime(ClientID);
		if (left)
		{
			str_format(aChatmsg, sizeof(aChatmsg), "You are not allowed to vote for the next %d:%02d min.", left / 60, left % 60);
			SendChatTarget(ClientID, aChatmsg);
			return;
		}

		int Timeleft = pPlayer->m_LastVoteCall + Server()->TickSpeed() * 60 - Now;
		if (pPlayer->m_LastVoteCall && Timeleft > 0)
		{
			str_format(aChatmsg, sizeof(aChatmsg), "You must wait %d seconds before making another vote", (Timeleft / Server()->TickSpeed()) + 1);
			SendChatTarget(ClientID, aChatmsg);
			return;
		}

		char aDesc[VOTE_DESC_LENGTH] = {0};
		char aCmd[VOTE_CMD_LENGTH] = {0};
		CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)pRawMsg;
		const char *pReason = pMsg->m_Reason[0] ? pMsg->m_Reason : "No reason given";

		if (str_comp_nocase(pMsg->m_Type, "option") == 0)
		{
			CVoteOptionServer *pOption = m_pVoteOptionFirst;
			while (pOption)
			{
				if (str_comp_nocase(pMsg->m_Value, pOption->m_aDescription) == 0)
				{
					str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s' (%s)", Server()->ClientName(ClientID),
					           pOption->m_aDescription, pReason);
					str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);
					str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
					break;
				}

				pOption = pOption->m_pNext;
			}

			if (!pOption)
			{
				str_format(aChatmsg, sizeof(aChatmsg), "'%s' isn't an option on this server", pMsg->m_Value);
				SendChatTarget(ClientID, aChatmsg);
				return;
			}
		}
		else if (g_Config.m_SvVoteForceReason && !pMsg->m_Reason[0])
		{
			SendChatTarget(ClientID, "You must give a reason for your vote");
			return;
		}
		else if (str_comp_nocase(pMsg->m_Type, "kick") == 0)
		{
			if (!g_Config.m_SvVoteKick)
			{
				SendChatTarget(ClientID, "Server does not allow voting to kick players");
				return;
			}

			if (g_Config.m_SvVoteKickMin)
			{
				int PlayerNum = 0;
				for (int i = 0; i < MAX_CLIENTS; ++i)
					if (m_apPlayers[i] && !m_apPlayers[i]->m_SpecExplicit) // zCatch - Count all players which are not explicit in spectator
						++PlayerNum;

				if (PlayerNum < g_Config.m_SvVoteKickMin)
				{
					str_format(aChatmsg, sizeof(aChatmsg), "Kick voting requires %d players on the server", g_Config.m_SvVoteKickMin);
					SendChatTarget(ClientID, aChatmsg);
					return;
				}
			}

			int KickID = str_toint(pMsg->m_Value);
			if (KickID < 0 || KickID >= MAX_CLIENTS || !m_apPlayers[KickID])
			{
				SendChatTarget(ClientID, "Invalid client id to kick");
				return;
			}
			if (KickID == ClientID)
			{
				SendChatTarget(ClientID, "You can't kick yourself");
				return;
			}
			if (Server()->IsAuthed(KickID))
			{
				SendChatTarget(ClientID, "You can't kick admins");
				char aBufKick[128];
				str_format(aBufKick, sizeof(aBufKick), "'%s' called for vote to kick you (%s)", Server()->ClientName(ClientID), pReason);
				SendChatTarget(KickID, aBufKick);
				return;
			}

			str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to kick '%s' (%s)", Server()->ClientName(ClientID), Server()->ClientName(KickID), pReason);
			str_format(aDesc, sizeof(aDesc), "Kick '%s'", Server()->ClientName(KickID));
			if (!g_Config.m_SvVoteKickBantime)
				str_format(aCmd, sizeof(aCmd), "kick %d Kicked by vote", KickID);
			else
			{
				char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
				Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
				str_format(aCmd, sizeof(aCmd), "ban %s %d Banned by vote", aAddrStr, g_Config.m_SvVoteKickBantime);
			}
		}
		else if (str_comp_nocase(pMsg->m_Type, "spectate") == 0)
		{
			if (!g_Config.m_SvVoteSpectate)
			{
				SendChatTarget(ClientID, "Server does not allow voting to move players to spectators");
				return;
			}

			int SpectateID = str_toint(pMsg->m_Value);
			if (SpectateID < 0 || SpectateID >= MAX_CLIENTS || !m_apPlayers[SpectateID] || m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
			{
				SendChatTarget(ClientID, "Invalid client id to move");
				return;
			}
			if (SpectateID == ClientID)
			{
				SendChatTarget(ClientID, "You can't move yourself");
				return;
			}

			str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to move '%s' to spectators (%s)", Server()->ClientName(ClientID), Server()->ClientName(SpectateID), pReason);
			str_format(aDesc, sizeof(aDesc), "move '%s' to spectators", Server()->ClientName(SpectateID));
			str_format(aCmd, sizeof(aCmd), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
		}

		if (aCmd[0])
		{
			SendChat(-1, CGameContext::CHAT_ALL, aChatmsg);
			StartVote(aDesc, aCmd, pReason);
			pPlayer->m_Vote = 1;
			pPlayer->m_VotePos = m_VotePos = 1;
			m_VoteCreator = ClientID;
			pPlayer->m_LastVoteCall = Now;
		}
	}
	else if (MsgID == NETMSGTYPE_CL_VOTE)
	{
		if (!m_VoteCloseTime)
			return;

		if (pPlayer->m_Vote == 0)
		{
			CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
			if (!pMsg->m_Vote)
				return;

			pPlayer->m_Vote = pMsg->m_Vote;
			pPlayer->m_VotePos = ++m_VotePos;
			m_VoteUpdate = true;
		}
	}
	else if (MsgID == NETMSGTYPE_CL_SETTEAM && !m_World.m_Paused)
	{
		CNetMsg_Cl_SetTeam *pMsg = (CNetMsg_Cl_SetTeam *)pRawMsg;

		if (pPlayer->GetTeam() == pMsg->m_Team || (g_Config.m_SvSpamprotection && pPlayer->m_LastSetTeam && pPlayer->m_LastSetTeam + Server()->TickSpeed() * 3 > Server()->Tick()))
			return;

		if (pMsg->m_Team != TEAM_SPECTATORS && m_LockTeams)
		{
			pPlayer->m_LastSetTeam = Server()->Tick();
			SendBroadcast("Teams are locked", ClientID);
			return;
		}

		if (pPlayer->m_TeamChangeTick > Server()->Tick())
		{
			pPlayer->m_LastSetTeam = Server()->Tick();
			int TimeLeft = (pPlayer->m_TeamChangeTick - Server()->Tick()) / Server()->TickSpeed();
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Time to wait before changing team: %02d:%02d", TimeLeft / 60, TimeLeft % 60);
			SendBroadcast(aBuf, ClientID);
			return;
		}

		// Switch team on given client and kill/respawn him
		if (m_pController->CanJoinTeam(pMsg->m_Team, ClientID))
		{
			if (m_pController->CanChangeTeam(pPlayer, pMsg->m_Team) || !Server()->ClientIngame(pPlayer->m_CaughtBy))
			{
				pPlayer->m_LastSetTeam = Server()->Tick();
				pPlayer->SetTeam(pMsg->m_Team);
			}
			else
			{
				char aBuf[128];
				pPlayer->m_zCatchJoinSpecWhenReleased = !pPlayer->m_zCatchJoinSpecWhenReleased;
				str_format(aBuf, sizeof(aBuf), "You will join the %s when '%s' dies.", m_pController->GetTeamName(pPlayer->m_zCatchJoinSpecWhenReleased ? TEAM_SPECTATORS : TEAM_RED), Server()->ClientName(pPlayer->m_CaughtBy));
				SendChatTarget(ClientID, aBuf);
				return;
			}
		}
		else
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Only %d active players are allowed", Server()->MaxClients() - g_Config.m_SvSpectatorSlots);
			SendBroadcast(aBuf, ClientID);
		}
	}
	else if (MsgID == NETMSGTYPE_CL_SETSPECTATORMODE && !m_World.m_Paused)
	{
		CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

		if (pPlayer->GetTeam() != TEAM_SPECTATORS || pPlayer->m_SpectatorID == pMsg->m_SpectatorID || ClientID == pMsg->m_SpectatorID ||
		        (g_Config.m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode + Server()->TickSpeed() * 3 > Server()->Tick()))
			return;

		pPlayer->m_LastSetSpectatorMode = Server()->Tick();
		if (pMsg->m_SpectatorID != SPEC_FREEVIEW && (!m_apPlayers[pMsg->m_SpectatorID] || m_apPlayers[pMsg->m_SpectatorID]->GetTeam() == TEAM_SPECTATORS))
			SendChatTarget(ClientID, "Invalid spectator id used");
		else
			pPlayer->m_SpectatorID = pMsg->m_SpectatorID;
	}
	else if (MsgID == NETMSGTYPE_CL_STARTINFO)
	{
		if (pPlayer->m_IsReady)
			return;

		CNetMsg_Cl_StartInfo *pMsg = (CNetMsg_Cl_StartInfo *)pRawMsg;
		pPlayer->m_LastChangeInfo = Server()->Tick();

		// set start infos
		Server()->SetClientName(ClientID, pMsg->m_pName);

		// nick name ban stuff on connection
		if (IsInNicknameBanList(pMsg->m_pName)) {
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Your nickname '%s' is banned.", pMsg->m_pName);
			Server()->Kick(ClientID, aBuf);
			return;
		}

		Server()->SetClientClan(ClientID, pMsg->m_pClan);
		Server()->SetClientCountry(ClientID, pMsg->m_Country);
		str_copy(pPlayer->m_TeeInfos.m_SkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_SkinName));
		pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
		pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
		pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
		m_pController->OnPlayerInfoChange(pPlayer);

		/*sqlitehistorian*/
		char aJoinHash[MAX_NAME_LENGTH + 32];
		str_format(aJoinHash, sizeof(aJoinHash), "%32d %s-%d", Server()->Tick(), Server()->ClientName(ClientID), ClientID);

		int Hash = str_quickhash(aJoinHash);
		Server()->SetClientJoinHash(ClientID, Hash);


		// send vote options
		CNetMsg_Sv_VoteClearOptions ClearMsg;
		Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);

		CNetMsg_Sv_VoteOptionListAdd OptionMsg;
		int NumOptions = 0;
		OptionMsg.m_pDescription0 = "";
		OptionMsg.m_pDescription1 = "";
		OptionMsg.m_pDescription2 = "";
		OptionMsg.m_pDescription3 = "";
		OptionMsg.m_pDescription4 = "";
		OptionMsg.m_pDescription5 = "";
		OptionMsg.m_pDescription6 = "";
		OptionMsg.m_pDescription7 = "";
		OptionMsg.m_pDescription8 = "";
		OptionMsg.m_pDescription9 = "";
		OptionMsg.m_pDescription10 = "";
		OptionMsg.m_pDescription11 = "";
		OptionMsg.m_pDescription12 = "";
		OptionMsg.m_pDescription13 = "";
		OptionMsg.m_pDescription14 = "";
		CVoteOptionServer *pCurrent = m_pVoteOptionFirst;
		while (pCurrent)
		{
			switch (NumOptions++)
			{
			case 0: OptionMsg.m_pDescription0 = pCurrent->m_aDescription; break;
			case 1: OptionMsg.m_pDescription1 = pCurrent->m_aDescription; break;
			case 2: OptionMsg.m_pDescription2 = pCurrent->m_aDescription; break;
			case 3: OptionMsg.m_pDescription3 = pCurrent->m_aDescription; break;
			case 4: OptionMsg.m_pDescription4 = pCurrent->m_aDescription; break;
			case 5: OptionMsg.m_pDescription5 = pCurrent->m_aDescription; break;
			case 6: OptionMsg.m_pDescription6 = pCurrent->m_aDescription; break;
			case 7: OptionMsg.m_pDescription7 = pCurrent->m_aDescription; break;
			case 8: OptionMsg.m_pDescription8 = pCurrent->m_aDescription; break;
			case 9: OptionMsg.m_pDescription9 = pCurrent->m_aDescription; break;
			case 10: OptionMsg.m_pDescription10 = pCurrent->m_aDescription; break;
			case 11: OptionMsg.m_pDescription11 = pCurrent->m_aDescription; break;
			case 12: OptionMsg.m_pDescription12 = pCurrent->m_aDescription; break;
			case 13: OptionMsg.m_pDescription13 = pCurrent->m_aDescription; break;
			case 14:
			{
				OptionMsg.m_pDescription14 = pCurrent->m_aDescription;
				OptionMsg.m_NumOptions = NumOptions;
				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
				OptionMsg = CNetMsg_Sv_VoteOptionListAdd();
				NumOptions = 0;
				OptionMsg.m_pDescription1 = "";
				OptionMsg.m_pDescription2 = "";
				OptionMsg.m_pDescription3 = "";
				OptionMsg.m_pDescription4 = "";
				OptionMsg.m_pDescription5 = "";
				OptionMsg.m_pDescription6 = "";
				OptionMsg.m_pDescription7 = "";
				OptionMsg.m_pDescription8 = "";
				OptionMsg.m_pDescription9 = "";
				OptionMsg.m_pDescription10 = "";
				OptionMsg.m_pDescription11 = "";
				OptionMsg.m_pDescription12 = "";
				OptionMsg.m_pDescription13 = "";
				OptionMsg.m_pDescription14 = "";
			}
			}
			pCurrent = pCurrent->m_pNext;
		}
		if (NumOptions > 0)
		{
			OptionMsg.m_NumOptions = NumOptions;
			Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
		}

		// send tuning parameters to client
		SendTuningParams(ClientID);

		// client is ready to enter
		pPlayer->m_IsReady = true;
		CNetMsg_Sv_ReadyToEnter m;
		Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
	}
	else if (MsgID == NETMSGTYPE_CL_CHANGEINFO)
	{
		if (g_Config.m_SvSpamprotection && pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo + Server()->TickSpeed() * 5 > Server()->Tick())
			return;

		CNetMsg_Cl_ChangeInfo *pMsg = (CNetMsg_Cl_ChangeInfo *)pRawMsg;
		pPlayer->m_LastChangeInfo = Server()->Tick();

		// set infos
		char aOldName[MAX_NAME_LENGTH];
		str_copy(aOldName, Server()->ClientName(ClientID), sizeof(aOldName));
		Server()->SetClientName(ClientID, pMsg->m_pName);

		// nickban nickname ban on nick change.
		if (IsInNicknameBanList(pMsg->m_pName)) {
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Your nickname '%s' is banned.", pMsg->m_pName);
			Server()->Kick(ClientID, aBuf);
			return;
		}

		if (str_comp(aOldName, Server()->ClientName(ClientID)) != 0 && Muted(ClientID) == -1)
		{
			char aChatText[256];
			str_format(aChatText, sizeof(aChatText), "'%s' changed name to '%s'", aOldName, Server()->ClientName(ClientID));
			SendChat(-1, CGameContext::CHAT_ALL, aChatText);
		}
		Server()->SetClientClan(ClientID, pMsg->m_pClan);
		Server()->SetClientCountry(ClientID, pMsg->m_Country);
		str_copy(pPlayer->m_TeeInfos.m_SkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_SkinName));
		pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
		pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
		pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
		m_pController->OnPlayerInfoChange(pPlayer);
	}
	else if (MsgID == NETMSGTYPE_CL_EMOTICON && !m_World.m_Paused)
	{
		CNetMsg_Cl_Emoticon *pMsg = (CNetMsg_Cl_Emoticon *)pRawMsg;

		if (g_Config.m_SvSpamprotection && pPlayer->m_LastEmote && pPlayer->m_LastEmote + Server()->TickSpeed() * 3 > Server()->Tick())
			return;

		pPlayer->m_LastEmote = Server()->Tick();

		SendEmoticon(ClientID, pMsg->m_Emoticon);
	}
	else if (MsgID == NETMSGTYPE_CL_KILL && !m_World.m_Paused)
	{
		/* begin zCatch*/
		if (pPlayer->GetTeam() == TEAM_SPECTATORS || (pPlayer->m_LastKill && pPlayer->m_LastKill + Server()->TickSpeed() * 3 > Server()->Tick()) ||
		        (pPlayer->m_LastKillTry + Server()->TickSpeed() * 3 > Server()->Tick()))
			return;

		if (pPlayer->HasZCatchVictims())
		{
			int lastVictim = pPlayer->LastZCatchVictim();
			pPlayer->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL, 1, true);
			int nextToRelease = pPlayer->LastZCatchVictim();
			char aBuf[128], bBuf[128];
			str_format(bBuf, sizeof(bBuf), ", next: %s", Server()->ClientName(nextToRelease));
			str_format(aBuf, sizeof(aBuf), "You released '%s'. (%d left%s)", Server()->ClientName(lastVictim), pPlayer->m_zCatchNumVictims, pPlayer->m_zCatchNumVictims > 0 ? bBuf : "");
			SendChatTarget(ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "You were released by '%s'.", Server()->ClientName(ClientID));
			SendChatTarget(lastVictim, aBuf);

			// console release log
			str_format(aBuf, sizeof(aBuf), "release killer='%d:%s' victim='%d:%s'" , ClientID, Server()->ClientName(ClientID), lastVictim, Server()->ClientName(lastVictim));
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);

			// Send release information to the same killer's victims
			// Message flood risk on first killed players!
			//if(pPlayer->m_zCatchNumVictims > 0)
			//{
			//	CPlayer::CZCatchVictim *v = pPlayer->m_ZCatchVictims;
			//	str_format(aBuf, sizeof(aBuf), "'%s' released '%s'", Server()->ClientName(ClientID), Server()->ClientName(lastVictim));
			//	while(v != NULL)
			//	{
			//		SendChatTarget(v->ClientID, aBuf);
			//		v = v->prev;
			//	}
			//}


			return;
		}
		else if (g_Config.m_SvSuicideTime == 0)
		{
			SendChatTarget(ClientID, "Suicide is not allowed.");
		}
		else if (pPlayer->m_LastKill && pPlayer->m_LastKill + Server()->TickSpeed()*g_Config.m_SvSuicideTime > Server()->Tick())
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Only one suicide every %d seconds is allowed.", g_Config.m_SvSuicideTime);
			SendChatTarget(ClientID, aBuf);
		}
		else if (pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_FreezeTicks)
		{
			SendChatTarget(ClientID, "You can't kill yourself while you're frozen.");
		}
		else
		{
			pPlayer->m_LastKill = Server()->Tick();
			pPlayer->KillCharacter(WEAPON_SELF);
			return;
		}
		pPlayer->m_LastKillTry = Server()->Tick();
		/* end zCatch*/
	}
}

void CGameContext::OnSetAuthed(int ClientID, int Level)
{
	if (m_apPlayers[ClientID])
	{
		char aBuf[512], aIP[NETADDR_MAXSTRSIZE];
		Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));
		str_format(aBuf, sizeof(aBuf), "ban %s %d Banned by vote", aIP, g_Config.m_SvVoteKickBantime);
		if (!str_comp_nocase(m_aVoteCommand, aBuf) && Level > 0)
		{
			m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO_ADMIN;
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "CGameContext", "Aborted vote by admin login.");
		}
	}
	if (m_TeeHistorian.GetMode())
	{
		if (Level)
		{
			m_TeeHistorian.RecordAuthLogin(Server()->ClientName(ClientID), ClientID, Level, Server()->GetAuthName(ClientID));
		}
		else
		{
			m_TeeHistorian.RecordAuthLogout(Server()->ClientName(ClientID), ClientID);
		}
	}
}

void CGameContext::AddMute(const char* pIP, int Secs)
{
	int Pos = Muted(pIP);
	if (Pos > -1)
		m_aMutes[Pos].m_Expires = Server()->TickSpeed() * Secs + Server()->Tick();	// overwrite mute
	else
		for (int i = 0; i < MAX_MUTES; i++)	// find free slot
			if (!m_aMutes[i].m_aIP[0])
			{
				str_copy(m_aMutes[i].m_aIP, pIP, sizeof(m_aMutes[i].m_aIP));
				m_aMutes[i].m_Expires = Server()->TickSpeed() * Secs + Server()->Tick();
				break;
			}
}

void CGameContext::AddMute(int ClientID, int Secs, bool Auto)
{
	char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
	Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
	AddMute(aAddrStr, Secs);

	char aBuf[128];
	if (Secs > 0)
		str_format(aBuf, sizeof(aBuf), "%s has been %smuted for %d:%02d min.", Server()->ClientName(ClientID), Auto ? "auto-" : "", Secs / 60, Secs % 60);
	else
		str_format(aBuf, sizeof(aBuf), "%s has been unmuted.", Server()->ClientName(ClientID));
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
	SendChatTarget(-1, aBuf);
}

int CGameContext::Muted(const char *pIP)
{
	CleanMutes();
	int Pos = -1;
	if (!pIP[0])
		return -1;
	for (int i = 0; i < MAX_MUTES; i++)
		if (!str_comp_num(pIP, m_aMutes[i].m_aIP, sizeof(m_aMutes[i].m_aIP)))
		{
			Pos = i;
			break;
		}
	return Pos;
}

int CGameContext::Muted(int ClientID)
{
	char aIP[NETADDR_MAXSTRSIZE] = {0};
	Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));
	return Muted(aIP);
}

void CGameContext::CleanMutes()
{
	for (int i = 0; i < MAX_MUTES; i++)
		if (m_aMutes[i].m_Expires < Server()->Tick())
			m_aMutes[i].m_aIP[0] = 0;
}

void CGameContext::ConTuneParam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);
	float NewValue = pResult->GetFloat(1);

	if (pSelf->Tuning()->Set(pParamName, NewValue))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		pSelf->SendTuningParams(-1);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
}

void CGameContext::ConTuneReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CTuningParams TuningParams;
	*pSelf->Tuning() = TuningParams;
	pSelf->SendTuningParams(-1);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "Tuning reset");
}

void CGameContext::ConTuneDump(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[256];
	for (int i = 0; i < pSelf->Tuning()->Num(); i++)
	{
		float v;
		pSelf->Tuning()->Get(i, &v);
		str_format(aBuf, sizeof(aBuf), "%s %.2f", pSelf->Tuning()->m_apNames[i], v);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConPause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->TogglePause();
}

void CGameContext::ConChangeMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->ChangeMap(pResult->NumArguments() ? pResult->GetString(0) : "");
}

void CGameContext::ConRestart(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (pResult->NumArguments())
		pSelf->m_pController->DoWarmup(pResult->GetInteger(0));
	else
		pSelf->m_pController->StartRound();
}

void CGameContext::ConBroadcast(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendBroadcast(pResult->GetString(0), -1);
}

void CGameContext::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, pResult->GetString(0));
}

void CGameContext::ConSayTo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int playerID(pResult->GetInteger(0));
	if (playerID < 0 || playerID > MAX_CLIENTS)
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "Invalid id: %d", playerID);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	pSelf->SendChatTarget(playerID, pResult->GetString(1));
}


void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS - 1);
	int Team = clamp(pResult->GetInteger(1), -1, 1);
	int Delay = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : 0;
	if (!pSelf->m_apPlayers[ClientID])
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientID, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	pSelf->m_apPlayers[ClientID]->m_TeamChangeTick = pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * Delay * 60;
	pSelf->m_apPlayers[ClientID]->SetTeam(Team);
	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConSetTeamAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Team = clamp(pResult->GetInteger(0), -1, 1);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "All players were moved to the %s", pSelf->m_pController->GetTeamName(Team));
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

	for (int i = 0; i < MAX_CLIENTS; ++i)
		if (pSelf->m_apPlayers[i])
			pSelf->m_apPlayers[i]->SetTeam(Team, false);

	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConSpeakAs(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int playerID(pResult->GetInteger(0));
	if (playerID < 0 || playerID > MAX_CLIENTS)
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "Invalid id: %d", playerID);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	pSelf->SendChat(playerID, CGameContext::CHAT_ALL, pResult->GetString(1));
}

void CGameContext::ConSwapTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SwapTeams();
}

void CGameContext::ConShuffleTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf->m_pController->IsTeamplay())
		return;

	int CounterRed = 0;
	int CounterBlue = 0;
	int PlayerTeam = 0;
	for (int i = 0; i < MAX_CLIENTS; ++i)
		if (pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			++PlayerTeam;
	PlayerTeam = (PlayerTeam + 1) / 2;

	pSelf->SendChat(-1, CGameContext::CHAT_ALL, "Teams were shuffled");

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
		{
			if (CounterRed == PlayerTeam)
				pSelf->m_apPlayers[i]->SetTeam(TEAM_BLUE, false);
			else if (CounterBlue == PlayerTeam)
				pSelf->m_apPlayers[i]->SetTeam(TEAM_RED, false);
			else
			{
				if (rand() % 2)
				{
					pSelf->m_apPlayers[i]->SetTeam(TEAM_BLUE, false);
					++CounterBlue;
				}
				else
				{
					pSelf->m_apPlayers[i]->SetTeam(TEAM_RED, false);
					++CounterRed;
				}
			}
		}
	}

	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConLockTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_LockTeams ^= 1;
	if (pSelf->m_LockTeams)
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, "Teams were locked");
	else
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, "Teams were unlocked");
}

void CGameContext::ConAddVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	if (pSelf->m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of vote options reached");
		return;
	}

	// check for valid option
	if (!pSelf->Console()->LineIsValid(pCommand) || str_length(pCommand) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", pCommand);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	while (*pDescription && *pDescription == ' ')
		pDescription++;
	if (str_length(pDescription) >= VOTE_DESC_LENGTH || *pDescription == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// check for duplicate entry
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while (pOption)
	{
		if (str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "option '%s' already exists", pDescription);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
		pOption = pOption->m_pNext;
	}

	// add the option
	++pSelf->m_NumVoteOptions;
	int Len = str_length(pCommand);

	pOption = (CVoteOptionServer *)pSelf->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = pSelf->m_pVoteOptionLast;
	if (pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	pSelf->m_pVoteOptionLast = pOption;
	if (!pSelf->m_pVoteOptionFirst)
		pSelf->m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, pDescription, sizeof(pOption->m_aDescription));
	mem_copy(pOption->m_aCommand, pCommand, Len + 1);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// inform clients about added option
	CNetMsg_Sv_VoteOptionAdd OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);
}

void CGameContext::ConRemoveVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);

	// check for valid option
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while (pOption)
	{
		if (str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
			break;
		pOption = pOption->m_pNext;
	}
	if (!pOption)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "option '%s' does not exist", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// inform clients about removed option
	CNetMsg_Sv_VoteOptionRemove OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);

	// TODO: improve this
	// remove the option
	--pSelf->m_NumVoteOptions;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "removed option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	CHeap *pVoteOptionHeap = new CHeap();
	CVoteOptionServer *pVoteOptionFirst = 0;
	CVoteOptionServer *pVoteOptionLast = 0;
	int NumVoteOptions = pSelf->m_NumVoteOptions;
	for (CVoteOptionServer *pSrc = pSelf->m_pVoteOptionFirst; pSrc; pSrc = pSrc->m_pNext)
	{
		if (pSrc == pOption)
			continue;

		// copy option
		int Len = str_length(pSrc->m_aCommand);
		CVoteOptionServer *pDst = (CVoteOptionServer *)pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
		pDst->m_pNext = 0;
		pDst->m_pPrev = pVoteOptionLast;
		if (pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		pVoteOptionLast = pDst;
		if (!pVoteOptionFirst)
			pVoteOptionFirst = pDst;

		str_copy(pDst->m_aDescription, pSrc->m_aDescription, sizeof(pDst->m_aDescription));
		mem_copy(pDst->m_aCommand, pSrc->m_aCommand, Len + 1);
	}

	// clean up
	delete pSelf->m_pVoteOptionHeap;
	pSelf->m_pVoteOptionHeap = pVoteOptionHeap;
	pSelf->m_pVoteOptionFirst = pVoteOptionFirst;
	pSelf->m_pVoteOptionLast = pVoteOptionLast;
	pSelf->m_NumVoteOptions = NumVoteOptions;
}

void CGameContext::ConForceVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pType = pResult->GetString(0);
	const char *pValue = pResult->GetString(1);
	const char *pReason = pResult->NumArguments() > 2 && pResult->GetString(2)[0] ? pResult->GetString(2) : "No reason given";
	char aBuf[128] = {0};

	if (str_comp_nocase(pType, "option") == 0)
	{
		CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
		while (pOption)
		{
			if (str_comp_nocase(pValue, pOption->m_aDescription) == 0)
			{
				str_format(aBuf, sizeof(aBuf), "admin forced server option '%s' (%s)", pValue, pReason);
				pSelf->SendChatTarget(-1, aBuf);
				pSelf->Console()->ExecuteLine(pOption->m_aCommand);
				break;
			}

			pOption = pOption->m_pNext;
		}

		if (!pOption)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' isn't an option on this server", pValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
	}
	else if (str_comp_nocase(pType, "kick") == 0)
	{
		int KickID = str_toint(pValue);
		if (KickID < 0 || KickID >= MAX_CLIENTS || !pSelf->m_apPlayers[KickID])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to kick");
			return;
		}

		if (!g_Config.m_SvVoteKickBantime)
		{
			str_format(aBuf, sizeof(aBuf), "kick %d %s", KickID, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
		else
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			pSelf->Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
			str_format(aBuf, sizeof(aBuf), "ban %s %d %s", aAddrStr, g_Config.m_SvVoteKickBantime, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
	}
	else if (str_comp_nocase(pType, "spectate") == 0)
	{
		int SpectateID = str_toint(pValue);
		if (SpectateID < 0 || SpectateID >= MAX_CLIENTS || !pSelf->m_apPlayers[SpectateID] || pSelf->m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to move");
			return;
		}

		str_format(aBuf, sizeof(aBuf), "admin moved '%s' to spectator (%s)", pSelf->Server()->ClientName(SpectateID), pReason);
		pSelf->SendChatTarget(-1, aBuf);
		str_format(aBuf, sizeof(aBuf), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
		pSelf->Console()->ExecuteLine(aBuf);
	}
}

void CGameContext::ConClearVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "cleared votes");
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	pSelf->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
	pSelf->m_pVoteOptionHeap->Reset();
	pSelf->m_pVoteOptionFirst = 0;
	pSelf->m_pVoteOptionLast = 0;
	pSelf->m_NumVoteOptions = 0;
}

void CGameContext::ConVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	// check if there is a vote running
	if (!pSelf->m_VoteCloseTime)
		return;

	if (str_comp_nocase(pResult->GetString(0), "yes") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_YES;
	else if (str_comp_nocase(pResult->GetString(0), "no") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "admin forced vote %s", pResult->GetString(0));
	pSelf->SendChatTarget(-1, aBuf);
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pResult->GetString(0));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if (pResult->NumArguments())
	{
		CNetMsg_Sv_Motd Msg;
		Msg.m_pMessage = g_Config.m_SvMotd;
		CGameContext *pSelf = (CGameContext *)pUserData;
		for (int i = 0; i < MAX_CLIENTS; ++i)
			if (pSelf->m_apPlayers[i])
				pSelf->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
	}
}

void CGameContext::ConMute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID = pResult->GetInteger(0);
	if (CID < 0 || CID >= MAX_CLIENTS || !pSelf->m_apPlayers[CID])
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Invalid ClientID");
	else
		pSelf->AddMute(CID, pResult->GetInteger(1));
}

void CGameContext::ConMutes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[128];
	int Sec, Count = 0;
	pSelf->CleanMutes();
	for (int i = 0; i < MAX_MUTES; i++)
	{
		if (pSelf->m_aMutes[i].m_aIP[0] == 0)
			continue;

		Sec = (pSelf->m_aMutes[i].m_Expires - pSelf->Server()->Tick()) / pSelf->Server()->TickSpeed();
		str_format(aBuf, sizeof(aBuf), "#%d: %s for %d:%02d min", i, pSelf->m_aMutes[i].m_aIP, Sec / 60, Sec % 60);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		Count++;
	}
	str_format(aBuf, sizeof(aBuf), "%d mute(s)", Count);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
}

void CGameContext::ConUnmuteID(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID = pResult->GetInteger(0);
	if (CID < 0 || CID >= MAX_CLIENTS || !pSelf->m_apPlayers[CID])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Invalid ClientID");
	}
	else
		pSelf->AddMute(CID, 0);
}

void CGameContext::ConUnmuteIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int MuteID = pResult->GetInteger(0);
	char aBuf[128];

	if (MuteID < 0 || MuteID >= MAX_MUTES || pSelf->Muted(pSelf->m_aMutes[MuteID].m_aIP) == -1)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "mute not found");
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "unmuted %s", pSelf->m_aMutes[MuteID].m_aIP);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		pSelf->AddMute(pSelf->m_aMutes[MuteID].m_aIP, 0);
	}
}

void CGameContext::ConKill(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID = pResult->GetInteger(0);
	if (CID < -1 || CID >= MAX_CLIENTS || !pSelf->m_apPlayers[CID])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Invalid ClientID");
		return;
	}
	if (pSelf->m_apPlayers[CID]->GetCharacter() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Player is already dead");
		return;
	}
	if (CID == -1)
	{
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (pSelf->m_apPlayers[i])
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "You have been killed by the admin.");
				pSelf->SendChatTarget(i, aBuf);
			}
		}
	}
	pSelf->m_apPlayers[CID]->KillCharacter();
	// message to console and chat
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' has been killed by the admin.", pSelf->Server()->ClientName(CID));
	pSelf->SendChatTarget(-1, aBuf);
}

/**
 * @brief Hooks the function CGameController_zCatch::MergeRankingIntoTarget(...) to the specific command
 * in the remote console.
 * @details [long description]
 *
 * @param pResult [description]
 * @param pUserData [description]
 */
void CGameContext::ConMergeRecords(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;

	// Get strings
	std::string Source(pResult->GetString(0));
	std::string Target(pResult->GetString(1));

	if (Source.compare(Target) == 0) {
		// if both strings are equal, do nothing, otherwise this would delete all records of given player.

		/* print info */
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Merging was not successful, because both nicks are equal.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		return;
	}
	char *source = (char*)malloc(MAX_NAME_LENGTH);
	str_copy(source, Source.c_str(), MAX_NAME_LENGTH);

	char *target = (char*)malloc(MAX_NAME_LENGTH);
	str_copy(target, Target.c_str(), MAX_NAME_LENGTH);

	pSelf->AddFuture(std::async(std::launch::async, &CGameController_zCatch::MergeRankingIntoTarget, pSelf, source, target));

}


/**
 * @brief Merges records of two players using their current ingame client IDs
 * @details  \see{CGameContext::ConMergeRecords}
 *
 * @param pResult Console Input
 * @param pUserData Context data(?)
 */
void CGameContext::ConMergeRecordsId(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	// Get IDs
	int Source(pResult->GetInteger(0));
	int Target(pResult->GetInteger(1));

	/*Invalid input handling */
	if (Source == Target ||
	        Source < 0 ||
	        Target < 0 ||
	        Source >= MAX_CLIENTS ||
	        Target >= MAX_CLIENTS) {
		/* print info */
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Merging was not successful, because either both IDs are equal or ID's are not valid.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		return;
	}

	char* source = (char*)malloc(sizeof(char) * MAX_NAME_LENGTH);
	char* target = (char*)malloc(sizeof(char) * MAX_NAME_LENGTH);

	str_copy(source, pSelf->Server()->ClientName(Source), MAX_NAME_LENGTH);
	str_copy(target, pSelf->Server()->ClientName(Target), MAX_NAME_LENGTH);

	/*More invalid input handling depending on what ClientName() returned*/
	if (str_comp(source, "(invalid)") == 0 ||
	        str_comp(target, "(invalid)") == 0 ||
	        str_comp(source, "(connecting)") == 0 ||
	        str_comp(target, "(connecting)") == 0) {
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Merging was not successful, because either both one or both IDs are invalid or one of them has not connected to the server yet. Please try again.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		free(source);
		free(target);
		return;
	}
	pSelf->AddFuture(std::async(std::launch::async, &CGameController_zCatch::MergeRankingIntoTarget, pSelf, source, target));
	// source and target are freed after execution of the thread within the function MergeRankingIntoTarget

}

void CGameContext::ConPrintBotData(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (pSelf->m_apPlayers[i])
		{
			char *aBuf = pSelf->m_BotDetection->GetInfoString(i);
			pSelf->SendChatTarget(-1, aBuf);
			free(aBuf);
		}
	}

}



void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	Console()->Register("tune", "si", CFGFLAG_SERVER, ConTuneParam, this, "Tune variable to value");
	Console()->Register("tune_reset", "", CFGFLAG_SERVER, ConTuneReset, this, "Reset tuning");
	Console()->Register("tune_dump", "", CFGFLAG_SERVER, ConTuneDump, this, "Dump tuning");

	Console()->Register("pause", "", CFGFLAG_SERVER, ConPause, this, "Pause/unpause game");
	Console()->Register("change_map", "?r", CFGFLAG_SERVER | CFGFLAG_STORE, ConChangeMap, this, "Change map");
	Console()->Register("restart", "?i", CFGFLAG_SERVER | CFGFLAG_STORE, ConRestart, this, "Restart in x seconds (0 = abort)");
	Console()->Register("broadcast", "r", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message");
	Console()->Register("say", "r", CFGFLAG_SERVER, ConSay, this, "Say in chat");
	Console()->Register("say_to", "i?r", CFGFLAG_SERVER, ConSayTo, this, "Say something to the ID given: say_to <ID> <Text>");
	Console()->Register("set_team", "ii?i", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team");
	Console()->Register("set_team_all", "i", CFGFLAG_SERVER, ConSetTeamAll, this, "Set team of all players to team");
	Console()->Register("speak_as", "i?r", CFGFLAG_SERVER, ConSpeakAs, this, "Speak as if you were the person with the given ID: speak_as <ID> <Text>");
	Console()->Register("swap_teams", "", CFGFLAG_SERVER, ConSwapTeams, this, "Swap the current teams");
	Console()->Register("shuffle_teams", "", CFGFLAG_SERVER, ConShuffleTeams, this, "Shuffle the current teams");
	Console()->Register("lock_teams", "", CFGFLAG_SERVER, ConLockTeams, this, "Lock/unlock teams");

	Console()->Register("add_vote", "sr", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option");
	Console()->Register("remove_vote", "s", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option");
	Console()->Register("force_vote", "ss?r", CFGFLAG_SERVER, ConForceVote, this, "Force a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options");
	Console()->Register("vote", "r", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no");

	Console()->Register("mute", "ii", CFGFLAG_SERVER, ConMute, this, "Mutes a player for x sec");
	Console()->Register("unmute", "i", CFGFLAG_SERVER, ConUnmuteID, this, "Unmutes a player by its id");
	Console()->Register("unmuteid", "i", CFGFLAG_SERVER, ConUnmuteID, this, "Unmutes a player by its client id");
	Console()->Register("unmuteip", "i", CFGFLAG_SERVER, ConUnmuteIP, this, "Removes a mute by its index");
	Console()->Register("mutes", "", CFGFLAG_SERVER, ConMutes, this, "Show all mutes");

	Console()->Register("kill", "i", CFGFLAG_SERVER, ConKill, this, "Kill a player by id");
	Console()->Register("bot_data", "", CFGFLAG_SERVER, ConPrintBotData, this, "Prints available bot data as server message.");
	Console()->Register("snapshot", "ii", CFGFLAG_SERVER, ConSnapshotId, this, "Takes a Snapshot of given id for x inputs. If you don't know how many ticks to set, use 0: snapshot <id> <different inputs>.");
	Console()->Register("snapshot_print", "i", CFGFLAG_SERVER, ConPrintSnapshotId, this, "First you need to capture two snapshots of a player in order to print them side by side. Usage: snapshot_print <ID>");
	Console()->Register("snapshot_print_core", "i", CFGFLAG_SERVER, ConPrintSnapshotIdCore, this, "First you need to capture two snapshots of a player in order to print them side by side. Usage: snapshot_print_core <ID>");
	Console()->Register("snapshot_print_input", "i", CFGFLAG_SERVER, ConPrintSnapshotIdInput, this, "First you need to capture two snapshots of a player in order to print them side by side. Usage: snapshot_print_input <ID>");
	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);

	// jxsl13 was here. Console Commands
	Console()->Register("merge_records", "ss", CFGFLAG_SERVER, ConMergeRecords, this, "Merge two records into the target username and delete source records: merge_records <source nickname> <target nickname>", IConsole::ACCESS_LEVEL_ADMIN);
	Console()->Register("merge_records_id", "ii", CFGFLAG_SERVER, ConMergeRecordsId, this, "Merge two records into the target ID and delete source ID's records: merge_records <source ID> <target ID>", IConsole::ACCESS_LEVEL_ADMIN);
	Console()->Register("save_logging_settings", "", CFGFLAG_SERVER, ConSaveTeehistorian, this, "Writes latest records to either the Sqlite Database or creates a new Teehistorian File.", IConsole::ACCESS_LEVEL_ADMIN);
	Console()->Register("track", "i", CFGFLAG_SERVER, ConTeehistorianTrackPlayer, this, "Tracks game using TeeHistorian as long as the tracked player is online.");
	Console()->Register("untrack", "i", CFGFLAG_SERVER, ConTeehistorianUntrackPlayer, this, "Stops tracking the game using TeeHistorian.");
	Console()->Register("untrack_all", "", CFGFLAG_SERVER, ConTeehistorianUntrackAllPlayers, this, "Stops tracking all players.");

	Console()->Register("list", "", CFGFLAG_SERVER, ConList, this, "Lists player information like status, but is less of a pain in the ass to handle.");
	Console()->Register("ls", "", CFGFLAG_SERVER, ConList, this, "Lists player information like status, but is less of a pain in the ass to handle.");

	Console()->Register("tracked", "", CFGFLAG_SERVER, ConTrackedPlayers, this, "Shows tracked player count");
	Console()->Register("show_banned_nicks", "", CFGFLAG_SERVER, ConShowBannedNicks, this, "Lists all banned nicks.");
	Console()->Register("unban_nick", "i", CFGFLAG_SERVER, ConRemoveFromBannedNicks, this, "Removes ID (from show_banned_nicks) from banned nicks list.");
	Console()->Register("ban_nick_by_id", "i", CFGFLAG_SERVER, ConBanNickByID, this, "Bans a nick by given ID (from ls or status command).");
	Console()->Register("ban_nick_by_name", "r", CFGFLAG_SERVER, ConBanNickByName, this, "Bans a nick by given nickname.");
	Console()->Register("show_irregular_flags", "i", CFGFLAG_SERVER, ConShowIrregularFlags, this, "Shows all irregular flags of given player id. show_irregular_flags <ID>, -1 for all players.");
	Console()->Register("show_flags_current", "i", CFGFLAG_SERVER, ConShowCurrentFlags, this, "Shows irregular flags of given player id at the current moment. show_long_term_data <ID>, -1 for all.");
	Console()->Register("show_long_term_data", "i", CFGFLAG_SERVER, ConShowLongTermData, this, "Shows the gathered data of the given <ID>.");
	Console()->Register("show_current_data", "i", CFGFLAG_SERVER, ConShowCurrentData, this, "Shows currently calculated of the given <ID>.");
	Console()->Register("give_rainbow", "i", CFGFLAG_SERVER, ConGiveRainbow, this, "Enables Rainbow for given id.");
	Console()->Register("give_rainbow_body", "i", CFGFLAG_SERVER, ConGiveRainbowBody, this, "Enables Rainbow body for given id.");
	Console()->Register("give_rainbow_feet", "i", CFGFLAG_SERVER, ConGiveRainbowFeet, this, "Enables Rainbow feet for given id.");

	Console()->Register("ban_range_level", "ii?ir", CFGFLAG_SERVER|CFGFLAG_MASTER|CFGFLAG_STORE, ConBanRangeLevel, this, "Ban ID with level of Subranges L for X minutes with reason Y: ban_range_level <ID> <L> <Minutes> <Reason>");
	Console()->Register("show_weird_client_messages", "i", CFGFLAG_SERVER|CFGFLAG_MASTER|CFGFLAG_STORE, ConShowWeirdClientMessages, this, "Lists all unique weird client messages of a player's client given the <ID>");

}

void CGameContext::ConShowWeirdClientMessages(IConsole::IResult *pResult, void *pUserData){
	CGameContext *pSelf = (CGameContext *)pUserData;
	int playerID(pResult->GetInteger(0));
	if (playerID >= 0 && playerID < MAX_CLIENTS)
	{

		if (pSelf->m_apPlayers[playerID])
		{
			std::vector<std::string> v = pSelf->m_apPlayers[playerID]->GetWeirdClientMessages();

			char aBuf[256];

			if (v.size() <= 0)
			{
				str_format(aBuf, sizeof(aBuf), "Player '%s' has got no weird client messages.", pSelf->Server()->ClientName(playerID));

				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weird_msg", aBuf);
				return;
			}


			str_format(aBuf, sizeof(aBuf), "Showing weird client messages of player '%s'. Client version %d",
		           pSelf->Server()->ClientName(playerID),
		           pSelf->m_apPlayers[playerID]->GetClientVersion());

			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weird_msg", aBuf);

			for (size_t i = 0; i < v.size(); ++i)
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weird_msg", v.at(i).c_str());
			}

		} else {
			char aBuf[32];
			str_format(aBuf, sizeof(aBuf), "no such player with id '%d'", playerID);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		}

	} else {
		char aBuf[24];
		str_format(aBuf, sizeof(aBuf), "invalid id '%d'", playerID);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
	}
}

void CGameContext::ConShowCurrentData(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf || !pResult) {
		dbg_msg("", "CGameContext::Error in ConShowLongTermData");
		return;
	}
	int playerID(pResult->GetInteger(0));
	if (playerID >= 0) {
		pSelf->PrintCurrentData(playerID);
	} else {
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (pSelf->m_apPlayers[i])
			{
				pSelf->PrintCurrentData(i);
			}
		}
	}

}

void CGameContext::ConShowLongTermData(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf || !pResult) {
		dbg_msg("", "CGameContext::Error in ConShowLongTermData");
		return;
	}
	int playerID(pResult->GetInteger(0));
	if (playerID >= 0) {
		pSelf->PrintLongTermData(playerID);
	} else {
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (pSelf->m_apPlayers[i])
			{
				pSelf->PrintLongTermData(i);
			}
		}
	}

}

void CGameContext::ConPrintSnapshotId(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int playerID(pResult->GetInteger(0));
	PrintSnapShot(pSelf, playerID, 0); // all
}

void CGameContext::ConPrintSnapshotIdCore(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int playerID(pResult->GetInteger(0));
	PrintSnapShot(pSelf, playerID, 1); // core
}

void CGameContext::ConPrintSnapshotIdInput(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int playerID(pResult->GetInteger(0));
	PrintSnapShot(pSelf, playerID, 2); // input
}

void CGameContext::ConSnapshotId(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int playerID(pResult->GetInteger(0));
	int snapshotLenth(pResult->GetInteger(1));
	if (playerID >= 0 && playerID < MAX_CLIENTS && pSelf->m_apPlayers[playerID])
	{
		if (pSelf->m_apPlayers[playerID]->IsSnapshotEnabled()) {
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "There is already a snapshot being take, please wait until that is done(%d snaps left).", pSelf->m_apPlayers[playerID]->GetSnapsLeft());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		}
		pSelf->m_apPlayers[playerID]->SetSnapshotWantedLength(snapshotLenth);
		pSelf->m_apPlayers[playerID]->EnableSnapshot();
	} else {
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Invalid id given. Try 'status', 'ls' or 'list'");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
	}
}

void CGameContext::ConShowCurrentFlags(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf || !pResult) {
		dbg_msg("", "CGameContext::Error in ConShowIrregularFlags");
		return;
	}

	int playerID(pResult->GetInteger(0));
	// id is checked in the print method
	pSelf->PrintIrregularFlags(playerID, true);
}


void CGameContext::ConShowIrregularFlags(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf || !pResult) {
		dbg_msg("", "CGameContext::Error in ConShowIrregularFlags");
		return;
	}

	int playerID(pResult->GetInteger(0));
	if (playerID >= 0)
	{
		pSelf->PrintIrregularFlags(playerID, false);
	} else {
		bool somethingPrinted = false;
		for (int p = 0; p < MAX_CLIENTS; ++p)
		{
			if (pSelf->m_apPlayers[p]) {
				// id is also checked in the print method
				somethingPrinted = somethingPrinted || pSelf->PrintIrregularFlags(p, false);
			}
		}
		if (!somethingPrinted) {
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "No irregular flags found.");
		}
	}

}

void CGameContext::ConGiveRainbowFeet(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf || !pResult) {
		dbg_msg("", "CGameContext::Error in ConGiveRainbow");
		return;
	}

	int playerID(pResult->GetInteger(0));
	if (playerID >= 0 && playerID < MAX_CLIENTS && pSelf->m_apPlayers[playerID]) {
		pSelf->m_apPlayers[playerID]->m_IsRainbowFeetTee = true;
	} else {
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Invalid id given. Try 'status', 'ls' or 'list'");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);

	}
}

void CGameContext::ConGiveRainbowBody(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf || !pResult) {
		dbg_msg("", "CGameContext::Error in ConGiveRainbow");
		return;
	}

	int playerID(pResult->GetInteger(0));
	if (playerID >= 0 && playerID < MAX_CLIENTS && pSelf->m_apPlayers[playerID]) {
		pSelf->m_apPlayers[playerID]->m_IsRainbowBodyTee = true;
	} else {
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Invalid id given. Try 'status', 'ls' or 'list'");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);

	}
}

void CGameContext::ConGiveRainbow(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	if (!pSelf || !pResult) {
		dbg_msg("", "CGameContext::Error in ConGiveRainbow");
		return;
	}

	int playerID(pResult->GetInteger(0));
	if (playerID >= 0 && playerID < MAX_CLIENTS && pSelf->m_apPlayers[playerID]) {
		pSelf->m_apPlayers[playerID]->m_IsRainbowBodyTee = true;
		pSelf->m_apPlayers[playerID]->m_IsRainbowFeetTee = true;
	} else {
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Invalid id given. Try 'status', 'ls' or 'list'");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);

	}
}

void CGameContext::ConBanRangeLevel(IConsole::IResult *pResult, void *pUserData)
{
	// id - level - minutes - reason
	//CNetBan *pThis = static_cast<CNetBan *>(pUser);
	CGameContext *pSelf = (CGameContext *)pUserData;

	if (!pSelf || !pResult) {
		dbg_msg("", "CGameContext::Error in ConBanRangeLevel");
		return;
	}
	int playerID = pResult->GetInteger(0);
	if(!pSelf->m_apPlayers[playerID]){
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (invalid player id)");
		return;
	}

	int level = pResult->GetInteger(1);
	if(level < 1 || level > 3){
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (invalid level)");
		return;
	}
	// get ip address of player with playerID
	char aIP[NETADDR_MAXSTRSIZE];
	CServer *pCServer = pSelf->GetBanServer()->Server() ;
	const NETADDR *playerAddress = pCServer->m_NetServer.ClientAddr(playerID);
	net_addr_str(playerAddress, aIP, sizeof(aIP), false);
	std::string subRange = std::string(aIP);

	// create lower and upper bounds based on that ip address
	std::string lowerBound = "";
	std::string upperBound = "";

	if (playerAddress->type == NETTYPE_WEBSOCKET_IPV4 || playerAddress->type == NETTYPE_IPV4)
	{
		for (int i = 0; i < level; ++i)
		{	
			// removes last partition of the ip address per iteration
			subRange = subRange.substr(0, subRange.find_last_of("."));
			// adds a lower and upper bound per iteration
			lowerBound.append(".0");
			upperBound.append(".255");
		}
		// resulting boundaries
		lowerBound = subRange + lowerBound;
		upperBound = subRange + upperBound;

	} else if (playerAddress->type == NETTYPE_IPV6)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (invalid IPv6 is not supported)");
		return;
		// Not working, seems to be not supported
		/**
		subRange.erase(std::remove(subRange.begin(), subRange.end(), '['), subRange.end());
		subRange.erase(std::remove(subRange.begin(), subRange.end(), ']'), subRange.end());

		for (int i = 0; i < level; ++i)
		{
			subRange = subRange.substr(0, subRange.find_last_of(":"));
			lowerBound.append(":0");
			upperBound.append(":ffff");
		}
		lowerBound = subRange + lowerBound;
		upperBound = subRange + upperBound;
		*/

	} else {
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (invalid ip version/not supported)");
		return;
	}

	const char *pStr1 = lowerBound.c_str();
	const char *pStr2 = upperBound.c_str();

	// if minutes is not set, set minutes
	int Minutes = pResult->NumArguments()>2 ? clamp(pResult->GetInteger(2), 0, 44640) : 30;
	// if reason is not given, set reason
	const char *pReason = pResult->NumArguments()>3 ? pResult->GetString(3) : "No reason given";
	CServerBan *pCServerBan = pSelf->GetBanServer();

	// ban range if valid.
	CNetRange Range;
	if(net_addr_from_str(&Range.m_LB, pStr1) == 0 && net_addr_from_str(&Range.m_UB, pStr2) == 0)
		pCServerBan->BanRange(&Range, Minutes*60, pReason);
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (invalid range)");
}


void CGameContext::ConBanNickByName(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char* nickToBan(pResult->GetString(0));
	int id = -1;
	// check all nicks of ingame players.
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (str_comp(pSelf->Server()->ClientName(i), nickToBan) == 0)
		{
			id = i;
			break;
		}
	}
	// add to ban list
	pSelf->AddToNicknameBanList(nickToBan);

	// if player is on the server, kick him.
	if (id >= 0)
	{
		char aBuf[48];
		str_format(aBuf, sizeof(aBuf), "Your nickname '%s' is banned.", pSelf->Server()->ClientName(id));
		pSelf->Server()->Kick(id, aBuf);
	}

}

void CGameContext::ConBanNickByID(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int playerID(pResult->GetInteger(0));
	pSelf->AddToNicknameBanList(playerID);

	char aBuf[48];
	str_format(aBuf, sizeof(aBuf), "Your nickname '%s' is banned.", pSelf->Server()->ClientName(playerID));
	pSelf->Server()->Kick(playerID, aBuf);
}

void CGameContext::ConRemoveFromBannedNicks(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int bannedId(pResult->GetInteger(0));
	pSelf->RemoveFromNicknameBanList(bannedId);
}

void CGameContext::ConShowBannedNicks(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[128];
	long size = pSelf->m_BannedNicks.size();

	if (size == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BannedNicks", "There are no nicknames on the banned nicks list.");
		return;
	}

	const char* tempNick;
	pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "NickBans", "=========== Banned Nicks ===========");
	for (int i = 0; i < size; ++i)
	{
		tempNick = pSelf->m_BannedNicks.at(i).c_str();
		str_format(aBuf, sizeof(aBuf), "%3d : %s", i, tempNick);
		pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "NickBans", aBuf);
	}
}

void CGameContext::ConTrackedPlayers(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Tracked player count: %d", pSelf->m_TeeHistorian.GetTrackedPlayersCount());
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);
}

void CGameContext::ConList(IConsole::IResult *pResult, void *pUserData) {


	CGameContext *pSelf = (CGameContext *)pUserData;

	if (pSelf == 0) {
		return;
	}


	// banned nicks stuff ###############################################################
	long size = pSelf->m_BannedNicks.size();

	if (size > 0)
	{
		char aBuf[128];
		const char* tempNick;
		str_format(aBuf, sizeof(aBuf), "============================== Banned Nicks(%ld) ==============================", size);
		pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "NickBans", aBuf);
		for (int i = 0; i < size; ++i)
		{
			tempNick = pSelf->m_BannedNicks.at(i).c_str();
			str_format(aBuf, sizeof(aBuf), "%3d : %-20s", i, tempNick);
			pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "NickBans", aBuf);
		}
	}
	// banned nicks stuff end ###########################################################

	// bans stuff

	CNetBan *pThis = static_cast<CNetBan*>(pSelf->GetBanServer());

	if (pThis->m_BanAddrPool.First() || pThis->m_BanRangePool.First()) {
		int cntBan = 0;
		int cntRange = 0;
		for (CNetBan::CBanAddr *pBan = pThis->m_BanAddrPool.First(); pBan; pBan = pBan->m_pNext)
		{
			cntBan++;
		}
		for (CNetBan::CBanRange *pBan = pThis->m_BanRangePool.First(); pBan; pBan = pBan->m_pNext)
		{
			cntRange++;
		}

		int Count = 0;
		char aBuf[256], aMsg[256];
		if (pThis->m_BanAddrPool.First()) {
			str_format(aMsg, sizeof(aMsg), "================================= Bans(%i) =================================", cntBan);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aMsg);

			for (CNetBan::CBanAddr *pBan = pThis->m_BanAddrPool.First(); pBan; pBan = pBan->m_pNext)
			{
				pThis->MakeBanInfo(pBan, aBuf, sizeof(aBuf), CNetBan::MSGTYPE_LIST);
				str_format(aMsg, sizeof(aMsg), "#%i %s", Count++, aBuf);

				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aMsg);
			}
		}

		if (pThis->m_BanRangePool.First()) {
			str_format(aMsg, sizeof(aMsg), "================================= Range Bans(%i) =================================", cntRange);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aMsg);

			for (CNetBan::CBanRange *pBan = pThis->m_BanRangePool.First(); pBan; pBan = pBan->m_pNext)
			{
				pThis->MakeBanInfo(pBan, aBuf, sizeof(aBuf), CNetBan::MSGTYPE_LIST);
				str_format(aMsg, sizeof(aMsg), "#%i %s", Count++, aBuf);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aMsg);
			}
		}

	}
	// bans stuff end


	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "================================= Player List =================================");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "ID : [SecureConnection][FlagsIrregular][VersionIrregular][Version][Tracked][IP][AdminLevel][Name][Clan]);");
	CServer* pCServer = pSelf->GetBanServer()->Server();
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (pSelf->m_apPlayers[i] && pCServer->m_aClients[i].m_State != CServer::CClient::STATE_EMPTY) {

			// variables needed
			char aBuf[256];
			char aID[8];
			char aName[MAX_NAME_LENGTH];
			char aClan[MAX_CLAN_LENGTH];
			char aIP[NETADDR_MAXSTRSIZE];
			char aTracked[4];
			char aClientVersion[24];
			char aAdminLevel[4];
			char aFlags[48];
			char aSecureConnection[5];


			str_format(aID, sizeof(aID), "%-2d", i);
			str_format(aName, sizeof(aName), "%s", pSelf->Server()->ClientName(i));
			str_format(aClan, sizeof(aClan), "%s", pSelf->Server()->ClientClan(i));

			net_addr_str(pCServer->m_NetServer.ClientAddr(i), aIP, sizeof(aIP), true);

			str_format(aFlags, sizeof(aFlags), "[%s]", pSelf->m_apPlayers[i]->HasIrregularFlags() ? "I" : "N");

			str_format(aClientVersion, sizeof(aClientVersion), "%s[%6d]",
			           pSelf->m_apPlayers[i]->HasIrregularClientVersion() ? "[I]" : "[N]",
			           pSelf->m_apPlayers[i]->GetClientVersion());

			str_format(aTracked, sizeof(aTracked), "%s",
			           pSelf->m_apPlayers[i]->GetTeeHistorianTracked() ? "[T]" : "[N]");

			str_format(aSecureConnection, sizeof(aSecureConnection), "%s",
			           pCServer->m_NetServer.HasSecurityToken(i) ? "[S]" : "[N]");

			switch (pSelf->Server()->GetAuthLevel(i)) {
			case CServer::AUTHED_ADMIN:
				str_format(aAdminLevel, sizeof(aAdminLevel), "%s", "[A]");
				break;
			case CServer::AUTHED_SUBADMIN:
				str_format(aAdminLevel, sizeof(aAdminLevel), "%s", "[S]");
				break;
			case CServer::AUTHED_MOD:
				str_format(aAdminLevel, sizeof(aAdminLevel), "%s", "[M]");
				break;
			default:
				str_format(aAdminLevel, sizeof(aAdminLevel),  "%s", "[N]");
				break;
			}

			str_format(aBuf, sizeof(aBuf), "%s: %3s%-3s%-3s%-3s  %-22s %3s %-16s  %12s ", aID, aSecureConnection, aFlags, aClientVersion, aTracked, aIP, aAdminLevel, aName, aClan);

			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);

		}
	}
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "=========================================================================");
}


void CGameContext::ConSaveTeehistorian(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	// if tracking is enabled, do not allow to change the settings.
	if (pSelf->m_TeeHistorian.GetTrackedPlayersCount() == 0)
	{
		pSelf->m_TeeHistorian.Stop();
		pSelf->AddFuture(std::async(std::launch::async, &CTeeHistorian::OnSave, &(pSelf->m_TeeHistorian)));
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Saved logging settings.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);
	} else {
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Could not save settings because player tracking is enabled.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);
	}
}

void CGameContext::ConTeehistorianUntrackAllPlayers(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int countPlayers = 0;
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeeHistorianTracked()) {
			pSelf->m_apPlayers[i]->SetTeeHistorianTracked(false);
			countPlayers++;
		}
	}

	// Send console message
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Untracked %d player.", countPlayers);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);

	// Disable tracking if nobody is being tracked anymore and save teehistorian file.
	if (pSelf->m_TeeHistorian.GetTrackedPlayersCount() == 0) {
		ConSaveTeehistorian(pResult, pUserData);
	}

}

void CGameContext::ConTeehistorianUntrackPlayer(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int trackedId(pResult->GetInteger(0));


	// illegal id: out of player id range
	if (trackedId < 0 || trackedId > MAX_CLIENTS) {
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Given ID \'%d\' is not valid.", trackedId);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);

	} else if (pSelf->m_apPlayers[trackedId]) // player exists
	{
		// player is being tracked
		if (pSelf->m_apPlayers[trackedId]->GetTeeHistorianTracked())
		{
			char aBuf[128];
			pSelf->m_apPlayers[trackedId]->SetTeeHistorianTracked(false);
			str_format(aBuf, sizeof(aBuf), "\'%s\' is not being tracked anymore!", pSelf->Server()->ClientName(trackedId));
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);
			// Disable tracking if nobody is being tracked anymore
			if (pSelf->m_TeeHistorian.GetTrackedPlayersCount() == 0) {
				ConSaveTeehistorian(pResult, pUserData);
			}

		} else { // player is not being tracked
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "\'%s\' is not being tracked!", pSelf->Server()->ClientName(trackedId));
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);
		}
	} else	// id is valid, but there is no such player on the server.
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "There is no player with the id \'%d\' online", trackedId);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);
	}

}

void CGameContext::ConTeehistorianTrackPlayer(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int trackedId(pResult->GetInteger(0));

	if (pSelf->m_TeeHistorian.IsPlayerTrackingEnabled()) {
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "\'%s\' is already being tracked. You can track at most one player. ",
		           pSelf->Server()->ClientName(pSelf->m_TeeHistorian.GetFirstTrackedPlayerId()));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);
		return;
	}

	// illegal id: out of player id range
	if (trackedId < 0 || trackedId > MAX_CLIENTS) {
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Given ID \'%d\' is not valid.", trackedId);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);

	} else if (pSelf->m_apPlayers[trackedId]) // player exists
	{
		// player is already being tracked
		if (pSelf->m_apPlayers[trackedId]->GetTeeHistorianTracked())
		{
			char aBuf[48];
			str_format(aBuf, sizeof(aBuf), "Player is already being tracked!");
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);
		} else { // player is not yet being tracked
			if (pSelf->m_TeeHistorian.GetTrackedPlayersCount() == 0) {
				ConSaveTeehistorian(pResult, pUserData);
			}
			pSelf->m_apPlayers[trackedId]->SetTeeHistorianTracked(true);
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "\'%s\' is now being tracked!", pSelf->Server()->ClientName(trackedId));
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);
		}
	} else	// id is valid, but there is no such player on the server.
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "There is no player with the id \'%d\' online", trackedId);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "TeeHistorian", aBuf);
	}
}

void CGameContext::OnInit(/*class IKernel *pKernel*/)
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);

	/*teehistorian*/
	Console()->SetTeeHistorianCommandCallback(CommandCallback, this);


	//if(!data) // only load once
	//data = load_data_from_memory(internal_data);

	for (int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers);

	// reset everything here
	//world = new GAMEWORLD;
	//players = new CPlayer[MAX_CLIENTS];

	/* open ranking system db */
	if (g_Config.m_SvRanking == 1)
	{
		int rc = sqlite3_open(g_Config.m_SvRankingFile, &m_RankingDb);
		if (rc) {
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Can't open database (#%d): %s\n", rc, sqlite3_errmsg(m_RankingDb));
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
			sqlite3_close(m_RankingDb);
			exit(1);
		}
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", "SQLite3 database opened");
		/* wait up to 5 seconds if the db is used */
		sqlite3_busy_timeout(m_RankingDb, 5000);
	}




	// select gametype
	/*if(str_comp(g_Config.m_SvGametype, "mod") == 0)
		m_pController = new CGameControllerMOD(this);
	else if(str_comp(g_Config.m_SvGametype, "ctf") == 0)
		m_pController = new CGameControllerCTF(this);
	else if(str_comp(g_Config.m_SvGametype, "tdm") == 0)
		m_pController = new CGameControllerTDM(this);
	else if(str_comp_nocase(g_Config.m_SvGametype, "zcatch") == 0)
		m_pController = new CGameController_zCatch(this);
	else
		m_pController = new CGameControllerDM(this);*/
	m_pController = new CGameController_zCatch(this);

	m_BotDetection = new CBotDetection(this);

	/* ranking system */
	if (RankingEnabled())
	{
		m_pController->OnInitRanking(m_RankingDb);
	}

	// setup core world
	//for(int i = 0; i < MAX_CLIENTS; i++)
	//	game.players[i].core.world = &game.world.core;

	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);



	/*
	num_spawn_points[0] = 0;
	num_spawn_points[1] = 0;
	num_spawn_points[2] = 0;
	*/

	for (int y = 0; y < pTileMap->m_Height; y++)
	{
		for (int x = 0; x < pTileMap->m_Width; x++)
		{
			int Index = pTiles[y * pTileMap->m_Width + x].m_Index;

			if (Index >= ENTITY_OFFSET)
			{
				vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
				m_pController->OnEntity(Index - ENTITY_OFFSET, Pos);
			}
		}
	}

	//game.world.insert_entity(game.Controller);

	// after everything else is ready? teehistorian
	m_TeeHistorian.RetrieveMode(true);
	m_TeeHistorian.OnInit(Storage(), Server(), m_pController, Tuning(), this);

	if (m_TeeHistorian.GetMode())
	{

		for (int i = 0; i < MAX_CLIENTS; i++)
		{

			if (Server()->IsAuthed(i))
			{
				m_TeeHistorian.RecordAuthInitial(i, Server()->GetAuthLevel(i), Server()->GetAuthName(i));
			}
		}

	}

	// retrieve banned nicks list and sort
	InitNicknameBanList();



#ifdef CONF_DEBUG
	if (g_Config.m_DbgDummies)
	{
		for (int i = 0; i < g_Config.m_DbgDummies ; i++)
		{
			OnClientConnected(MAX_CLIENTS - i - 1);
		}
	}
#endif
}

void CGameContext::OnShutdown()
{
	SaveNicknameBanListToFile();
	m_TeeHistorian.OnShutDown(true);
	delete m_pController;
	m_pController = 0;

	Clear();
}

void CGameContext::OnSnap(int ClientID)
{
	// add tuning to demo
	CTuningParams StandardTuning;
	if (ClientID == -1 && Server()->DemoRecorder_IsRecording() && mem_comp(&StandardTuning, &m_Tuning, sizeof(CTuningParams)) != 0)
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		int *pParams = (int *)&m_Tuning;
		for (unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
			Msg.AddInt(pParams[i]);
		Server()->SendMsg(&Msg, MSGFLAG_RECORD | MSGFLAG_NOSEND, ClientID);
	}

	m_World.Snap(ClientID);
	m_pController->Snap(ClientID);
	m_Events.Snap(ClientID);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i])
			m_apPlayers[i]->Snap(ClientID);
	}
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	m_Events.Clear();
}

bool CGameContext::IsClientReady(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsReady ? true : false;
}

bool CGameContext::IsClientPlayer(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS ? false : true;
}

// old bot detection
/*
bool CGameContext::IsClientAimBot(int ClientID)
{
return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsAimBot;
}
*/

/**

 *
 * @param mode 0 - all, 1 - core, 2 - input
 */
void CGameContext::PrintSnapShot(CGameContext *pSelf, int playerID, int mode) {
	CPlayer *player = pSelf->m_apPlayers[playerID];
	int cellWidth = 5;

	bool core = false;
	bool input = false;
	switch (mode) {
	case 1:
		core = true;
		break;
	case 2:
		input = true;
		break;
	default:
		core = true;
		input = true;
	}

	if (playerID >= 0 && playerID < MAX_CLIENTS && player)
	{
		// we need to be at screenshot 2, number = 0 to be able to print correct data here.

		if (player->IsSnapshotFull()) {
			std::vector<TickPlayer> snapOne = player->GetFirstSnapshotResult();
			std::vector<TickPlayer> snapTwo = player->GetSecondSnapshotResult();

			std::stringstream Tick;
			std::stringstream Core_X;
			std::stringstream Core_Y;
			std::stringstream Core_VelX;
			std::stringstream Core_VelY;
			std::stringstream Core_Angle;
			std::stringstream Core_Direction;
			std::stringstream Core_Jumped;
			std::stringstream Core_HookedPlayer;
			std::stringstream Core_HookState;
			std::stringstream Core_HookTick;
			std::stringstream Core_HookX;
			std::stringstream Core_HookY;
			std::stringstream Core_HookDx;
			std::stringstream Core_HookDy;
			std::stringstream Input_Direction;
			std::stringstream Input_TargetX;
			std::stringstream Input_TargetY;
			std::stringstream Input_Jump;
			std::stringstream Input_Fire;
			std::stringstream Input_Hook;
			std::stringstream Input_PlayerFlags;
			std::stringstream Input_WantedWeapon;
			std::stringstream Input_NextWeapon;
			std::stringstream Input_PrevWeapon;

			Tick << std::left << std::setw(20) << "Tick" << std::right;
			// set line header
			if (core)
			{
				Core_X << std::left << std::setw(20) << "Core_X" << std::right;
				Core_Y << std::left << std::setw(20) << "Core_Y" << std::right;
				Core_VelX << std::left << std::setw(20) << "Core_VelX" << std::right;
				Core_VelY << std::left << std::setw(20) << "Core_VelY" << std::right;
				Core_Angle << std::left << std::setw(20) << "Core_Angle" << std::right;
				Core_Direction << std::left << std::setw(20) << "Core_Direction" << std::right;
				Core_Jumped << std::left << std::setw(20) << "Core_Jumped" << std::right;
				Core_HookedPlayer << std::left << std::setw(20) << "Core_HookedPlayer" << std::right;
				Core_HookState << std::left << std::setw(20) << "Core_HookState" << std::right;
				Core_HookTick << std::left << std::setw(20) << "Core_HookTick" << std::right;
				Core_HookX << std::left << std::setw(20) << "Core_HookX" << std::right;
				Core_HookY << std::left << std::setw(20) << "Core_HookY" << std::right;
				Core_HookDx << std::left << std::setw(20) << "Core_HookDx" << std::right;
				Core_HookDy << std::left << std::setw(20) << "Core_HookDy" << std::right;
			}

			if (input)
			{
				Input_Direction << std::left << std::setw(20) << "Input_Direction" << std::right;
				Input_TargetX << std::left << std::setw(20) << "Input_TargetX" << std::right;
				Input_TargetY << std::left << std::setw(20) << "Input_TargetY" << std::right;
				Input_Jump << std::left << std::setw(20) << "Input_Jump" << std::right;
				Input_Fire << std::left << std::setw(20) << "Input_Fire" << std::right;
				Input_Hook << std::left << std::setw(20) << "Input_Hook" << std::right;
				Input_PlayerFlags << std::left << std::setw(20) << "Input_PlayerFlags" << std::right;
				Input_WantedWeapon << std::left << std::setw(20) << "Input_WantedWeapon" << std::right;
				Input_NextWeapon << std::left << std::setw(20) << "Input_NextWeapon" << std::right;
				Input_PrevWeapon << std::left << std::setw(20) << "Input_PrevWeapon" << std::right;
			}
			for (size_t i = 0; i < snapOne.size(); ++i)
			{
				Tick << std::setw(cellWidth) << snapOne.at(i).m_Tick << " ";
				if (core)
				{
					Core_X << std::setw(cellWidth) << snapOne.at(i).m_Core_X << " ";
					Core_Y << std::setw(cellWidth) << snapOne.at(i).m_Core_Y << " ";
					Core_VelX << std::setw(cellWidth) << snapOne.at(i).m_Core_VelX << " ";
					Core_VelY << std::setw(cellWidth) << snapOne.at(i).m_Core_VelY << " ";
					Core_Angle << std::setw(cellWidth) << snapOne.at(i).m_Core_Angle << " ";
					Core_Direction << std::setw(cellWidth) << snapOne.at(i).m_Core_Direction << " ";
					Core_Jumped << std::setw(cellWidth) << snapOne.at(i).m_Core_Jumped << " ";
					Core_HookedPlayer << std::setw(cellWidth) << snapOne.at(i).m_Core_HookedPlayer << " ";
					Core_HookState << std::setw(cellWidth) << snapOne.at(i).m_Core_HookState << " ";
					Core_HookTick << std::setw(cellWidth) << snapOne.at(i).m_Core_HookTick << " ";
					Core_HookX << std::setw(cellWidth) << snapOne.at(i).m_Core_HookX << " ";
					Core_HookY << std::setw(cellWidth) << snapOne.at(i).m_Core_HookY << " ";
					Core_HookDx << std::setw(cellWidth) << snapOne.at(i).m_Core_HookDx << " ";
					Core_HookDy << std::setw(cellWidth) << snapOne.at(i).m_Core_HookDy << " ";
				}

				if (input) {
					Input_Direction << std::setw(cellWidth) << snapOne.at(i).m_Input_Direction << " ";
					Input_TargetX << std::setw(cellWidth) << snapOne.at(i).m_Input_TargetX << " ";
					Input_TargetY << std::setw(cellWidth) << snapOne.at(i).m_Input_TargetY << " ";
					Input_Jump << std::setw(cellWidth) << snapOne.at(i).m_Input_Jump << " ";
					Input_Fire << std::setw(cellWidth) << snapOne.at(i).m_Input_Fire << " ";
					Input_Hook << std::setw(cellWidth) << snapOne.at(i).m_Input_Hook << " ";
					Input_PlayerFlags << std::setw(cellWidth) << snapOne.at(i).m_Input_PlayerFlags << " ";
					Input_WantedWeapon << std::setw(cellWidth) << snapOne.at(i).m_Input_WantedWeapon << " ";
					Input_NextWeapon << std::setw(cellWidth) << snapOne.at(i).m_Input_NextWeapon << " ";
					Input_PrevWeapon << std::setw(cellWidth) << snapOne.at(i).m_Input_PrevWeapon << " ";
				}
			}

			Tick << " | ";
			if (core)
			{
				Core_X << " | ";
				Core_Y << " | ";
				Core_VelX << " | ";
				Core_VelY << " | ";
				Core_Angle << " | ";
				Core_Direction << " | ";
				Core_Jumped << " | ";
				Core_HookedPlayer << " | ";
				Core_HookState << " | ";
				Core_HookTick << " | ";
				Core_HookX << " | ";
				Core_HookY << " | ";
				Core_HookDx << " | ";
				Core_HookDy << " | ";
			}
			if (input)
			{
				Input_Direction << " | ";
				Input_TargetX << " | ";
				Input_TargetY << " | ";
				Input_Jump << " | ";
				Input_Fire << " | ";
				Input_Hook << " | ";
				Input_PlayerFlags << " | ";
				Input_WantedWeapon << " | ";
				Input_NextWeapon << " | ";
				Input_PrevWeapon << " | ";
			}
			for (size_t i = 0; i < snapTwo.size(); ++i)
			{
				Tick << std::setw(cellWidth) << snapTwo.at(i).m_Tick << " ";
				if (core)
				{
					Core_X << std::setw(cellWidth) << snapTwo.at(i).m_Core_X << " ";
					Core_Y << std::setw(cellWidth) << snapTwo.at(i).m_Core_Y << " ";
					Core_VelX << std::setw(cellWidth) << snapTwo.at(i).m_Core_VelX << " ";
					Core_VelY << std::setw(cellWidth) << snapTwo.at(i).m_Core_VelY << " ";
					Core_Angle << std::setw(cellWidth) << snapTwo.at(i).m_Core_Angle << " ";
					Core_Direction << std::setw(cellWidth) << snapTwo.at(i).m_Core_Direction << " ";
					Core_Jumped << std::setw(cellWidth) << snapTwo.at(i).m_Core_Jumped << " ";
					Core_HookedPlayer << std::setw(cellWidth) << snapTwo.at(i).m_Core_HookedPlayer << " ";
					Core_HookState << std::setw(cellWidth) << snapTwo.at(i).m_Core_HookState << " ";
					Core_HookTick << std::setw(cellWidth) << snapTwo.at(i).m_Core_HookTick << " ";
					Core_HookX << std::setw(cellWidth) << snapTwo.at(i).m_Core_HookX << " ";
					Core_HookY << std::setw(cellWidth) << snapTwo.at(i).m_Core_HookY << " ";
					Core_HookDx << std::setw(cellWidth) << snapTwo.at(i).m_Core_HookDx << " ";
					Core_HookDy << std::setw(cellWidth) << snapTwo.at(i).m_Core_HookDy << " ";
				}

				if (input)
				{
					Input_Direction << std::setw(cellWidth) << snapTwo.at(i).m_Input_Direction << " ";
					Input_TargetX << std::setw(cellWidth) << snapTwo.at(i).m_Input_TargetX << " ";
					Input_TargetY << std::setw(cellWidth) << snapTwo.at(i).m_Input_TargetY << " ";
					Input_Jump << std::setw(cellWidth) << snapTwo.at(i).m_Input_Jump << " ";
					Input_Fire << std::setw(cellWidth) << snapTwo.at(i).m_Input_Fire << " ";
					Input_Hook << std::setw(cellWidth) << snapTwo.at(i).m_Input_Hook << " ";
					Input_PlayerFlags << std::setw(cellWidth) << snapTwo.at(i).m_Input_PlayerFlags << " ";
					Input_WantedWeapon << std::setw(cellWidth) << snapTwo.at(i).m_Input_WantedWeapon << " ";
					Input_NextWeapon << std::setw(cellWidth) << snapTwo.at(i).m_Input_NextWeapon << " ";
					Input_PrevWeapon << std::setw(cellWidth) << snapTwo.at(i).m_Input_PrevWeapon << " ";
				}
			}

			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Tick.str().c_str());
			if (core)
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_Y.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_Y.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_VelX.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_VelY.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_Angle.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_Direction.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_Jumped.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_HookedPlayer.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_HookState.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_HookTick.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_HookX.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_HookY.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_HookDx.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Core_HookDy.str().c_str());
			}

			if (input)
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Input_Direction.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Input_TargetX.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Input_TargetY.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Input_Jump.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Input_Fire.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Input_Hook.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Input_PlayerFlags.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Input_WantedWeapon.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Input_NextWeapon.str().c_str());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Snapshot", Input_PrevWeapon.str().c_str());
			}

		} else {
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Could not print snapshot, please try doing more snapshots before printing.");
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		}
	} else {
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Invalid id given. Try 'status', 'ls' or 'list'");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
	}
}

bool CGameContext::PrintIrregularFlags(int ClientID, bool currentFlags) {
	if (ClientID >= 0 && ClientID < MAX_CLIENTS && m_apPlayers[ClientID]) {
		std::vector<int> v;
		if (!currentFlags) {
			v = m_apPlayers[ClientID]->GetIrregularFlags();
		} else {
			v = {m_apPlayers[ClientID]->m_PlayerFlags};
		}

		char aBuf[128];
		if (v.size() <= 0)
		{
			// don't print, because it is quite spammy.
			//str_format(aBuf, sizeof(aBuf), "Player '%s' has no %s flags.", Server()->ClientName(ClientID), currentFlags ? "current" : "irregular");
			//Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
			return false;
		}
		str_format(aBuf, sizeof(aBuf), "Showing %s flags of player '%s'. Client version %d",
		           currentFlags ? "current" : "irregular ",
		           Server()->ClientName(ClientID),
		           m_apPlayers[ClientID]->GetClientVersion());

		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);

		for (size_t i = 0; i < v.size(); ++i)
		{
			std::bitset<32> currentFlagMask = CPlayer::ConvertToBitMask(v.at(i));
			// show bit mask instead of integer
			std::stringstream s;

			for (size_t j = 0; j < currentFlagMask.size(); ++j) {
				s << currentFlagMask.test(j);
			}

			// flips string in order to have the small bit at the right side.
			std::string result = s.str();
			std::reverse(result.begin(), result.end());

			// print flag mask as string with at least 32 bits
			str_format(aBuf, sizeof(aBuf), "%s Value: %d", result.c_str(), v.at(i));
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		}
		return true;

	} else {
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Invalid id given.");
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		return false;
	}
}

void CGameContext::PrintStringStreamToConsoleAndReset(std::stringstream &s, const char* ConsoleLabel){
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, ConsoleLabel, s.str().c_str());
	s.str(std::string());
}

void CGameContext::PrintLongTermData(int ClientID) {
	if (ClientID >= 0 && ClientID < MAX_CLIENTS && m_apPlayers[ClientID]) {
		char aBuf[64];
		std::stringstream s;
		int headerColumnWidth = 64;
		int singleNumberColumnWidth = 10;

		// header
		str_format(aBuf, sizeof(aBuf), "Showing long term data of player '%s'.", Server()->ClientName(ClientID));
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "Long_Term_Data", aBuf);

		s << std::setw(headerColumnWidth) << std::left << "Biggest Cursor Distance: " << std::setw(singleNumberColumnWidth) << std::right << m_apPlayers[ClientID]->GetBiggestCursorDistanceFromTee();
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << std::setw(headerColumnWidth) << std::left << "Zoom Indication Counter: " << std::setw(singleNumberColumnWidth) << std::right << m_apPlayers[ClientID]->GetZoomIndicatorCounter();
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << std::setw(headerColumnWidth) << std::left << "Zoom distances: ";
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << m_apPlayers[ClientID]->GetZoomIndicationDistances();
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << std::setw(headerColumnWidth) << std::left << "Longest Distance/Tick of three inputs: " << std::setw(singleNumberColumnWidth) << std::right << m_apPlayers[ClientID]->GetLongestDistancePerTickOfThreeConsequtiveMousePositions();
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << std::setw(headerColumnWidth) << std::left << "Inputs of longest Distance/Tick:";
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << CPlayer::ConvertToString(m_apPlayers[ClientID]->GetThreeConsequtiveMousePositionsWithLongestDistance());
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << "Nearly identical 1st & 3rd input: ";
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << std::setw(headerColumnWidth) << std::left << "Distance/Tick: " << std::setw(singleNumberColumnWidth) << std::right << m_apPlayers[ClientID]->GetDistancePerTickOfNearlyIdenticalFirstAndLastPosition();
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << std::setw(headerColumnWidth) << std::left << "Distance travelled: " << std::setw(singleNumberColumnWidth) << std::right << m_apPlayers[ClientID]->GetDistanceOfNearlyIdenticalFirstAndLastPosition();
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << std::setw(headerColumnWidth) << std::left << "Fast Aim Indication Counter: " << std::setw(singleNumberColumnWidth) << std::right << m_apPlayers[ClientID]->GetNearlyIdenticalFirstAndLastPositionCounter();
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << std::setw(headerColumnWidth) << std::left << "Most striking inputs:";
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

		s << CPlayer::ConvertToString(m_apPlayers[ClientID]->GetThreeConsequtiveMousePositionsWithNearlyIdenticalFirstAndLastPosition());
		PrintStringStreamToConsoleAndReset(s, "Long_Term_Data");

	} else {
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "Server", "Invalid id given.");
	}
}


void CGameContext::PrintCurrentData(int ClientID) {
	if (ClientID >= 0 && ClientID < MAX_CLIENTS && m_apPlayers[ClientID]) {
		char aBuf[64];
		std::stringstream s;
		// header
		str_format(aBuf, sizeof(aBuf), "Showing current data of player '%s'.", Server()->ClientName(ClientID));
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Current_Data", aBuf);
		// line
		s << "Cursor Distance from Player: " << m_apPlayers[ClientID]->GetCurrentDistanceFromTee();
		// print line
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Current_Data", s.str().c_str());
		s.str(std::string());

		s << "Current ViewPos: x: " << m_apPlayers[ClientID]->m_ViewPos.x << " y: " <<  m_apPlayers[ClientID]->m_ViewPos.y;
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Current_Data", s.str().c_str());
		// reset, build and print next line
	} else {
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Invalid id given.");
	}
}


void CGameContext::CGameContext::RetrieveNicknameBanListFromFile() {
	// exec the file
	IOHANDLE File = m_pStorage->OpenFile(g_Config.m_SvNickBanFile, IOFLAG_READ, IStorage::TYPE_ALL);

	char aBuf[256];
	if (File)
	{
		char *pLine;
		CLineReader lr;

		str_format(aBuf, sizeof(aBuf), "executing '%s'", g_Config.m_SvNickBanFile);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BannedNicks", aBuf);

		// only non whiespace nicks will be added.
		lr.Init(File);
		std::string line;
		while ((pLine = lr.Get())) {
			line = std::string(pLine);
			trim(line);
			if (line.size() > 0)
			{
				AddToNicknameBanList(line.c_str());
			}
		}

		io_close(File);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", g_Config.m_SvNickBanFile);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BannedNicks", aBuf);
	}

}

void CGameContext::InitNicknameBanList() {
	RetrieveNicknameBanListFromFile();
	std::sort(m_BannedNicks.begin(), m_BannedNicks.end());
	m_BannedNicks.erase( unique( m_BannedNicks.begin(), m_BannedNicks.end() ), m_BannedNicks.end() );
}

void CGameContext::SaveNicknameBanListToFile() {

	IOHANDLE File = m_pStorage->OpenFile(g_Config.m_SvNickBanFile, IOFLAG_WRITE, IStorage::TYPE_ALL);

	char aBuf[256];
	if (File)
	{
		if (!m_BannedNicks.empty())
		{
			std::string tempString;
			while (!m_BannedNicks.empty()) {
				// get nick from collection
				tempString = m_BannedNicks.back();
				m_BannedNicks.pop_back();

				str_format(aBuf, sizeof(aBuf), "%s", tempString.c_str());
				io_write(File, aBuf, strlen(aBuf));
				io_write_newline(File);
			}
		} else {
			//io_write_newline(File);
		}
		io_close(File);
	} else {
		str_format(aBuf, sizeof(aBuf), "failed to open '%s'", g_Config.m_SvNickBanFile);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BannedNicks", aBuf);
	}
}

void CGameContext::AddToNicknameBanList(int ID) {
	char aBuf[256];
	if (!m_apPlayers[ID] || ID < 0 || ID > MAX_CLIENTS)
	{
		str_format(aBuf, sizeof(aBuf), "Invalid ID: %d", ID);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BannedNicks", aBuf);
	} else {
		AddToNicknameBanList(Server()->ClientName(ID));
	}
}

void CGameContext::AddToNicknameBanList(const char* Nickname) {
	AddToNicknameBanList(std::string(Nickname));
}

void CGameContext::AddToNicknameBanList(std::string Nickname) {
	m_BannedNicks.push_back(Nickname);
	std::sort(m_BannedNicks.begin(), m_BannedNicks.end());
	m_BannedNicks.erase( unique( m_BannedNicks.begin(), m_BannedNicks.end() ), m_BannedNicks.end() );
}

void CGameContext::RemoveFromNicknameBanList(int ID) {
	char aBuf[256];

	if (m_BannedNicks.empty())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BannedNicks", "No banned nicknames available.");
		return;
	}
	if (ID < 0 || static_cast<unsigned long>(ID) >= m_BannedNicks.size()) {
		str_format(aBuf, sizeof(aBuf), "Invalid ID: %d", ID);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BannedNicks", aBuf);
		return;
	}

	m_BannedNicks.erase(m_BannedNicks.begin() + ID);
	std::sort(m_BannedNicks.begin(), m_BannedNicks.end());

}

void CGameContext::RemoveFromNicknameBanList(const char* Nickname) {
	RemoveFromNicknameBanList(std::string(Nickname));
}

void CGameContext::RemoveFromNicknameBanList(std::string Nickname) {
	if (m_BannedNicks.empty())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BannedNicks", "No banned nicknames available.");
	}

	unsigned long size = m_BannedNicks.size();
	std::string tempNick;
	for (unsigned long i = 0; i < size; ++i)
	{

		tempNick = m_BannedNicks.back();
		m_BannedNicks.pop_back();
		if (tempNick.compare(Nickname))
		{
			break;
		} else {
			m_BannedNicks.insert(m_BannedNicks.begin(), tempNick);
		}
	}
	std::sort(m_BannedNicks.begin(), m_BannedNicks.end());
	if (m_BannedNicks.size() < size)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Removed '%s' from the nick ban list. ", tempNick.c_str());
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BannedNicks", aBuf);
	} else {
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Given nickname '%s' not found on in the nick ban list. ", tempNick.c_str());
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BannedNicks", aBuf);
	}

}

bool CGameContext::IsInNicknameBanList(const char* Nickname) {
	if (m_BannedNicks.empty()) {
		return false;
	}
	if (!Nickname) {
		return false;
	}
	long size = m_BannedNicks.size();

	for (int i = 0; i < size; ++i)
	{
		if (!m_BannedNicks[i].compare(Nickname)) {
			return true;
		}
	}
	return false;
}
bool CGameContext::IsInNicknameBanList(std::string Nickname) {
	return IsInNicknameBanList(Nickname.c_str());
}



/* wait given ms to lock ranking db, if time is negative wait until database is unlocked */
bool CGameContext::LockRankingDb(int ms)
{
	if (ms < 0)
	{
		m_RankingDbMutex.lock();
		return true;
	}
	return m_RankingDbMutex.try_lock_for(std::chrono::milliseconds(ms));
}

/* unlock ranking db */
void CGameContext::UnlockRankingDb()
{
	m_RankingDbMutex.unlock();
}

const char *CGameContext::GameType() { return m_pController && m_pController->m_pGameType ? m_pController->m_pGameType : ""; }
const char *CGameContext::Version() { return GAME_VERSION; }
const char *CGameContext::NetVersion() { return GAME_NETVERSION; }

IGameServer *CreateGameServer() { return new CGameContext; }

void CGameContext::BanIf(bool Condition, int ID, int TimeMinutes, std::string Reason) {
	if (0 <= ID && ID <= MAX_CLIENTS && m_apPlayers[ID]) {
		if (Condition) {
			GetBanServer()->BanAddr(GetBanServer()->Server()->m_NetServer.ClientAddr(ID) , TimeMinutes * 60 , Reason.c_str());
		}
	}

}


