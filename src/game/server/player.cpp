/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <algorithm> // std::random_shuffle
#include <vector> // std::vector
#include <functional>
#include <engine/shared/config.h>
#include "player.h"


MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, int Team)
{
    m_pGameServer = pGameServer;
    m_RespawnTick = Server()->Tick();
    m_DieTick = Server()->Tick();
    m_ScoreStartTick = Server()->Tick();
    m_pCharacter = 0;
    m_ClientID = ClientID;
    m_Team = GameServer()->m_pController->ClampTeam(Team);
    m_SpectatorID = SPEC_FREEVIEW;
    m_LastActionTick = Server()->Tick();
    m_TeamChangeTick = Server()->Tick();

    //zCatch
    m_CaughtBy = ZCATCH_NOT_CAUGHT;
    m_SpecExplicit = true;
    m_Kills = 0;
    m_Deaths = 0;
    m_LastKillTry = Server()->Tick();
    m_TicksSpec = 0;
    m_TicksIngame = 0;
    m_ChatTicks = 0;

    // zCatch/TeeVi
    m_ZCatchVictims = NULL;
    m_zCatchNumVictims = 0;
    m_zCatchNumKillsInARow = 0;
    m_zCatchNumKillsReleased = 0;
    ResetHardMode();

    // ranking system
    m_RankCache.m_Points = 0;
    m_RankCache.m_NumWins = 0;
    m_RankCache.m_NumKills = 0;
    m_RankCache.m_NumKillsWallshot = 0;
    m_RankCache.m_NumDeaths = 0;
    m_RankCache.m_NumShots = 0;
    m_RankCache.m_TimePlayed = 0;
    m_RankCache.m_TimeStartedPlaying = -1;
    RankCacheStartPlaying(); // start immediately

    // bot detection
    // old bot detection
    /*
    m_IsAimBot = 0;
    m_AimBotIndex = 0;
    m_AimBotRange = 0;
    m_AimBotLastDetection = 0;
    m_AimBotTargetSpeed = .0;
    m_CurrentTarget.x = 0;
    m_CurrentTarget.y = 0;
    m_LastTarget.x = 0;
    m_LastTarget.y = 0;
    */

    /*teehistorian player tracking*/
    m_TeeHistorianTracked = false;
    /*teehistorian player tracking*/

    // Snapshot stuff init
    m_CurrentTickPlayer.Init(ClientID, Server()->ClientJoinHash(ClientID), Server()->Tick());
    // This will not change

    // ban stuff
    ResetClientBanUrgencyLevel();

}

CPlayer::~CPlayer()
{

    while (m_ZCatchVictims != NULL)
    {
        CZCatchVictim *tmp = m_ZCatchVictims;
        m_ZCatchVictims = tmp->prev;
        delete tmp;
    }

    delete m_pCharacter;
    m_pCharacter = 0;

    /*teehistorian player tracking*/
    m_TeeHistorianTracked = false;
    /*teehistorian player tracking*/
    ClearIrregularFlags();
    // snapshot stuff
    ResetSnapshots();
    m_CurrentTickPlayer.ResetAllData();
}


double CPlayer::Angle(int meX, int meY, int otherX, int otherY) {

    static const double TWOPI = 6.2831853071795865;
    static const double RAD2DEG = 57.2957795130823209;
    if (meX == otherX && meY == otherY)
        return -1;

    double theta = atan2(otherX - meX, otherY - meY);
    if (theta < 0.0)
        theta += TWOPI;
    return RAD2DEG * theta;
}

double CPlayer::Distance(int meX, int meY, int otherX, int otherY) {
    return sqrt(pow(meX - otherX , 2) + pow(meY - otherY, 2));
}

