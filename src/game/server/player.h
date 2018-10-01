/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

// this include should perhaps be removed
#include "entities/character.h"
#include "gamecontext.h"
#include "botdetectionstructs.h"
#include <vector> // std::vector
#include <set>
#include <bitset>
#include <deque>



// player object
class CPlayer
{
    MACRO_ALLOC_POOL_ID()

public:
    CPlayer(CGameContext *pGameServer, int ClientID, int Team);
    ~CPlayer();

    void Init(int CID);

    void TryRespawn();
    void Respawn();
    void SetTeam(int Team, bool DoChatMsg = true);
    void SetTeamDirect(int Team); //zCatch
    int GetTeam() const { return m_Team; };
    int GetCID() const { return m_ClientID; };

    void Tick();
    void PostTick();
    void Snap(int SnappingClient);

    void OnDirectInput(CNetObj_PlayerInput *NewInput);
    void OnPredictedInput(CNetObj_PlayerInput *NewInput);
    void OnDisconnect(const char *pReason);

    void KillCharacter(int Weapon = WEAPON_GAME);
    CCharacter *GetCharacter();

    //---------------------------------------------------------
    // this is used for snapping so we know how we can clip the view for the player
    vec2 m_ViewPos;

    // states if the client is chatting, accessing a menu etc.
    int m_PlayerFlags;

    /**
     * @brief Irregular flags that the client sends to the server.
     * These are partly used to detect weird bot/zoom and other cheat clients.
     * This set contains unique detected flags that were sent to the server.
     */
    std::set<int> m_PlayerIrregularFlags;

    /**
     * @brief In order to categorize player clients and make them visible to other players
     * this description is filled with information, which should be accessible to
     * other player in some way.
     */
    std::string m_CurrentClientDescription{""};
    std::string GetCurrentClientDescription(){return m_CurrentClientDescription;}
    void UpdateCurrentClientDescription(std::string Description){
        if(m_CurrentClientDescription.find(Description) == std::string::npos){
            if(m_CurrentClientDescription.size() == 0){
                m_CurrentClientDescription = Description;
            } else {
                m_CurrentClientDescription.append(Description);
            }
        }
    }
    void ResetCurrentClientDescription(){m_CurrentClientDescription = "";}
    /**
     * @brief Checks if the player has some information regarding irregular client usage.
     * If the player has such information, it cloud/should be printed to the screen.
     */
    bool HasClientDescription(){return m_CurrentClientDescription.size() > 0;}

    /**
     * @brief Looks into the client description and returns, whether the given
     *        substring is part of the description.
     */
    bool IsInClientDescription(std::string SubString){
        // for faster checking of not cheating players.
        if (!HasClientDescription())
        {
            return false;
        }
        // search fo substing in the client description
        std::size_t foundPos = m_CurrentClientDescription.find(SubString);
        // if found, return true, else false
        return foundPos != std::string::npos;
    }

    enum{
        URGENCY_LEVEL_NORMAL_PLAYER = 0,
        // do not confuse with cheat client, these, weird clients, are client not detected in anything basically
        // they re for example client like H-Client or other clients that send flags or a client version.
        URGENCY_LEVEL_WEIRD_CLIENT = 1,
        URGENCY_LEVEL_CHEAT_CLIENT = 2,
        URGENCY_LEVEL_ZOOM = 3,
        URGENCY_LEVEL_BOT = 4,
        URGENCY_LEVEL_ZOOM_AND_BOT = 5,
        URGENCY_LEVEL_INSTANT_BAN = 6
    };
    /**
     * @brief How urgent it is to ban the player
     * 0 - normal player, nothing to do.
     * 1 - weird client                                 // do nothing
     * 2 - known "half cheat client" ath for exmple.    // inform players periodically.
     * 3 - zoom                                         // inform players and ban.
     * 4 - bot                                          // inform player and ban
     * 5 - both                                         // inform player and ban
     * 6 - clients that should be banned instantly.     // instant ban.
     * 7 - ...
     */
    int m_ClientBanUrgencyLevel{0};
    void UpdateClientBanUrgencyLevel(int updateTo){
        if (updateTo < 0) return;
        if((m_ClientBanUrgencyLevel == URGENCY_LEVEL_ZOOM && updateTo == URGENCY_LEVEL_BOT)
            || (m_ClientBanUrgencyLevel == URGENCY_LEVEL_BOT && updateTo == URGENCY_LEVEL_ZOOM)){
            m_ClientBanUrgencyLevel = URGENCY_LEVEL_ZOOM_AND_BOT;
            return;
        }
        m_ClientBanUrgencyLevel = max(m_ClientBanUrgencyLevel, updateTo);
    }
    void ResetClientBanUrgencyLevel(){m_ClientBanUrgencyLevel = URGENCY_LEVEL_NORMAL_PLAYER;}
    int GetClientBanUrgency(){return  m_ClientBanUrgencyLevel;}

