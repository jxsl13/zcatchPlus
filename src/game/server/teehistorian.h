
#ifndef GAME_SERVER_TEE_HISTORIAN_H
#define  GAME_SERVER_TEE_HISTORIAN_H

#include <engine/console.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <engine/shared/uuid_manager.h>
#include <engine/storage.h>
#include <engine/server.h>
#include <game/server/gamecontroller.h>
#include <game/version.h>


#include <game/generated/protocol.h>
#include <time.h>
#include <base/sqlite.h>

#include <thread>
#include <queue>

struct CConfiguration;
class CTuningParams;
class CUuidManager;

class CTeeHistorian
{
public:
	typedef void (*WRITE_CALLBACK)(const void *pData, int DataSize, void *pUser);

	struct CGameInfo
	{
		CUuid m_GameUuid;
		const char *m_pServerVersion;
		time_t m_StartTime;

		const char *m_pServerName;
		int m_ServerPort;
		const char *m_pGameType;

		const char *m_pMapName;
		int m_MapSize;
		int m_MapCrc;

		CConfiguration *m_pConfig;
		CTuningParams *m_pTuning;
		CUuidManager *m_pUuids;
	};

	CTeeHistorian();
	void RetrieveMode(bool OnInit);
	void Stop();
	void CheckHistorianModeToggled();
	void OnInit(IStorage *pStorage, IServer *pServer, IGameController *pController, CTuningParams *pTuning, CGameContext *pGameContext);
	ASYNCIO *GetHistorianFile() {return m_pTeeHistorianFile;};

	void Reset(const CGameInfo *pGameInfo, WRITE_CALLBACK pfnWriteCallback, void *pUser);
	void Finish();
	void OnShutDown(bool FinalShutdown);
	void OnSave();
	void CloseDatabase();
	bool Starting() const { return m_State == STATE_START; }

	void BeginTick(int Tick);

	void BeginPlayers();
	void RecordPlayer(int JoinHash, const char* ClientNick, int ClientID, const CNetObj_CharacterCore *pChar);
	void RecordDeadPlayer(int ClientID);
	void EndPlayers();

	void BeginInputs();
	void RecordPlayerInput(int JoinHash, const char* ClientNick, int ClientID, const CNetObj_PlayerInput *pInput);
	void RecordPlayerMessage(int ClientID, const void *pMsg, int MsgSize);
	void RecordPlayerJoin(int ClientJoinHash, const char* ClientNick, int ClientID, int Tick);
	void RecordPlayerDrop(int ClientJoinHash, const char* ClientNick, int ClientID, int Tick, const char *pReason);
	void RecordConsoleCommand(const char* ClientName, int ClientID, int FlagMask, const char *pCmd, IConsole::IResult *pResult);
	void RecordTestExtra();
	void EndInputs();

	void EndTick();

	void RecordAuthInitial(int ClientID, int Level, const char *pAuthName);
	void RecordAuthLogin( const char* ClientNick, int ClientID, int Level, const char *pAuthName);
	void RecordAuthLogout( const char* ClientNick, int ClientID);

	int GetTeeHistorianMode() {return m_HistorianMode;}
	// SELECT FROM SQLite DB statements for later real time analysis.
	//TODO: Create another analysis table which is updated using sql queries.

	int m_Debug; // Possible values: 0, 1, 2.
	enum tee_historian_mode
	{
		MODE_NONE = 0,
		MODE_TEE_HISTORIAN = 1,
		MODE_SQLITE = 2,
	};

private:

	/*teehistorian*/
	void WriteHeader(const CGameInfo *pGameInfo);
	void WriteExtra(CUuid Uuid, const void *pData, int DataSize);
	void EnsureTickWrittenPlayerData(int ClientID);
	void EnsureTickWritten();
	void WriteTick();
	void Write(const void *pData, int DataSize);


	/*SQLiteHistorian*/
	void CreateDatabase(const char* filename);
	void GeneralCheck();
	void OptimizeDatabase();
	void BeginTransaction();
	void EndTransaction();
	void MiddleTransaction();

	void InsertIntoRconActivityTable(char *NickName, char* TimeStamp, char *Command, char *Arguments);
	void InsertIntoPlayerMovementTable(int ClientJoinHash,  char *TimeStamp, int Tick, int x, int y, int old_x, int old_y);
	void InsertIntoPlayerInputTable(int ClientJoinHash, char *TimeStamp, int Tick, int Direction, int TargetX, int TargetY, int Jump, int Fire, int Hook, int PlayerFlags, int WantedWeapon, int NextWeapon, int PrevWeapon);
	void InsertIntoPlayerConnectedStateTable(int ClientJoinHash, char* NickName, int ClientID, char *TimeStamp, int Tick, bool ConnectedState, char* Reason);

	void CreateRconActivityTable();
	void CreatePlayerMovementTable();
	void CreatePlayerInputTable();
	void CreatePlayerConnectedStateTable();


	char* GetTimeStamp();

	enum
	{
		STATE_START,
		STATE_BEFORE_TICK,
		STATE_BEFORE_PLAYERS,
		STATE_PLAYERS,
		STATE_BEFORE_INPUTS,
		STATE_INPUTS,
		STATE_BEFORE_ENDTICK,
		NUM_STATES,
	};

	struct CPlayer
	{
		bool m_Alive;
		int m_X;
		int m_Y;

		CNetObj_PlayerInput m_Input;
		bool m_InputExists;
	};

	WRITE_CALLBACK m_pfnWriteCallback;
	void *m_pWriteCallbackUserdata;

	int m_State;
	int m_HistorianMode;
	ASYNCIO *m_pTeeHistorianFile;
	/*SQLiteHistorian*/
	sqlite3 *m_SqliteDB;
	std::timed_mutex m_SqliteMutex;
	std::queue<std::thread*> m_Threads;

	void AddThread(std::thread *thread) { m_Threads.push(thread); };
	void CleanThreads() {
		unsigned int size = m_Threads.size();
		for (int i = 0; i < size;i++)
		{
			std::thread *t = m_Threads.front();
			m_Threads.pop();
			if(t && !(t->joinable())){
				t->join();
				delete t;
			} else if(t){
				m_Threads.push(t);
			}
		}
		// OS supported thread limit should be compiled with.
		while(size >= (std::thread::hardware_concurrency()-1))
		{
			std::thread *t = m_Threads.front();
			m_Threads.pop();
			t->join();
			delete t;
			size = m_Threads.size();
		}

	};
	void JoinThreads() {
		while (!m_Threads.empty()) {
			std::thread *t = m_Threads.front();
			m_Threads.pop();
			t->join();
			delete t;

		} ;
	};


	char *m_pFileName;
	IStorage *m_pStorage;
	IServer *m_pServer;
	IGameController *m_pController;
	CTuningParams *m_pTuning;
	CGameContext *m_pGameContext;
	int m_OldHistorianMode;


int m_LastWrittenTick;
bool m_TickWritten;
int m_Tick;
int m_PrevMaxClientID;
int m_MaxClientID;
bool m_TickTresholdReached;
CPlayer m_aPrevPlayers[MAX_CLIENTS];
CUuid m_GameUuid;
};

#endif