void CPlayer::Tick()
{
#ifdef CONF_DEBUG
    if (!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS - g_Config.m_DbgDummies)
#endif
        if (!Server()->ClientIngame(m_ClientID))
            return;

    Server()->SetClientScore(m_ClientID, m_Score);

    /* begin zCatch*/

    if (m_Team == TEAM_SPECTATORS)
        m_TicksSpec++;
    else
        m_TicksIngame++;

    if (m_ChatTicks > 0)
        m_ChatTicks--;

    if ((g_Config.m_SvAnticamper == 2 && g_Config.m_SvMode == 1) || (g_Config.m_SvAnticamper == 1))
        Anticamper();
    /* end zCatch*/

    // do latency stuff
    {
        IServer::CClientInfo Info;
        if (Server()->GetClientInfo(m_ClientID, &Info))
        {
            m_Latency.m_Accum += Info.m_Latency;
            m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
            m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
        }
        // each second
        if (Server()->Tick() % Server()->TickSpeed() == 0)
        {
            m_Latency.m_Avg = m_Latency.m_Accum / Server()->TickSpeed();
            m_Latency.m_Max = m_Latency.m_AccumMax;
            m_Latency.m_Min = m_Latency.m_AccumMin;
            m_Latency.m_Accum = 0;
            m_Latency.m_AccumMin = 1000;
            m_Latency.m_AccumMax = 0;
        }
    }

    // about ten times per second changes the tee color.
    if ( ( m_IsRainbowBodyTee || m_IsRainbowFeetTee ) && Server()->Tick() >= (m_LastRainbowTick + 3.5)) {
        if (m_IsRainbowBodyTee)
        {
            DoRainbowBodyStep();
        }
        if (m_IsRainbowFeetTee)
        {
            DoRainbowFeetStep();
        }

    }

    if (!GameServer()->m_World.m_Paused)
    {
        if (!m_pCharacter && m_Team == TEAM_SPECTATORS && m_SpectatorID == SPEC_FREEVIEW)
            m_ViewPos -= vec2(clamp(m_ViewPos.x - m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y - m_LatestActivity.m_TargetY, -400.0f, 400.0f));

        if (!m_pCharacter && m_DieTick + Server()->TickSpeed() * 3 <= Server()->Tick())
            m_Spawning = true;

        if (m_pCharacter)
        {
            if (m_pCharacter->IsAlive())
            {
                m_ViewPos = m_pCharacter->m_Pos;
            }
            else
            {
                delete m_pCharacter;
                m_pCharacter = 0;
            }
        }
        else if (m_Spawning && m_RespawnTick <= Server()->Tick())
            TryRespawn();
    }
    else
    {
        ++m_RespawnTick;
        ++m_DieTick;
        ++m_ScoreStartTick;
        ++m_LastActionTick;
        ++m_TeamChangeTick;
    }

    // bot detection
    // old bot detection
    /*
    m_LastTarget = m_CurrentTarget;
    m_CurrentTarget.x = m_LatestActivity.m_TargetX;
    m_CurrentTarget.y = m_LatestActivity.m_TargetY;
    m_AimBotTargetSpeed = abs(distance(m_CurrentTarget, m_LastTarget));
    */
    // filling needs to be done before Doing the snapshot.
    FillCurrentTickPlayer();
    // mostly everything below depends on a full/half full currentTickplayer
    DoSnapshot();
    UpdateLongTermDataOnTick();
    CalculateBasedOnLastThreeMousePositionsOnTick();
    CheckIrregularFlags();

    // zCatch/TeeVi: hard mode
    if (m_HardMode.m_Active)
    {
        auto tl = &m_HardMode.m_ModeKillTimelimit;
        if (tl->m_Active && m_ZCatchVictims != NULL)
        {
            int nextKillTick = tl->m_LastKillTick + tl->m_TimeSeconds * Server()->TickSpeed();
            int ticksLeft = nextKillTick - Server()->Tick();
            if (ticksLeft == 0)
            {
                ReleaseZCatchVictim(ZCATCH_RELEASE_ALL, 1, false);
                tl->m_LastKillTick = Server()->Tick();
                GameServer()->SendBroadcast("One of your victims has been released.", GetCID());
            }
            else if ((ticksLeft % Server()->TickSpeed()) == 0) // show every second
            {
                char aBuf[256];
                str_format(aBuf, sizeof(aBuf), "You have %d seconds to catch the next one.", ticksLeft / Server()->TickSpeed());
                GameServer()->SendBroadcast(aBuf, GetCID());
            }
        }
    }
}

void CPlayer::PostTick()
{

    // update latency value
    if (m_PlayerFlags & PLAYERFLAG_SCOREBOARD)
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
                m_aActLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
        }
    }

    // update view pos for spectators
    if (m_Team == TEAM_SPECTATORS && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID])
        m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->m_ViewPos;
}

void CPlayer::Snap(int SnappingClient)
{
#ifdef CONF_DEBUG
    if (!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS - g_Config.m_DbgDummies)
#endif
        if (!Server()->ClientIngame(m_ClientID))
            return;

    CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, m_ClientID, sizeof(CNetObj_ClientInfo)));
    if (!pClientInfo)
        return;

    StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientID));
    StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
    pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
    StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_SkinName);
    pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
    pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
    pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;

    CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, m_ClientID, sizeof(CNetObj_PlayerInfo)));
    if (!pPlayerInfo)
        return;

    pPlayerInfo->m_Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
    pPlayerInfo->m_Local = 0;
    pPlayerInfo->m_ClientID = m_ClientID;
    pPlayerInfo->m_Score = m_Score;
    pPlayerInfo->m_Team = m_Team;

    if (m_ClientID == SnappingClient)
        pPlayerInfo->m_Local = 1;

    if (m_ClientID == SnappingClient && m_Team == TEAM_SPECTATORS)
    {
        CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
        if (!pSpectatorInfo)
            return;

        pSpectatorInfo->m_SpectatorID = m_SpectatorID;
        pSpectatorInfo->m_X = m_ViewPos.x;
        pSpectatorInfo->m_Y = m_ViewPos.y;
    }
}