    int m_LastClientMentionedTick{-1};
    /**
     * @brief Resets the LastClientMentionedTick to -1 thus it is handled, as if the player's
     * client has never been mentioned before.
     */
    void ResetLastClientMentionedTick(){m_LastClientMentionedTick = -1;};
    /**
     * @brief This method is used to inform players periodically that a player may
     * be using a cheat client. It returns the tick, when the last mention was that
     * the player uses a weird client. If this method returns a value < 0, then
     * it has not been mentioned before that
     */
    int GetLastClientMentionedTick(){return m_LastClientMentionedTick;}

    /**
     * @brief This method is used to update the last mentioned tick of a player.
     * If the players ingame have been informed that this player uses a weird client,
     * this method should be called to update the mentioned tick to the current server tick.
     */
    void UpdateLastClientMentionedTick(){m_LastClientMentionedTick = Server()->Tick();}

    /**
     * @brief Flags 1,2,4,8 are default vanilla flags for chatting etc.
     *  and the flag 16 is for aimlines in the ddnet client (noby told me)
     *  meaning: 1 + 2 + 4 + 8 + 16 = 31 and anything above it is not regular
     *  flag 32 seems to be some H-client flag
     */
    static bool IsIrregularFlag(int Flags) { return Flags > 63;}

    /**
     * @brief Checks if given client version matches one
     * of the known irregular, most likely bot client versions.
     * To handle the updates of the client description, please take a look at
     * CPlayer::GeneralClientCheck()!
     * DO NOT handle banning or other stuff in this method.
     */
    static bool IsIrregularClientVersion(int version) {
        return  (version >= 15 && version < 100) ||
                version == 405 ||
                version == 502 || // FClient
                version == 602 || // zClient
                version == 605 ||
                version == 1 ||
                version == 708; // baumalein
    }
    void CheckIrregularFlags() {if (IsIrregularFlag(m_PlayerFlags)) m_PlayerIrregularFlags.insert(m_PlayerFlags);}
    void ClearIrregularFlags() { m_PlayerIrregularFlags.clear();}
    /**
     * @brief Returns a vector created from the set of irregular flags.
     */
    std::vector<int> GetIrregularFlags() {
        std::vector<int> v;
        std::set<int>::iterator setIt = m_PlayerIrregularFlags.begin();
        for (size_t i = 0; i < m_PlayerIrregularFlags.size(); ++i)
        {
            v.push_back(*setIt);
            setIt++;
        }
        return v;
    }

    /**
     * @brief Checks if the player has some tracked irregular flags in his flags set.
     */
    bool HasIrregularFlags() {return m_PlayerIrregularFlags.size() > 0;};

    /**
     * @brief Checks if the player has an irregular client version,
     * meaning that the list of version codes sent to the server need
     * to contain irregular version numbers.
     */
    bool HasIrregularClientVersion() {
        return IsIrregularClientVersion(m_ClientVersion);
    }

    int m_ClientVersion{0};
    void SetClientVersion(int version) {
        if (version > 0) {
            m_ClientVersion = version;
        }
    }
    int GetClientVersion() {
        return m_ClientVersion;
    }
    // ########### end of flags/ client version stuff ##########
    /**
     * @brief Returns a vector with at least 32 elements representing zeroes and ones.
     * Accessing this vector using the index, which accesses the specific flag at 2^(index)
     */
    static std::bitset<32> ConvertToBitMask(int Flags);

    static std::string ConvertToString(int value);

    static std::string ConvertToString(const std::vector<double> &vector);

    /**
     * @brief This method should be called first followed by IsZoom and then by IsBot.
     */
    void GeneralClientCheck();

    /**
     * @brief This function contains some basic tests, if the player uses
     *  a bot client using the client version and sent client flags.
     *  This method should be called after GeneralClientCheck.
     */
    bool IsBot();

    /**
     * @brief This here also uses some basic detection based on
     * player falgs and client version.
     * This method should be called after GeneralClientCheck.
     */
    bool IsZoom();

    // used for snapping to just update latency if the scoreboard is active
    int m_aActLatency[MAX_CLIENTS];

