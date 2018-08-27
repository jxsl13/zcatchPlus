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
	std::set<int> m_PlayerIrregularFlags;

	/**
	 * @brief Flags 1,2,4,8 are default vanilla flags for chatting etc.
	 *  and the flag 16 is for aimlines in the ddnet client (noby told me)
	 *  meaning: 1 + 2 + 4 + 8 + 16 = 31 and anything above it is not regular
	 *  flag 32 seems to be some H-client flag
	 */
	static bool IsIrregularFlag(int Flags){ return Flags > 63;}
	void CheckIrregularFlags(){if(IsIrregularFlag(m_PlayerFlags)) m_PlayerIrregularFlags.insert(m_PlayerFlags);}
	void ClearIrregularFlags(){ m_PlayerIrregularFlags.clear();}
	/**
	 * @brief Returns a vector created from the set of irregular flags.
	 */
	std::vector<int> GetIrregularFlags(){
		std::vector<int> v;
		std::set<int>::iterator setIt = m_PlayerIrregularFlags.begin();
		for (size_t i = 0; i < m_PlayerIrregularFlags.size(); ++i)
		{
			v.push_back(*setIt);
			setIt++;
		}
		return v;}

	/**
	 * @brief Checks if the player has some tracked irregular flags in his flags set.
	 */
	bool HasIrregularFlags(){return m_PlayerIrregularFlags.size() > 0;};

	/**
	 * @brief Returns a vector with at least 32 elements representing zeroes and ones.
	 * Accessing this vector using the index, which accesses the specific flag at 2^(index)
	 */
	static std::vector<bool> ConvertToBitMask(int Flags);


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
	bool IsRainbowTee(){ return m_IsRainbowBodyTee || m_IsRainbowFeetTee;}
	void ResetRainbowTee(){ m_IsRainbowBodyTee = false; m_IsRainbowFeetTee = false;}
	void GiveBodyRainbow(){ m_IsRainbowBodyTee = true;}
	void GiveFeetRainbow(){ m_IsRainbowFeetTee = true;}
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

	void SetTeeHistorianTracked(bool tracked){m_TeeHistorianTracked = tracked;}
	bool GetTeeHistorianTracked(){return m_TeeHistorianTracked;}
	/*teehistorian player tracking*/

	//########## snapshot stuff ##########
#define SNAPSHOT_DEFAULT_LENGTH 10
	int GetCurrentSnapshotNumber(){return m_SnapshotCount % 2;}
	void AddAndResetCurrentTickPlayerToCurrentSnapshot(){
		if(m_CurrentTickPlayer.equalInput(m_OldTickPlayer)){
			return;
		}
		m_OldTickPlayer = m_CurrentTickPlayer;
		GetCurrentSnapshotNumber() == 1 ?
		m_SnapshotOne.insert(m_CurrentTickPlayer) :
		m_SnapshotTwo.insert(m_CurrentTickPlayer);
		m_CurrentTickPlayer.ResetTickData();
	}

	void SetSnapshotWantedLength(int ticks){ticks > 0 ? m_SnapshotWantedLength = ticks : m_SnapshotWantedLength = SNAPSHOT_DEFAULT_LENGTH;}
	size_t GetSnapshotWantedLength(){return m_SnapshotWantedLength;}
	int GetSnapsLeft(){return GetSnapshotWantedLength() - GetCurrentSnapshotSize();}
	void EnableSnapshot(){m_IsSnapshotActive = true;}
	bool IsSnapshotEnabled(){return m_IsSnapshotActive;}

	bool IsSnapshotFull(){
		return
		m_SnapshotOne.size() > 0 &&
		m_SnapshotTwo.size() > 0 &&
		m_SnapshotOne.size() == m_SnapshotTwo.size() &&
		m_SnapshotOne.size() == GetSnapshotWantedLength() &&
		GetCurrentSnapshotNumber() == 1;}

	std::vector<TickPlayer> GetFirstSnapshotResult(){
		std::vector<TickPlayer> v;
		for (TickPlayer t : m_SnapshotOne){
			v.push_back(t);
		}
		std::sort(v.begin(), v.end());
		return v;
	}

	std::vector<TickPlayer> GetSecondSnapshotResult(){
		std::vector<TickPlayer> v;
		for (TickPlayer t : m_SnapshotTwo){
			v.push_back(t);
		}
		std::sort(v.begin(), v.end());
		return v;
	};
	// ########## snapshot stuff end ##########

private:
	CCharacter *m_pCharacter;
	CGameContext *m_pGameServer;

	CGameContext *GameServer() const {return m_pGameServer; }
	IServer *Server() const;

	// rainbow stuff
	int m_RainbowBodyStep{0};
	int m_RainbowFeetStep{0};
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


	void IncSnapshotCount(){++m_SnapshotCount;}
	/**
	 * @brief Used to switch between the two snapshot sets.
	 */
	int GetSnapshotCount(){return m_SnapshotCount;}
	size_t GetCurrentSnapshotSize(){ return (GetCurrentSnapshotNumber() == 1 ? m_SnapshotOne.size() : m_SnapshotTwo.size()); }
	void ResetSnapshots(){
		m_SnapshotWantedLength = SNAPSHOT_DEFAULT_LENGTH;
		m_SnapshotCount = 1;
		m_SnapshotOne.clear();
		m_SnapshotTwo.clear();}
	void ResetCurrentSnapshot(){ GetCurrentSnapshotNumber() == 1 ? m_SnapshotOne.clear() : m_SnapshotTwo.clear();}
	TickPlayer m_OldTickPlayer;
	TickPlayer m_CurrentTickPlayer;
	void DoSnapshot();

	// ########## snapshot stuff end ##########

	//
	bool m_Spawning;
	int m_ClientID;
	int m_Team;
};

#endif