void CPlayer::OnDisconnect(const char *pReason)
{

    // save ranking stats
    GameServer()->m_pController->SaveRanking(this);

    if (Server()->ClientIngame(m_ClientID))
    {
        char aBuf[512];
        if (pReason && *pReason)
            str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", Server()->ClientName(m_ClientID), pReason);
        else
            str_format(aBuf, sizeof(aBuf), "'%s' has left the game", Server()->ClientName(m_ClientID));

        GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

        str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", m_ClientID, Server()->ClientName(m_ClientID));
        GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);

        // irregular flags
        if (HasIrregularFlags())
        {
            // header
            str_format(aBuf, sizeof(aBuf), "Irregular flags of player '%s' Version: %s", Server()->ClientName(m_ClientID), ConvertToString(GetClientVersion()).c_str());
            GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "on_leave_player_flag", aBuf);

            std::vector<int> flags = GetIrregularFlags();
            for (size_t i = 0; i < flags.size(); ++i)
            {
                str_format(aBuf, sizeof(aBuf), "Irregular flag: %d", flags.at(i));
                GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "on_leave_player_flag", aBuf);
            }
        }
    }

    KillCharacter();

}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
    // skip the input if chat is active
    if ((m_PlayerFlags & PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags & PLAYERFLAG_CHATTING))
        return;

    if (m_pCharacter && m_pCharacter->m_FreezeTicks)
        return;

    if (m_pCharacter)
        m_pCharacter->OnPredictedInput(NewInput);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
    if (NewInput->m_PlayerFlags & PLAYERFLAG_CHATTING)
    {
        // skip the input if chat is active
        if (m_PlayerFlags & PLAYERFLAG_CHATTING)
            return;

        // reset input
        if (m_pCharacter)
            m_pCharacter->ResetInput();

        m_PlayerFlags = NewInput->m_PlayerFlags;
        return;
    }

    m_PlayerFlags = NewInput->m_PlayerFlags;
    if (m_pCharacter)
        m_pCharacter->OnDirectInput(NewInput);

    if (m_pCharacter && m_pCharacter->m_FreezeTicks)
        return;

    if (!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire & 1))
        m_Spawning = true;

    // check for activity
    if (NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
            m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
            NewInput->m_Fire & 1 || NewInput->m_Hook)
    {
        m_LatestActivity.m_TargetX = NewInput->m_TargetX;
        m_LatestActivity.m_TargetY = NewInput->m_TargetY;
        m_LastActionTick = Server()->Tick();
    }
}

CCharacter *CPlayer::GetCharacter()
{
    if (m_pCharacter && m_pCharacter->IsAlive())
        return m_pCharacter;
    return 0;
}

void CPlayer::KillCharacter(int Weapon)
{
    if (m_pCharacter)
    {
        m_pCharacter->Die(m_ClientID, Weapon);
        delete m_pCharacter;
        m_pCharacter = 0;
    }
}

void CPlayer::Respawn()
{
    if (m_Team != TEAM_SPECTATORS)
        m_Spawning = true;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{
    // clamp the team
    Team = GameServer()->m_pController->ClampTeam(Team);
    if (m_Team == Team)
        return;

    char aBuf[512];
    if (DoChatMsg)
    {
        str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(m_ClientID), GameServer()->m_pController->GetTeamName(Team));
        GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
    }

    KillCharacter();

    m_Team = Team;
    m_LastActionTick = Server()->Tick();
    m_SpectatorID = SPEC_FREEVIEW;
    // we got to wait 0.5 secs before respawning
    m_RespawnTick = Server()->Tick() + Server()->TickSpeed() / 2;
    str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", m_ClientID, Server()->ClientName(m_ClientID), m_Team);
    GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

    if (Team == TEAM_SPECTATORS)
    {
        // update spectator modes
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
                GameServer()->m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
        }
        m_SpecExplicit = true;
        RankCacheStopPlaying();
    }
    else
    {
        m_SpecExplicit = false;
        RankCacheStartPlaying();
    }
}

void CPlayer::SetTeamDirect(int Team)
{
    m_Team = Team;
}