    // used for spectator mode
    int m_SpectatorID;

    bool m_IsReady;

    //
    int m_Vote;
    int m_VotePos;
    //
    int m_LastVoteCall;
    int m_LastVoteTry;
    int m_LastChat;
    int m_LastSetTeam;
    int m_LastSetSpectatorMode;
    int m_LastChangeInfo;
    int m_LastEmote;
    int m_LastKill;

    // TODO: clean this up
    struct
    {
        char m_SkinName[64];
        int m_UseCustomColor;
        int m_ColorBody;
        int m_ColorFeet;
    } m_TeeInfos;

    // rainbow stuff
    bool IsRainbowTee() { return m_IsRainbowBodyTee || m_IsRainbowFeetTee;}
    void ResetRainbowTee() { m_IsRainbowBodyTee = false; m_IsRainbowFeetTee = false;}
    void GiveBodyRainbow() { m_IsRainbowBodyTee = true;}
    void GiveFeetRainbow() { m_IsRainbowFeetTee = true;}
    bool m_IsRainbowBodyTee{false};
    bool m_IsRainbowFeetTee{false};
    // rainbow end

    int m_RespawnTick;
    int m_DieTick;
    int m_Score;
    int m_ScoreStartTick;
    bool m_ForceBalanced;
    int m_LastActionTick;
    int m_TeamChangeTick;
    struct
    {
        int m_TargetX;
        int m_TargetY;
    } m_LatestActivity;

    // network latency calculations
    struct
    {
        int m_Accum;
        int m_AccumMin;
        int m_AccumMax;
        int m_Avg;
        int m_Min;
        int m_Max;
    } m_Latency;

    //zCatch:
    int m_CaughtBy;
    bool m_SpecExplicit;
    int m_Deaths;
    int m_Kills;
    int m_LastKillTry;

    int m_TicksSpec;
    int m_TicksIngame;
    int m_ChatTicks;
    //Anticamper
    int Anticamper();
    bool m_SentCampMsg;
    int m_CampTick;
    vec2 m_CampPos;

    // zCatch/TeeVi
    enum
    {
        ZCATCH_NOT_CAUGHT = -1,
        ZCATCH_RELEASE_ALL = -1,
        ZCATCH_CAUGHT_REASON_JOINING = 0,
        ZCATCH_CAUGHT_REASON_KILLED,
    };
    struct CZCatchVictim
    {
        int ClientID;
        int Reason;
        CZCatchVictim *prev;
    };
    CZCatchVictim *m_ZCatchVictims;
    int m_zCatchNumVictims;
    int m_zCatchNumKillsInARow;
    int m_zCatchNumKillsReleased;
    bool m_zCatchJoinSpecWhenReleased;
    void AddZCatchVictim(int ClientID, int reason = ZCATCH_CAUGHT_REASON_JOINING);
    void ReleaseZCatchVictim(int ClientID, int limit = 0, bool manual = false);
    bool HasZCatchVictims() { return (m_ZCatchVictims != NULL); }
    int LastZCatchVictim() { return HasZCatchVictims() ? m_ZCatchVictims->ClientID : -1; }

    /* ranking system */
    struct {
        int m_Points;
        int m_NumWins;
        int m_NumKills;
        int m_NumKillsWallshot;
        int m_NumDeaths;
        int m_NumShots;
        int m_TimePlayed; // ticks
        int m_TimeStartedPlaying; // tick
    } m_RankCache;
    void RankCacheStartPlaying();
    void RankCacheStopPlaying();

    // zCatch/TeeVi hard mode
    struct {
        bool m_Active;
        char m_Description[256];
        unsigned int m_ModeAmmoLimit;
        unsigned int m_ModeAmmoRegenFactor;
        struct {
            bool m_Active;
            CCharacter *m_Character;
        } m_ModeDoubleKill;

        struct {
            bool m_Active;
            int m_TimeSeconds;
            int m_LastKillTick;
        } m_ModeKillTimelimit;
        bool m_ModeHookWhileKilling;

        struct {
            bool m_Active;
            unsigned int m_Heat;
        } m_ModeWeaponOverheats;

        struct {
            bool m_Active;
            unsigned int m_Max;
            unsigned int m_Fails;
        } m_ModeTotalFails;
        bool m_ModeInvisibleProjectiles; // TODO
        bool m_ModeInvisiblePlayers; // TODO
    } m_HardMode;