void CPlayer::TryRespawn()
{
    vec2 SpawnPos;

    if (!GameServer()->m_pController->CanSpawn(m_Team, &SpawnPos))
        return;

    m_Spawning = false;
    m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
    m_pCharacter->Spawn(this, SpawnPos);

    // zCatch/TeeVi hard mode: weapon overheat reset
    m_HardMode.m_ModeWeaponOverheats.m_Heat = 0;

    GameServer()->CreatePlayerSpawn(SpawnPos);
}

int CPlayer::Anticamper()
{
    if (GameServer()->m_World.m_Paused || !m_pCharacter || m_Team == TEAM_SPECTATORS || m_pCharacter->m_FreezeTicks)
    {
        m_CampTick = -1;
        m_SentCampMsg = false;
        return 0;
    }

    int AnticamperTime = g_Config.m_SvAnticamperTime;
    int AnticamperRange = g_Config.m_SvAnticamperRange;

    if (m_CampTick == -1)
    {
        m_CampPos = m_pCharacter->m_Pos;
        m_CampTick = Server()->Tick() + Server()->TickSpeed() * AnticamperTime;
    }

    // Check if the player is moving
    if ((m_CampPos.x - m_pCharacter->m_Pos.x >= (float)AnticamperRange || m_CampPos.x - m_pCharacter->m_Pos.x <= -(float)AnticamperRange)
            || (m_CampPos.y - m_pCharacter->m_Pos.y >= (float)AnticamperRange || m_CampPos.y - m_pCharacter->m_Pos.y <= -(float)AnticamperRange))
    {
        m_CampTick = -1;
    }

    // Send warning to the player
    if (m_CampTick <= Server()->Tick() + Server()->TickSpeed() * AnticamperTime / 2 && m_CampTick != -1 && !m_SentCampMsg)
    {
        GameServer()->SendBroadcast("ANTICAMPER: Move or die", m_ClientID);
        m_SentCampMsg = true;
    }

    // Kill him
    if ((m_CampTick <= Server()->Tick()) && (m_CampTick > 0))
    {
        if (g_Config.m_SvAnticamperFreeze)
        {
            m_pCharacter->Freeze(Server()->TickSpeed()*g_Config.m_SvAnticamperFreeze);
            GameServer()->SendBroadcast("You have been freezed due camping", m_ClientID);
        }
        else
            m_pCharacter->Die(m_ClientID, WEAPON_GAME);
        m_CampTick = -1;
        m_SentCampMsg = false;
        return 1;
    }
    return 0;
}

// catch another player
void CPlayer::AddZCatchVictim(int ClientID, int reason)
{
    CPlayer *victim = GameServer()->m_apPlayers[ClientID];
    if (victim)
    {
        // add to list of victims
        CZCatchVictim *v = new CZCatchVictim;
        v->ClientID = ClientID;
        v->Reason = reason;
        v->prev = m_ZCatchVictims;
        m_ZCatchVictims = v;
        ++m_zCatchNumVictims;
        // set victim's status
        victim->m_CaughtBy = m_ClientID;
        victim->m_SpecExplicit = false;
        victim->m_zCatchJoinSpecWhenReleased = false;
        victim->SetTeamDirect(TEAM_SPECTATORS);
        victim->m_SpectatorID = m_ClientID;
    }
}

// release one or more of the victims
void CPlayer::ReleaseZCatchVictim(int ClientID, int limit, bool manual)
{
    CZCatchVictim **v = &m_ZCatchVictims;
    CZCatchVictim *tmp;
    CPlayer *victim;
    int count = 0;
    while (*v != NULL)
    {
        if (ClientID == ZCATCH_RELEASE_ALL || (*v)->ClientID == ClientID)
        {
            victim = GameServer()->m_apPlayers[(*v)->ClientID];
            if (victim)
            {
                victim->m_CaughtBy = ZCATCH_NOT_CAUGHT;
                victim->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));
                victim->m_SpectatorID = SPEC_FREEVIEW;
                // SetTeam after SetTeamDirect, otherwise it would skip the message for joining the spectators
                if (victim->m_zCatchJoinSpecWhenReleased)
                    victim->SetTeam(GameServer()->m_pController->ClampTeam(TEAM_SPECTATORS));
            }

            // count releases of players you killed
            if (manual && (*v)->Reason == ZCATCH_CAUGHT_REASON_KILLED)
            {
                ++m_zCatchNumKillsReleased;
            }

            // delete from list
            tmp = (*v)->prev;
            delete *v;
            *v = tmp;
            --m_zCatchNumVictims;

            if (limit && ++count >= limit)
                return;
        }
        else
            v = &(*v)->prev;
    }
}