    void DoRainbowBodyStep();
    void DoRainbowFeetStep();
    bool AddHardMode(const char*);
    const char* AddRandomHardMode();
    void ResetHardMode();
    void HardModeRestart();
    void HardModeFailedShot();

    // bot detection
    // old bot detection
    /*
    int m_IsAimBot;
    int m_AimBotIndex;
    int m_AimBotRange;
    float m_AimBotTargetSpeed;
    vec2 m_CurrentTarget;
    vec2 m_LastTarget;
    int m_AimBotLastDetection;
    vec2 m_AimBotLastDetectionPos;
    vec2 m_AimBotLastDetectionPosVictim;
    */
    /*teehistorian player tracking*/
    bool m_TeeHistorianTracked;

    void SetTeeHistorianTracked(bool tracked) {m_TeeHistorianTracked = tracked;}
    bool GetTeeHistorianTracked() {return m_TeeHistorianTracked;}
    /*teehistorian player tracking*/

    //########## snapshot stuff ##########
#define SNAPSHOT_DEFAULT_LENGTH 10
    int GetCurrentSnapshotNumber() {return m_SnapshotCount % 2;}
    void AddAndResetCurrentTickPlayerToCurrentSnapshot() {
        if (m_CurrentTickPlayer.equalInput(m_OldTickPlayer)) {
            return;
        }
        m_OldTickPlayer = m_CurrentTickPlayer;
        GetCurrentSnapshotNumber() == 1 ?
        m_SnapshotOne.insert(m_CurrentTickPlayer) :
        m_SnapshotTwo.insert(m_CurrentTickPlayer);
        m_CurrentTickPlayer.ResetTickData();
    }

    void SetSnapshotWantedLength(int ticks) {ticks > 0 ? m_SnapshotWantedLength = ticks : m_SnapshotWantedLength = SNAPSHOT_DEFAULT_LENGTH;}
    size_t GetSnapshotWantedLength() {return m_SnapshotWantedLength;}
    int GetSnapsLeft() {return GetSnapshotWantedLength() - GetCurrentSnapshotSize();}
    void EnableSnapshot() {m_IsSnapshotActive = true;}
    bool IsSnapshotEnabled() {return m_IsSnapshotActive;}

    bool IsSnapshotFull() {
        return
            m_SnapshotOne.size() > 0 &&
            m_SnapshotTwo.size() > 0 &&
            m_SnapshotOne.size() == m_SnapshotTwo.size() &&
            m_SnapshotOne.size() == GetSnapshotWantedLength() &&
            GetCurrentSnapshotNumber() == 1;
    }

    std::vector<TickPlayer> GetFirstSnapshotResult() {
        std::vector<TickPlayer> v;
        for (TickPlayer t : m_SnapshotOne) {
            v.push_back(t);
        }
        std::sort(v.begin(), v.end());
        return v;
    }

    std::vector<TickPlayer> GetSecondSnapshotResult() {
        std::vector<TickPlayer> v;

        for (TickPlayer t : m_SnapshotTwo) {
            v.push_back(t);
        }
        std::sort(v.begin(), v.end());
        return v;
    };
    // ########## snapshot stuff end ##########

    // ########## helper functions #########
    double GetCurrentDistanceFromTee(){
        return Distance(m_CurrentTickPlayer.m_Core_X,
                m_CurrentTickPlayer.m_Core_Y, m_CurrentTickPlayer.m_Core_X + m_CurrentTickPlayer.m_Input_TargetX,
                m_CurrentTickPlayer.m_Core_Y + m_CurrentTickPlayer.m_Input_TargetY);
    }
    // ########## long term data ##########
    double GetBiggestCursorDistanceFromTee() {return m_BiggestCursorDistanceFromTee;}
    int GetZoomIndicatorCounter(){return m_ZoomDistances.size();}
    std::string GetZoomIndicationDistances(){return ConvertToString(m_ZoomDistances);}

    // these two are used for clipping the visible players based on your current mouse position.
    // meaning around your tee as well as around your mouse position two bounding boxes are created that
    // remove entities from visibility when the other entits is not within the bounding box around the player
    // as well as not around the bounding box around the cursor position.
    double GetCurrentCursorPositionX(){return m_CurrentTickPlayer.m_Input_TargetX;}
    double GetCurrentCursorPositionY(){return m_CurrentTickPlayer.m_Input_TargetX;}

    double GetCurrentPlayerPositionX(){return m_CurrentTickPlayer.m_Core_X;}
    double GetCurrentPlayerPositionY(){return m_CurrentTickPlayer.m_Core_Y;}

    struct PointAtTick
    {
        int tick;
        double x;
        double y;
    };
    double GetLongestDistancePerTickOfThreeConsequtiveMousePositions(){ return m_LongestDistancePerTickOfThreeConsequtiveMousePositions;}
    std::vector<PointAtTick> GetThreeConsequtiveMousePositionsWithLongestDistance(){return m_ThreeConsequtiveMousePositionsWithLongestDistance;};
    std::vector<PointAtTick> GetThreeConsequtiveMousePositionsWithNearlyIdenticalFirstAndLastPosition(){return m_ThreeConsequtiveMousePositionsWithNearlyIdenticalFirstAndLastPosition;}
    double GetDistanceOfNearlyIdenticalFirstAndLastPosition(){return m_DistanceOfNearlyIdenticalFirstAndLastPosition;}
    double GetDistancePerTickOfNearlyIdenticalFirstAndLastPosition(){return m_DistancePerTickOfNearlyIdenticalFirstAndLastPosition;}
    int GetNearlyIdenticalFirstAndLastPositionCounter(){return m_NearlyIdenticalFirstAndLastPositionCounter;}
    static int CalculateTicksPassed(const std::deque<PointAtTick> &q){
        if(q.size() < 2){
            return -1;
        }
        return q.at(q.size() -1).tick - q.at(0).tick;
    }

    // distance mouse travelled in q.size() inputs
    static double CalculateDistance(const std::deque<PointAtTick> &q){
        if(q.size() < 2){
            return -1.0;
        }
        double distance = 0;

        // distance
        for (size_t i = 0; i < q.size() -1; ++i)
        {
            distance += Distance(q.at(i).x, q.at(i).y, q.at(i+1).x, q.at(i+1).y);
        }
        return distance;
    }
    // distance mouse travelled in q.size() inputs
    static double CalculateDistance(const std::vector<PointAtTick> &q){
        if(q.size() < 2){
            return -1.0;
        }
        double distance = 0;

        // distance
        for (size_t i = 0; i < q.size() -1; ++i)
        {
            distance += Distance(q.at(i).x, q.at(i).y, q.at(i+1).x, q.at(i+1).y);
        }
        return distance;
    }
    static double CalculateDistancePerTick(const std::deque<PointAtTick> &q){
        if(q.size() < 2){
            return -1.0;
        }
        // mouse speed calculation
        double distance = CalculateDistance(q);
        // time in ticks
        int ticks = CalculateTicksPassed(q);

        double speed = distance / ticks;
        return speed;
    }
    static std::string ConvertToString(const std::vector<PointAtTick> &vector){
        std::stringstream s;
        s << "[ ";
        for (size_t i = 0; i < vector.size(); ++i)
            {
                s << "Tick: " << vector.at(i).tick
                  << " (" << vector.at(i).x
                  << ", " << vector.at(i).y
                  << ")";
                 if (i < vector.size() -1)
                 {
                    s << ", ";
                 }
            }
        s << "]";
        return s.str();
    }
    // ########## long term data end ##########

    void SetPlayerEnteredServerOnTick(int tick){
        if (tick > 0)
        {
            m_PlayerEnteredServerOnTick = tick;
        }
    }

    bool IsPlayerEnteredServerOnTickSet(){
        return m_PlayerEnteredServerOnTick > 0;
    }

    void ResetPlayerEnteredServerOnTick(){
        m_PlayerEnteredServerOnTick = -1;
    }

    int GetPlayerEnteredServerOnTick(){
        return m_PlayerEnteredServerOnTick;
    }

    int GetTicksPassedSincePlayerEnteredServer(){
        if (IsPlayerEnteredServerOnTickSet())
        {
            int timePassed = Server()->Tick() - GetPlayerEnteredServerOnTick();
            return timePassed > 0 ? timePassed : -1;
        } else {
            return -1;
        }
    }

    int GetSecondsPassedAfterPlayerEnteredServer(){
        int ticksPassed = GetTicksPassedSincePlayerEnteredServer();
        if (ticksPassed > 0)
        {
            return static_cast<int>(ticksPassed / Server()->TickSpeed());
        } else {
            return -1;
        }
    }

    bool CanDetectionBeStarted(){
        // Player has to be 10 Seconds on the server in order for the detection to start
        // checking stuff
        if (GetSecondsPassedAfterPlayerEnteredServer() > 10)
        {
            return true;
        } else {
            return false;
        }
    }


private:
    CCharacter *m_pCharacter;
    CGameContext *m_pGameServer;