// start counter for playing time
void CPlayer::RankCacheStartPlaying() {
    if (m_RankCache.m_TimeStartedPlaying == -1) {
        m_RankCache.m_TimeStartedPlaying = Server()->Tick();
    }
}

// stop counter for playing time
void CPlayer::RankCacheStopPlaying() {
    if (m_RankCache.m_TimeStartedPlaying > -1) {
        m_RankCache.m_TimePlayed += Server()->Tick() - m_RankCache.m_TimeStartedPlaying;
        m_RankCache.m_TimeStartedPlaying = -1;
    }
}

// add hard mode setting
bool CPlayer::AddHardMode(const char* mode)
{
    bool isLaser = g_Config.m_SvMode == 1;
    bool isGrenade = g_Config.m_SvMode == 4;
    bool isLaserOrGrenade = isLaser || isGrenade;

    if (!str_comp_nocase("ammo210", mode) && isGrenade)
    {
        m_HardMode.m_ModeAmmoLimit = 2;
        m_HardMode.m_ModeAmmoRegenFactor = 10;
    }
    else if (!str_comp_nocase("ammo15", mode) && isGrenade)
    {
        m_HardMode.m_ModeAmmoLimit = 1;
        m_HardMode.m_ModeAmmoRegenFactor = 5;
    }
    else if (!str_comp_nocase("overheat", mode) && isLaserOrGrenade)
        m_HardMode.m_ModeWeaponOverheats.m_Active = true;
    else if (!str_comp_nocase("hookkill", mode) && isLaserOrGrenade)
        m_HardMode.m_ModeHookWhileKilling = true;
    else if (!str_comp_nocase("fail0", mode) && isGrenade)
    {
        m_HardMode.m_ModeTotalFails.m_Active = true;
        m_HardMode.m_ModeTotalFails.m_Max = 0;
    }
    else if (!str_comp_nocase("fail3", mode) && isGrenade)
    {
        m_HardMode.m_ModeTotalFails.m_Active = true;
        m_HardMode.m_ModeTotalFails.m_Max = 3;
    }
    else if (!str_comp_nocase("5s", mode) && isLaserOrGrenade)
    {
        m_HardMode.m_ModeKillTimelimit.m_Active = true;
        m_HardMode.m_ModeKillTimelimit.m_TimeSeconds = 5;
    }
    else if (!str_comp_nocase("10s", mode) && isLaserOrGrenade)
    {
        m_HardMode.m_ModeKillTimelimit.m_Active = true;
        m_HardMode.m_ModeKillTimelimit.m_TimeSeconds = 10;
    }
    else if (!str_comp_nocase("20s", mode) && isLaserOrGrenade)
    {
        m_HardMode.m_ModeKillTimelimit.m_Active = true;
        m_HardMode.m_ModeKillTimelimit.m_TimeSeconds = 20;
    }
    else if (!str_comp_nocase("double", mode) && isLaserOrGrenade)
    {
        m_HardMode.m_ModeDoubleKill.m_Active = true;
        m_HardMode.m_ModeDoubleKill.m_Character = NULL;
    }
    else
        return false;

    m_HardMode.m_Active = true;
    return true;
}

// add a random hard mode
const char* CPlayer::AddRandomHardMode()
{
    auto modes = m_pGameServer->GetHardModes();
    std::random_shuffle(modes.begin(), modes.end());

    for (auto it = modes.begin(); it != modes.end(); ++it)
    {
        if ((g_Config.m_SvMode == 1 && it->laser) || (g_Config.m_SvMode == 4 && it->grenade))
        {
            AddHardMode(it->name);
            return it->name;
        }
    }
    return "";
}

void CPlayer::DoRainbowBodyStep() {
    if (m_IsRainbowBodyTee) {
        int h = ((m_TeeInfos.m_ColorBody & 0xFF00FF)+1) % 0xFF;
        int s = 0xFF;
        int l = 0x00;
        int color = (h << 16) | (s << 8) | l;
        m_TeeInfos.m_ColorBody = color;
    }
}

void CPlayer::DoRainbowFeetStep() {
    if (m_IsRainbowFeetTee) {

        int f = ((m_TeeInfos.m_ColorFeet & 0xFF00FF) +1) % 0xFF;
        int s = 0xFF;
        int l = 0x00;
        int colorFeet = (f << 16) | (s << 8) | l;
        m_TeeInfos.m_ColorFeet = colorFeet;

    }
}

// reset hard mode
void CPlayer::ResetHardMode()
{
    mem_zero(&m_HardMode, sizeof(m_HardMode));
}

// reset hard mode counters
void CPlayer::HardModeRestart()
{
    m_HardMode.m_ModeTotalFails.m_Fails = 0;
    m_HardMode.m_ModeDoubleKill.m_Character = NULL;
}