    CGameContext *GameServer() const {return m_pGameServer; }
    IServer *Server() const;

    // rainbow stuff
    int m_LastRainbowTick{0};
    // rainbow end

    // ########## snapshot stuff ##########
    /**
     * Basically this works like so: An admin takes a screenshot before
     * and one after some event and can then analyze the before and after events side by side.
     *
     * Each time the player has a new mouse input, not movement, that player's whole character
     * consisting of player core and player input is added to one of two snapshot sets.
     * If the snapshot set has enough elements, the snapshot procedure is halted.
     * After two snapshots with two full snapshot sets the admin is able to print the results side by side.
     * Further analysis of one individual snapshot could be added later on.
     *
     */
    std::set<TickPlayer> m_SnapshotOne;
    std::set<TickPlayer> m_SnapshotTwo;
    bool m_IsSnapshotActive{false};
    bool m_OldIsSnapshotActive{false};
    int m_SnapshotCount{1};
    size_t m_SnapshotWantedLength{SNAPSHOT_DEFAULT_LENGTH};


    void IncSnapshotCount() {++m_SnapshotCount;}
    /**
     * @brief Used to switch between the two snapshot sets.
     */
    int GetSnapshotCount() {return m_SnapshotCount;}
    size_t GetCurrentSnapshotSize() { return (GetCurrentSnapshotNumber() == 1 ? m_SnapshotOne.size() : m_SnapshotTwo.size()); }
    void ResetSnapshots() {
        m_SnapshotWantedLength = SNAPSHOT_DEFAULT_LENGTH;
        m_SnapshotCount = 1;
        m_SnapshotOne.clear();
        m_SnapshotTwo.clear();
    }
    void ResetCurrentSnapshot() { GetCurrentSnapshotNumber() == 1 ? m_SnapshotOne.clear() : m_SnapshotTwo.clear();}
    TickPlayer m_OldTickPlayer;
    TickPlayer m_CurrentTickPlayer;
    void FillCurrentTickPlayer();
    void DoSnapshot();
    // ########## snapshot stuff end ##########

    // ########## log term data ##########
    void UpdateLongTermDataOnTick();
    double m_BiggestCursorDistanceFromTee{0};

    std::deque<PointAtTick> m_LastThreeMousePositions{};
    double m_LongestDistanceTravelledBetweenLastTheePositions{0};
    double m_LongestDistancePerTickOfThreeConsequtiveMousePositions{-1.0};
    std::vector<PointAtTick> m_ThreeConsequtiveMousePositionsWithLongestDistance{};
    std::vector<PointAtTick> m_ThreeConsequtiveMousePositionsWithNearlyIdenticalFirstAndLastPosition{};
    double m_DistancePerTickOfNearlyIdenticalFirstAndLastPosition{-1.0};
    double m_m_DistanceTravelledBetweenNearlyIdenticalFirstAndLastPosition{-1};
    double m_DistanceOfNearlyIdenticalFirstAndLastPosition{-1.0};
    int m_NearlyIdenticalFirstAndLastPositionCounter{0};
    // fill the deque
    void UpdateLastThreeMousePositionsOnTick(){
        if (!m_CurrentTickPlayer.IsFull())
        {
            return;
        }
        PointAtTick p;
        // gest stuff from current tickplayer
        p.x = m_CurrentTickPlayer.m_Input_TargetX;
        p.y = m_CurrentTickPlayer.m_Input_TargetY;
        p.tick = m_CurrentTickPlayer.m_Tick;

        // below 3, just pushback
        if(m_LastThreeMousePositions.size() < 3){
            m_LastThreeMousePositions.push_back(p);
        } else {
            // == 3, remove front and push back p to keep this at 3 
            m_LastThreeMousePositions.pop_front();
            m_LastThreeMousePositions.push_back(p);
        }
    }
    // calculate speed
    void CalculateBasedOnLastThreeMousePositionsOnTick();

    bool IsZoomIndicatorCursorDistance(double distance);
    std::vector<double> m_ZoomDistances{};


    // ########## helper functions ##########
    static double Angle(int meX, int meY, int otherX, int otherY);
    static double Distance(int meX, int meY, int otherX, int otherY);
    // ########## helper functions end ##########

    // ########## log term data end ##########


    //
    bool m_Spawning;
    int m_ClientID;
    int m_Team;
    int m_PlayerEnteredServerOnTick{-1};
};

#endif