// when the players fails a shot (no kill and no speed nade)
void CPlayer::HardModeFailedShot()
{
    if (m_HardMode.m_Active && m_HardMode.m_ModeTotalFails.m_Active)
    {
        m_HardMode.m_ModeTotalFails.m_Fails++;
        char Buf[128];
        if (m_HardMode.m_ModeTotalFails.m_Fails > m_HardMode.m_ModeTotalFails.m_Max)
        {
            KillCharacter();
            str_copy(Buf, "You failed too often.", sizeof(Buf));
        }
        else
        {
            str_format(Buf, sizeof(Buf), "Fails: %d/%d", m_HardMode.m_ModeTotalFails.m_Fails, m_HardMode.m_ModeTotalFails.m_Max);
        }
        GameServer()->SendBroadcast(Buf, GetCID());
    }
}

std::bitset<32> CPlayer::ConvertToBitMask(int flags) {
    return std::bitset<32>(flags);
}

std::string CPlayer::ConvertToString(int value) {
    std::stringstream s;
    s << value;
    return s.str();
}

std::string CPlayer::ConvertToString(const std::vector<double> &vector) {
    std::stringstream s;
    s << "[ ";
    for (size_t i = 0; i < vector.size(); ++i)
    {
        if (i == vector.size() - 1) {
            s << vector.at(i);
        } else {
            s << vector.at(i) << ", ";
        }
    }
    s << " ]";
    return s.str();
}

void CPlayer::CalculateBasedOnLastThreeMousePositionsOnTick(){
    // update to current tick
    UpdateLastThreeMousePositionsOnTick();

    if(m_LastThreeMousePositions.size() == 3){

        // mouse speed stuff between three inputs
        double distance = CalculateDistance(m_LastThreeMousePositions);
        double ticks = CalculateTicksPassed(m_LastThreeMousePositions);
        double speed = distance / ticks;
        double distanceBetweenFirstAndThridPosition = Distance(m_LastThreeMousePositions.at(0).x,
                                                        m_LastThreeMousePositions.at(0).y,
                                                        m_LastThreeMousePositions.at(2).x,
                                                        m_LastThreeMousePositions.at(2).y);
        // update fastest speed, meaning highest value
        if (speed > m_LongestDistancePerTickOfThreeConsequtiveMousePositions) {
            m_LongestDistancePerTickOfThreeConsequtiveMousePositions = speed;
            // empty vector
            m_ThreeConsequtiveMousePositionsWithLongestDistance.clear();
            // fill vector with new values
            for (size_t i = 0; i < m_LastThreeMousePositions.size(); ++i)
            {
                m_ThreeConsequtiveMousePositionsWithLongestDistance.push_back(m_LastThreeMousePositions.at(i));
            }
        }
        // mouse speed calculation end

        // maybe fast aiming bots
        if (distanceBetweenFirstAndThridPosition < g_Config.m_SvFastAimFirstAndThirdPositionDistanceTolerance && distance > g_Config.m_SvFastAimDistanceTravelledTolerance) // TODO: gotta adjust this stuff here.
        {
            m_NearlyIdenticalFirstAndLastPositionCounter++;

            // if current distance travelled is bigger and first and third posision are closer together, update the vector.
            if (m_ThreeConsequtiveMousePositionsWithNearlyIdenticalFirstAndLastPosition.size() < 3 || // needed otherwise this check makes no sens at the start, when that vector is empty.
            (distance > m_m_DistanceTravelledBetweenNearlyIdenticalFirstAndLastPosition
            && distanceBetweenFirstAndThridPosition < m_DistanceOfNearlyIdenticalFirstAndLastPosition))
            {
                // update the vector containing the positions
                m_ThreeConsequtiveMousePositionsWithNearlyIdenticalFirstAndLastPosition.clear();
                for (size_t i = 0; i < m_LastThreeMousePositions.size(); ++i)
                {
                    m_ThreeConsequtiveMousePositionsWithNearlyIdenticalFirstAndLastPosition.push_back(m_LastThreeMousePositions.at(i));
                }
                //update speed
                m_DistancePerTickOfNearlyIdenticalFirstAndLastPosition = speed;
                m_m_DistanceTravelledBetweenNearlyIdenticalFirstAndLastPosition = distance;
                m_DistanceOfNearlyIdenticalFirstAndLastPosition = distanceBetweenFirstAndThridPosition;

            }

        }
        // maybe fast aiming bots end
    }

}

void CPlayer::FillCurrentTickPlayer() {
    if (GetCharacter())
    {
        // if core available & input available
        if (GetCharacter()->Core() && GetCharacter()->Input()) {

            m_CurrentTickPlayer.SetTick(Server()->Tick());
            // fill core
            CNetObj_CharacterCore Char;
            GetCharacter()->GetCore().Write(&Char);
            m_CurrentTickPlayer.FillCore(&Char);

            // fill input
            m_CurrentTickPlayer.FillInput(GetCharacter()->Input());

        } else {
            // core & input not available
            return;
        }
    }
}

void CPlayer::DoSnapshot() {
    if (m_OldIsSnapshotActive != m_IsSnapshotActive) {
        if (m_IsSnapshotActive) // snapshot was enabled
        {
            // enabled, so set old to new active.
            m_OldIsSnapshotActive = m_IsSnapshotActive;
            if (GetCurrentSnapshotSize() >= GetSnapshotWantedLength()) {
                ResetCurrentSnapshot();
            }
        } else { // snapshot was disabled
            m_OldIsSnapshotActive = m_IsSnapshotActive;
        }
    }

    // possible problem: nearly impossible for tick & snap (mouse input) data to be available at the same time.
    // as long as snapshot is active & haven't reached the needed amount of snapshots
    if (m_IsSnapshotActive && GetCurrentSnapshotSize() < GetSnapshotWantedLength()) {
        // if core & input available, push data and then reset the buffer.
        if (m_CurrentTickPlayer.IsFull()) {
            // adds tick player and resets current tick player's tick data
            AddAndResetCurrentTickPlayerToCurrentSnapshot();
        }
    } else if (m_IsSnapshotActive && GetCurrentSnapshotSize() >= GetSnapshotWantedLength()) {
        // if the snapshot has the wanted lenth, everything and disable snapshoting.
        char aBuf[48];
        str_format(aBuf, sizeof(aBuf), "Snapshot done(%ld): ID=%d Name=%s", GetSnapshotWantedLength(), m_ClientID, Server()->ClientName(m_ClientID));
        GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
        m_IsSnapshotActive = false;
        m_CurrentTickPlayer.ResetTickData();
        IncSnapshotCount();
    }
}

void CPlayer::UpdateLongTermDataOnTick() {

    // cursor position from player.
    double CurrentCursorDistanceFromTee = GetCurrentDistanceFromTee();
    if (IsZoomIndicatorCursorDistance(CurrentCursorDistanceFromTee)) {
        m_ZoomDistances.push_back(CurrentCursorDistanceFromTee);
    }
    if (CurrentCursorDistanceFromTee > m_BiggestCursorDistanceFromTee)
    {
        // update member.
        m_BiggestCursorDistanceFromTee = CurrentCursorDistanceFromTee;
    }
}

/**
 * @brief Checks client version and irregular flags and updates client description.
 * @details This method is used to check client version and client flags at once.
 *  This method also handles general updates of BAN_URGENCY_LEVELS, in order to ban specific clients
 *  without any further checks.
 */
void CPlayer::GeneralClientCheck(){

    if (HasIrregularFlags() || HasIrregularClientVersion())
    {

        switch(GetClientVersion()){

            // Grenade Bot by Prem
            case 1331:
            {
                UpdateCurrentClientDescription("Grenade Bot.");
                UpdateClientBanUrgencyLevel(URGENCY_LEVEL_INSTANT_BAN);
                break;
            }

            // Baumalein
            case 708:
            {
                UpdateCurrentClientDescription("Baumalein.");
                UpdateClientBanUrgencyLevel(URGENCY_LEVEL_INSTANT_BAN);
                break;
            }

            // zClient
            case 602:
            {
                UpdateCurrentClientDescription("zClient.");
                UpdateClientBanUrgencyLevel(URGENCY_LEVEL_INSTANT_BAN);
                break;
            }

            //Sn0wFlake & FClient
            case 502:
            {
                // Sn0wFlake client
                std::vector<int> Flags = GetIrregularFlags();
                bool hasSnowflakeFlags = false;
                for (size_t i = 0; i < Flags.size(); ++i)
                {
                    // flags 5 and 6 have to be set to be the snowflake client
                    std::bitset<32> currentBitmask = ConvertToBitMask(Flags.at(i));
                    hasSnowflakeFlags = hasSnowflakeFlags || (currentBitmask.test(5) && currentBitmask.test(6));
                }
                if (hasSnowflakeFlags) {
                    UpdateCurrentClientDescription("Sn0wFlake.");
                    UpdateClientBanUrgencyLevel(URGENCY_LEVEL_INSTANT_BAN);
                } else {
                    UpdateCurrentClientDescription("FClient.");
                    UpdateClientBanUrgencyLevel(URGENCY_LEVEL_INSTANT_BAN);
                }
                break;
            }

            // K-Client
            case 0:
            {
                std::vector<int> Flags = GetIrregularFlags();
                bool IsKClient = false;
                for (size_t i = 0; i < Flags.size(); ++i)
                {
                    if (ConvertToBitMask(Flags.at(i)).test(9)) {
                        IsKClient = IsKClient || true;
                    }
                }
                if (IsKClient)
                {
                    UpdateCurrentClientDescription("K-Client.");
                    UpdateClientBanUrgencyLevel(URGENCY_LEVEL_CHEAT_CLIENT);
                }
                break;
            }
            default: break;
        }

        // bigger ranges of version numbers.
        if (GetClientBanUrgency() == URGENCY_LEVEL_NORMAL_PLAYER)
            {
                int version = GetClientVersion();

                if((version >= 15 && version < 100) ||
                    version == 405 ||
                    version == 605 ||
                    version == 1){
                    UpdateCurrentClientDescription("Cheat Client.");
                    UpdateClientBanUrgencyLevel(URGENCY_LEVEL_CHEAT_CLIENT);

                // ATH
                } else if( 10071 <= version && version <= 10072)
                {
                    UpdateCurrentClientDescription("ATH.");
                    UpdateClientBanUrgencyLevel(URGENCY_LEVEL_CHEAT_CLIENT);
                }

            }
    }
}

bool CPlayer::IsBot() {
    // Check if client already detected as bot: then you don't have to do anything anymore:
    if (GetClientBanUrgency() == URGENCY_LEVEL_BOT || GetClientBanUrgency() == URGENCY_LEVEL_ZOOM_AND_BOT)
    {
        return true;
    }

    //K-Client or K-Client directly joining with bot enabled.
    if (IsInClientDescription("K-Client") || GetClientVersion() == 0)
    {
        std::vector<int> Flags = GetIrregularFlags();
        bool hasKClientBotInputFlag = false;

        for (size_t i = 0; i < Flags.size(); ++i)
        {
            // flag 8 is set according to the developer of the bot client.
            hasKClientBotInputFlag = hasKClientBotInputFlag || ConvertToBitMask(Flags.at(i)).test(8);
        }
        if (hasKClientBotInputFlag) {
            UpdateClientBanUrgencyLevel(URGENCY_LEVEL_BOT);
            UpdateCurrentClientDescription("Currently botting.");
            return true;
        }
    }

    return false;
}



bool CPlayer::IsZoomIndicatorCursorDistance(double distance) {
    // if player cannot be detected yet, we do not check anything
    // this here is used to prevent weird client stuff from happening on player join.
    if (!CanDetectionBeStarted())
    {
        return false;
    }
    int version = GetClientVersion();
    // short circuit operator && to improve performance for non-vanilla client players.
    if (version == 0 && IsInClientDescription("K-Client") && distance >= 1000.1) { // 1000 is the default dyn cam distance of K-Client
        // K-Client with zoom stuff.
        return true;
    } else if (distance >= 800.1 && version == 0) {
        // Either normal vanilla or also botless K-Client
        // Vanilla client with most likely zoom stuff
        return true;
    }
    // ddnet clients normally do not send a high mouse position, except in weird situations.
    return false;
}



bool CPlayer::IsZoom() {
    // if player cannot be detected yet, we do not check anything
    if (!CanDetectionBeStarted())
    {
        return false;
    }

    if (m_ZoomDistances.size() > 25)
    {
        UpdateClientBanUrgencyLevel(URGENCY_LEVEL_ZOOM);
        UpdateCurrentClientDescription("Using Zoom.");
        return true;
    }

    int distancesOver1000 = 0;

    for (size_t i = 0; i < m_ZoomDistances.size(); ++i)
    {
        double currentDistance = m_ZoomDistances.at(i);

        if (currentDistance > 1000.0) {
            distancesOver1000++;
        }
    }

    // ddnet client seems to have problems, where it also shows (5 ticks) the mouse position
    if (distancesOver1000 > 10)
    {
        UpdateClientBanUrgencyLevel(URGENCY_LEVEL_ZOOM);
        UpdateCurrentClientDescription("Using Zoom.");
        return true;
    }

    // resets the zoom indocation vector after given seconds, default is 2 minutes = 120 seconds.
    // look this up in the variables.h
    if(Server()->TickSpeed() % (Server()->TickSpeed() * g_Config.m_SvZoomIndicationResetInterval) == 0){

        // reset only reset if none of the criteria above are met!
        if (distancesOver1000 <= 3 && m_ZoomDistances.size() <= 13)
        {
            m_ZoomDistances.clear();
        }
    }

    return false;
}
