#include "teehistorian.h"

#include <engine/shared/config.h>
#include <engine/shared/snapshot.h>
#include <game/gamecore.h>
#include <engine/shared/protocol.h>
#include <sys/time.h>
#include <game/server/gamecontext.h>


static const char TEEHISTORIAN_NAME[] = "teehistorian@ddnet.tw";
static const CUuid TEEHISTORIAN_UUID = CalculateUuid(TEEHISTORIAN_NAME);
static const char TEEHISTORIAN_VERSION[] = "2";



/**INFO: TODO: Gameserver-> is accessible from here */

#define UUID(id, name) static const CUuid UUID_ ## id = CalculateUuid(name);
#include <engine/shared/teehistorian_ex_chunks.h>
#undef UUID

enum
{
	TEEHISTORIAN_NONE,
	TEEHISTORIAN_FINISH,
	TEEHISTORIAN_TICK_SKIP,
	TEEHISTORIAN_PLAYER_NEW,
	TEEHISTORIAN_PLAYER_OLD,
	TEEHISTORIAN_INPUT_DIFF,
	TEEHISTORIAN_INPUT_NEW,
	TEEHISTORIAN_MESSAGE,
	TEEHISTORIAN_JOIN,
	TEEHISTORIAN_DROP,
	TEEHISTORIAN_CONSOLE_COMMAND,
	TEEHISTORIAN_EX,
};

static char EscapeJsonChar(char c)
{
	switch (c)
	{
	case '\"': return '\"';
	case '\\': return '\\';
	case '\b': return 'b';
	case '\n': return 'n';
	case '\r': return 'r';
	case '\t': return 't';
	// Don't escape '\f', who uses that. :)
	default: return 0;
	}
}


void CTeeHistorian::OnInit(char *pFileName, IStorage *pStorage, IServer *pServer, IGameController *pController, CTuningParams *pTuning, CGameContext *pGameContext) {
	RetrieveMode();

	m_pFileName = pFileName;
	m_pStorage = pStorage;
	m_pServer = pServer;
	m_pController = pController;
	m_pTuning = pTuning;
	m_pGameContext = pGameContext;
	m_GameUuid = RandomUuid();

	if (m_HistorianMode) {
		char aGameUuid[UUID_MAXSTRSIZE];
		FormatUuid(m_GameUuid, aGameUuid, sizeof(aGameUuid));
		char aFilename[64];


		if (m_HistorianMode == MODE_SQLITE)
		{
			if (m_pFileName != NULL) {
				str_format(aFilename, sizeof(aFilename), "teehistorian/%s.db", m_pFileName);
			} else {
				str_format(aFilename, sizeof(aFilename), "teehistorian/%s.db", aGameUuid);
			}

			CreateDatabase(aFilename);
			//sqlite performance
			


		} else if (m_HistorianMode == MODE_TEE_HISTORIAN) {


			str_format(aFilename, sizeof(aFilename), "teehistorian/%s.teehistorian", aGameUuid);





			IOHANDLE File = m_pStorage->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
			if (!File)
			{
				dbg_msg("teehistorian", "failed to open '%s'", aFilename);
				m_pServer->SetErrorShutdown("teehistorian open error");
			}
			else
			{
				dbg_msg("teehistorian", "recording to '%s'", aFilename);
			}

			m_pTeeHistorianFile = (aio_new(File));

			CUuidManager Empty;

			CTeeHistorian::CGameInfo GameInfo;
			GameInfo.m_GameUuid = m_GameUuid;
			GameInfo.m_pServerVersion = "jsxl's zcatch " GAME_VERSION;
			GameInfo.m_StartTime = time(0);

			GameInfo.m_pServerName = g_Config.m_SvName;
			GameInfo.m_ServerPort = g_Config.m_SvPort;
			GameInfo.m_pGameType = m_pController->m_pGameType;

			GameInfo.m_pConfig = &g_Config;
			GameInfo.m_pTuning = m_pTuning;
			GameInfo.m_pUuids = &Empty;

			char aMapName[128];
			m_pServer->GetMapInfo(aMapName, sizeof(aMapName), &GameInfo.m_MapSize, &GameInfo.m_MapCrc);
			GameInfo.m_pMapName = aMapName;

			Reset(&GameInfo, CGameContext::TeeHistorianWrite, m_pGameContext);
			dbg_msg("TeeHistorian", "Initialization done.");
		}
	}

}

void CTeeHistorian::OnShutDown() {

	Finish();

	if (m_HistorianMode == MODE_SQLITE)
	{
		CleanThreads();
		CloseDatabase();



	} else  if (m_HistorianMode == MODE_TEE_HISTORIAN)
	{
		aio_close(m_pTeeHistorianFile);
		aio_wait(m_pTeeHistorianFile);
		int Error = aio_error(m_pTeeHistorianFile);
		if (Error)
		{
			dbg_msg("teehistorian", "error closing file, err=%d", Error);
			//GameContext->Server()->SetErrorShutdown("teehistorian close error");
		}
		aio_free(m_pTeeHistorianFile);

	}
}

void CTeeHistorian::OnSave() {
	OnShutDown();
	OnInit(m_pFileName, m_pStorage, m_pServer, m_pController, m_pTuning, m_pGameContext);
}

static char *EscapeJson(char *pBuffer, int BufferSize, const char *pString)
{
	dbg_assert(BufferSize > 0, "can't null-terminate the string");
	// Subtract the space for null termination early.
	BufferSize--;

	char *pResult = pBuffer;
	while (BufferSize && *pString)
	{
		char c = *pString;
		pString++;
		char Escaped = EscapeJsonChar(c);
		if (Escaped)
		{
			if (BufferSize < 2)
			{
				break;
			}
			*pBuffer++ = '\\';
			*pBuffer++ = Escaped;
			BufferSize -= 2;
		}
		// Assuming ASCII/UTF-8, "if control character".
		else if (c < 0x20)
		{
			// \uXXXX
			if (BufferSize < 6)
			{
				break;
			}
			str_format(pBuffer, BufferSize, "\\u%04x", c);
			BufferSize -= 6;
		}
		else
		{
			*pBuffer++ = c;
		}
	}
	*pBuffer = 0;
	return pResult;
}

CTeeHistorian::CTeeHistorian()
{
	m_State = STATE_START;
	m_pfnWriteCallback = 0;
	m_pWriteCallbackUserdata = 0;
	m_HistorianMode = MODE_NONE;
	m_GameUuid = RandomUuid();
}


void CTeeHistorian::RetrieveMode() {
	m_HistorianMode = g_Config.m_SvTeeHistorian + g_Config.m_SvSqliteHistorian;
}

void CTeeHistorian::Reset(const CGameInfo *pGameInfo, WRITE_CALLBACK pfnWriteCallback, void *pUser)
{
	if (m_HistorianMode) {


		dbg_assert(m_State == STATE_START || m_State == STATE_BEFORE_TICK, "invalid teehistorian state");

		m_Debug = 0;

		m_Tick = 0;
		m_LastWrittenTick = 0;
		// Tick 0 is implicit at the start, game starts as tick 1.
		m_TickWritten = true;
		m_MaxClientID = MAX_CLIENTS;
		// `m_PrevMaxClientID` is initialized in `BeginTick`
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			m_aPrevPlayers[i].m_Alive = false;
			m_aPrevPlayers[i].m_InputExists = false;
		}


		if (m_HistorianMode == MODE_SQLITE)
		{
			/* code */
		} else if (m_HistorianMode == MODE_TEE_HISTORIAN) {
			m_pfnWriteCallback = pfnWriteCallback;
			m_pWriteCallbackUserdata = pUser;
			WriteHeader(pGameInfo);
		}

		m_State = STATE_START;


	}

}

void CTeeHistorian::WriteHeader(const CGameInfo *pGameInfo)
{


	Write(&TEEHISTORIAN_UUID, sizeof(TEEHISTORIAN_UUID));

	char aGameUuid[UUID_MAXSTRSIZE];
	char aStartTime[128];

	FormatUuid(pGameInfo->m_GameUuid, aGameUuid, sizeof(aGameUuid));
	str_timestamp_ex(pGameInfo->m_StartTime, aStartTime, sizeof(aStartTime), "%Y-%m-%dT%H:%M:%S%z");

	char aCommentBuffer[128];
	char aServerVersionBuffer[128];
	char aStartTimeBuffer[128];
	char aServerNameBuffer[128];
	char aGameTypeBuffer[128];
	char aMapNameBuffer[128];

	char aJson[2048];

#define E(buf, str) EscapeJson(buf, sizeof(buf), str)

	str_format(aJson, sizeof(aJson), "{\"comment\":\"%s\",\"version\":\"%s\",\"game_uuid\":\"%s\",\"server_version\":\"%s\",\"start_time\":\"%s\",\"server_name\":\"%s\",\"server_port\":\"%d\",\"game_type\":\"%s\",\"map_name\":\"%s\",\"map_size\":\"%d\",\"map_crc\":\"%08x\",\"config\":{",
	           E(aCommentBuffer, TEEHISTORIAN_NAME),
	           TEEHISTORIAN_VERSION,
	           aGameUuid,
	           E(aServerVersionBuffer, pGameInfo->m_pServerVersion),
	           E(aStartTimeBuffer, aStartTime),
	           E(aServerNameBuffer, pGameInfo->m_pServerName),
	           pGameInfo->m_ServerPort,
	           E(aGameTypeBuffer, pGameInfo->m_pGameType),
	           E(aMapNameBuffer, pGameInfo->m_pMapName),
	           pGameInfo->m_MapSize,
	           pGameInfo->m_MapCrc);
	Write(aJson, str_length(aJson));

	char aBuffer1[1024];
	char aBuffer2[1024];
	bool First = true;

#define MACRO_CONFIG_INT(Name,ScriptName,Def,Min,Max,Flags,Desc) \
	if((Flags)&CFGFLAG_SERVER && !((Flags)&CFGFLAG_NONTEEHISTORIC) && pGameInfo->m_pConfig->m_##Name != (Def)) \
	{ \
		str_format(aJson, sizeof(aJson), "%s\"%s\":\"%d\"", \
			First ? "" : ",", \
			E(aBuffer1, #ScriptName), \
			pGameInfo->m_pConfig->m_##Name); \
		Write(aJson, str_length(aJson)); \
		First = false; \
	}

#define MACRO_CONFIG_STR(Name,ScriptName,Len,Def,Flags,Desc) \
	if((Flags)&CFGFLAG_SERVER && !((Flags)&CFGFLAG_NONTEEHISTORIC) && str_comp(pGameInfo->m_pConfig->m_##Name, (Def)) != 0) \
	{ \
		str_format(aJson, sizeof(aJson), "%s\"%s\":\"%s\"", \
			First ? "" : ",", \
			E(aBuffer1, #ScriptName), \
			E(aBuffer2, pGameInfo->m_pConfig->m_##Name)); \
		Write(aJson, str_length(aJson)); \
		First = false; \
	}

#define MACRO_CONFIG_INT_ACCESSLEVEL(Name,ScriptName,Def,Min,Max,Flags,Desc,AccessLevel) MACRO_CONFIG_INT(Name,ScriptName,Def,Min,Max,Flags,Desc)
#define MACRO_CONFIG_STR_ACCESSLEVEL(Name,ScriptName,Len,Def,Flags,Desc,AccessLevel) MACRO_CONFIG_STR(Name,ScriptName,Len,Def,Flags,Desc)
#include <engine/shared/config_variables.h>

#undef MACRO_CONFIG_INT_ACCESSLEVEL
#undef MACRO_CONFIG_STR_ACCESSLEVEL
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_STR

	str_format(aJson, sizeof(aJson), "},\"tuning\":{");
	Write(aJson, str_length(aJson));

	First = true;

	static const float TicksPerSecond = 50.0f;

#define MACRO_TUNING_PARAM(Name,ScriptName,Value) \
	if(pGameInfo->m_pTuning->m_##Name.Get() != (int)((Value)*100)) \
	{ \
		str_format(aJson, sizeof(aJson), "%s\"%s\":\"%d\"", \
			First ? "" : ",", \
			E(aBuffer1, #ScriptName), \
			pGameInfo->m_pTuning->m_##Name.Get()); \
		Write(aJson, str_length(aJson)); \
		First = false; \
	}
#include <game/tuning.h>
#undef MACRO_TUNING_PARAM

	str_format(aJson, sizeof(aJson), "},\"uuids\":[");
	Write(aJson, str_length(aJson));

	for (int i = 0; i < pGameInfo->m_pUuids->NumUuids(); i++)
	{
		str_format(aJson, sizeof(aJson), "%s\"%s\"",
		           i == 0 ? "" : ",",
		           E(aBuffer1, pGameInfo->m_pUuids->GetName(OFFSET_UUID + i)));
		Write(aJson, str_length(aJson));
	}

	str_format(aJson, sizeof(aJson), "]}");
	Write(aJson, str_length(aJson));
	Write("", 1); // Null termination.
}

void CTeeHistorian::WriteExtra(CUuid Uuid, const void *pData, int DataSize)
{

	EnsureTickWritten();

	CPacker Ex;
	Ex.Reset();
	Ex.AddInt(-TEEHISTORIAN_EX);
	Ex.AddRaw(&Uuid, sizeof(Uuid));
	Ex.AddInt(DataSize);
	Write(Ex.Data(), Ex.Size());
	Write(pData, DataSize);
}


void CTeeHistorian::BeginTick(int Tick)
{
	if (m_HistorianMode)
	{
		dbg_assert(m_State == STATE_START || m_State == STATE_BEFORE_TICK, "invalid teehistorian state");

		m_Tick = Tick;
		m_TickWritten = false;

		if (m_Debug > 1)
		{
			dbg_msg("teehistorian", "tick %d", Tick);
		}

		m_State = STATE_BEFORE_PLAYERS;
	}
}

void CTeeHistorian::BeginPlayers()
{
	if (m_HistorianMode)
	{
		dbg_assert(m_State == STATE_BEFORE_PLAYERS, "invalid teehistorian state");

		m_PrevMaxClientID = m_MaxClientID;
		m_MaxClientID = -1;

		m_State = STATE_PLAYERS;
	}
}

void CTeeHistorian::EnsureTickWrittenPlayerData(int ClientID)
{
	dbg_assert(ClientID > m_MaxClientID, "invalid player data order");
	m_MaxClientID = ClientID;

	if (!m_TickWritten && (ClientID > m_PrevMaxClientID || m_LastWrittenTick + 1 != m_Tick))
	{
		WriteTick();
	}
	else
	{
		// Tick is implicit.
		m_LastWrittenTick = m_Tick;
		m_TickWritten = true;
	}
}


void CTeeHistorian::RecordPlayer(int ClientJoinHash, const char* ClientNick, int ClientID, const CNetObj_CharacterCore *pChar)
{

	if (m_HistorianMode) {


		dbg_assert(m_State == STATE_PLAYERS, "invalid teehistorian state");

		CPlayer *pPrev = &m_aPrevPlayers[ClientID];

		if (m_HistorianMode == MODE_SQLITE) {

			//if (!pPrev->m_Alive || pPrev->m_X != pChar->m_X || pPrev->m_Y != pChar->m_Y) // doesn't write every tick


			EnsureTickWrittenPlayerData(ClientID);
			char *Nick = (char*)malloc(MAX_NAME_LENGTH * sizeof(char));
			str_copy(Nick, ClientNick, MAX_NAME_LENGTH * sizeof(char));

			char *aDate = GetTimeStamp();
			int Tick = m_Tick;
			int X = pChar->m_X;
			int Y = pChar->m_Y;
			int OldX = pPrev->m_X;
			int OldY = pPrev->m_Y;

			/**
			 * before detaching the thread let's check
			 * if enough ticks passed to write out
			 * transactions to the database
			 */

			if (Tick % (50 * g_Config.m_SvSqliteWriteInterval) == 0) {
				AddThread(new std::thread(&CTeeHistorian::MiddleTransaction, this));
				CleanThreads();

			}

			/*it does not matter if this is written before or after this thread is executed*/
			AddThread(new std::thread(&CTeeHistorian::InsertIntoPlayerMovementTable,
			                                 this,
			                                 ClientJoinHash,
			                                 aDate,
			                                 Tick,
			                                 X,
			                                 Y,
			                                 OldX,
			                                 OldY));



		} else if (m_HistorianMode == MODE_TEE_HISTORIAN) {

			if (!pPrev->m_Alive || pPrev->m_X != pChar->m_X || pPrev->m_Y != pChar->m_Y)
			{
				EnsureTickWrittenPlayerData(ClientID);

				CPacker Buffer;
				Buffer.Reset();
				if (pPrev->m_Alive)
				{
					int dx = pChar->m_X - pPrev->m_X;
					int dy = pChar->m_Y - pPrev->m_Y;
					Buffer.AddInt(ClientID);
					Buffer.AddInt(dx);
					Buffer.AddInt(dy);
					if (m_Debug)
					{
						dbg_msg("teehistorian", "diff cid=%d dx=%d dy=%d", ClientID, dx, dy);
					}
				}
				else
				{
					int x = pChar->m_X;
					int y = pChar->m_Y;
					Buffer.AddInt(-TEEHISTORIAN_PLAYER_NEW);
					Buffer.AddInt(ClientID);
					Buffer.AddInt(x);
					Buffer.AddInt(y);
					if (m_Debug)
					{
						dbg_msg("teehistorian", "new cid=%d x=%d y=%d", ClientID, x, y);
					}
				}
				Write(Buffer.Data(), Buffer.Size());
			}
		}
		// in both cases continue counting positions.
		pPrev->m_X = pChar->m_X;
		pPrev->m_Y = pChar->m_Y;
		pPrev->m_Alive = true;

	}

}

void CTeeHistorian::RecordDeadPlayer(int ClientID)
{
	if (m_HistorianMode)
	{
		dbg_assert(m_State == STATE_PLAYERS, "invalid teehistorian state");
		CPlayer *pPrev = &m_aPrevPlayers[ClientID];

		if (pPrev->m_Alive) {
			EnsureTickWrittenPlayerData(ClientID);
		}

		if (m_HistorianMode == MODE_SQLITE) {

		} else if (m_HistorianMode == MODE_TEE_HISTORIAN) {
			if (pPrev->m_Alive) {
				CPacker Buffer;
				Buffer.Reset();
				Buffer.AddInt(-TEEHISTORIAN_PLAYER_OLD);
				Buffer.AddInt(ClientID);
				if (m_Debug)
				{
					dbg_msg("teehistorian", "old cid=%d", ClientID);
				}
				Write(Buffer.Data(), Buffer.Size());
			}
		}

		pPrev->m_Alive = false;
	}

}

void CTeeHistorian::Write(const void *pData, int DataSize)
{
	if (m_HistorianMode == MODE_SQLITE)
	{
		/* code */
	} else if (m_HistorianMode == MODE_TEE_HISTORIAN) {
		m_pfnWriteCallback(pData, DataSize, m_pWriteCallbackUserdata);
	}

}


void CTeeHistorian::EnsureTickWritten()
{
	if (!m_TickWritten)
	{
		WriteTick();
	}

}

void CTeeHistorian::WriteTick()
{
	if (g_Config.m_SvSqliteHistorian) {

	} else {
		CPacker TickPacker;
		TickPacker.Reset();

		int dt = m_Tick - m_LastWrittenTick - 1;
		TickPacker.AddInt(-TEEHISTORIAN_TICK_SKIP);
		TickPacker.AddInt(dt);
		if (m_Debug)
		{
			dbg_msg("teehistorian", "skip_ticks dt=%d", dt);
		}
		Write(TickPacker.Data(), TickPacker.Size());
	}
	m_TickWritten = true;
	m_LastWrittenTick = m_Tick;

}

void CTeeHistorian::EndPlayers()
{
	if (m_HistorianMode)
	{
		dbg_assert(m_State == STATE_PLAYERS, "invalid teehistorian state");

		m_State = STATE_BEFORE_INPUTS;
	}
}

void CTeeHistorian::BeginInputs()
{
	if (m_HistorianMode)
	{
		dbg_assert(m_State == STATE_BEFORE_INPUTS, "invalid teehistorian state");

		m_State = STATE_INPUTS;
	}

}

void CTeeHistorian::RecordPlayerInput(int ClientJoinHash, const char* ClientNick, int ClientID, const CNetObj_PlayerInput * pInput)
{
	if (m_HistorianMode)
	{

		CPlayer *pPrev = &m_aPrevPlayers[ClientID];

		if (m_HistorianMode == MODE_SQLITE)
		{

			char *TimeStamp = GetTimeStamp();

			int Tick = m_Tick;
			int Direction = pInput->m_Direction;

			int TargetX = pInput->m_TargetX;
			int TargetY = pInput->m_TargetY;
			int Jump = pInput->m_Jump;
			int Fire = pInput->m_Fire;
			int Hook = pInput->m_Hook;
			int PlayerFlags = pInput->m_PlayerFlags;
			int WantedWeapon = pInput->m_WantedWeapon;
			int NextWeapon = pInput->m_NextWeapon;
			int PrevWeapon = pInput->m_PrevWeapon;
			// CmdArgs contains the given arguments.
			AddThread(new std::thread(&CTeeHistorian::InsertIntoPlayerInputTable,
			                                 this,
			                                 ClientJoinHash,
			                                 TimeStamp,
			                                 Tick,
			                                 Direction,
			                                 TargetX,
			                                 TargetY,
			                                 Jump,
			                                 Fire,
			                                 Hook,
			                                 PlayerFlags,
			                                 WantedWeapon,
			                                 NextWeapon,
			                                 PrevWeapon));


		} else if (m_HistorianMode == MODE_TEE_HISTORIAN) {

			CPacker Buffer;

			CNetObj_PlayerInput DiffInput;
			if (pPrev->m_InputExists)
			{
				if (mem_comp(&pPrev->m_Input, pInput, sizeof(pPrev->m_Input)) == 0)
				{
					return;
				}
				EnsureTickWritten();
				Buffer.Reset();

				Buffer.AddInt(-TEEHISTORIAN_INPUT_DIFF);
				CSnapshotDelta::DiffItem((int *)&pPrev->m_Input, (int *)pInput, (int *)&DiffInput, sizeof(DiffInput) / sizeof(int));
				if (m_Debug)
				{
					const int *pData = (const int *)&DiffInput;
					dbg_msg("teehistorian", "diff_input cid=%d %d %d %d %d %d %d %d %d %d %d", ClientID,
					        pData[0], pData[1], pData[2], pData[3], pData[4],
					        pData[5], pData[6], pData[7], pData[8], pData[9]);
				}
			}
			else
			{
				EnsureTickWritten();
				Buffer.Reset();
				Buffer.AddInt(-TEEHISTORIAN_INPUT_NEW);
				DiffInput = *pInput;
				if (m_Debug)
				{
					dbg_msg("teehistorian", "new_input cid=%d", ClientID);
				}
			}
			Buffer.AddInt(ClientID);
			for (int i = 0; i < (int)(sizeof(DiffInput) / sizeof(int)); i++)
			{
				Buffer.AddInt(((int *)&DiffInput)[i]);
			}

			Write(Buffer.Data(), Buffer.Size());
		}

		// in both cases the same
		pPrev->m_InputExists = true;
		pPrev->m_Input = *pInput;
	}


}

void CTeeHistorian::RecordPlayerMessage(int ClientID, const void *pMsg, int MsgSize)
{

	if (m_HistorianMode) {

		if (m_HistorianMode == MODE_SQLITE)
		{
			/* code */
		} else  if (m_HistorianMode == MODE_TEE_HISTORIAN) {

			EnsureTickWritten();

			CPacker Buffer;
			Buffer.Reset();
			Buffer.AddInt(-TEEHISTORIAN_MESSAGE);
			Buffer.AddInt(ClientID);
			Buffer.AddInt(MsgSize);
			Buffer.AddRaw(pMsg, MsgSize);

			if (m_Debug)
			{
				CUnpacker Unpacker;
				Unpacker.Reset(pMsg, MsgSize);
				int MsgID = Unpacker.GetInt();
				int Sys = MsgID & 1;
				MsgID >>= 1;
				dbg_msg("teehistorian", "msg cid=%d sys=%d msgid=%d", ClientID, Sys, MsgID);
			}

			Write(Buffer.Data(), Buffer.Size());
		}

	}


}

void CTeeHistorian::RecordPlayerJoin(int ClientJoinHash , const char* ClientNick, int ClientID, int Tick)
{
	if (m_HistorianMode)
	{
		if (m_HistorianMode == MODE_SQLITE)
		{
			char *Nick = (char*)malloc(MAX_NAME_LENGTH * sizeof(char));
			str_copy(Nick, ClientNick, MAX_NAME_LENGTH * sizeof(char));

			char* TimeStamp = GetTimeStamp();

			char *Reason = (char*)malloc(sizeof(char));
			str_copy(Reason, "", sizeof(Reason));

			AddThread(new std::thread(&CTeeHistorian::InsertIntoPlayerConnectedStateTable,
			                                 this,
			                                 ClientJoinHash,
			                                 Nick,
			                                 ClientID,
			                                 TimeStamp,
			                                 Tick,
			                                 true,
			                                 Reason));

		} else if (m_HistorianMode == MODE_TEE_HISTORIAN)
		{
			EnsureTickWritten();

			CPacker Buffer;
			Buffer.Reset();
			Buffer.AddInt(-TEEHISTORIAN_JOIN);
			Buffer.AddInt(ClientID);

			if (m_Debug)
			{
				dbg_msg("teehistorian", "join cid=%d", ClientID);
			}

			Write(Buffer.Data(), Buffer.Size());
		}
	}

}

void CTeeHistorian::RecordPlayerDrop(int ClientJoinHash, const char* ClientNick, int ClientID, int Tick, const char *pReason)
{
	if (m_HistorianMode)
	{
		if (m_HistorianMode == MODE_SQLITE)
		{
			char *Nick = (char*)malloc(MAX_NAME_LENGTH * sizeof(char));
			str_copy(Nick, ClientNick, MAX_NAME_LENGTH * sizeof(char));

			char* TimeStamp = GetTimeStamp();

			char* Reason = (char*)malloc(sizeof(char) * 128);
			str_copy(Reason, pReason, sizeof(char) * 128);

			AddThread(new std::thread(&CTeeHistorian::InsertIntoPlayerConnectedStateTable,
			                                 this,
			                                 ClientJoinHash,
			                                 Nick,
			                                 ClientID,
			                                 TimeStamp,
			                                 Tick,
			                                 false,
			                                 Reason));

			/*write to database if player leaves.*/


		} else if (m_HistorianMode == MODE_TEE_HISTORIAN)
		{

			EnsureTickWritten();

			CPacker Buffer;
			Buffer.Reset();
			Buffer.AddInt(-TEEHISTORIAN_DROP);
			Buffer.AddInt(ClientID);
			Buffer.AddString(pReason, 0);

			if (m_Debug)
			{
				dbg_msg("teehistorian", "drop cid=%d reason='%s'", ClientID, pReason);
			}

			Write(Buffer.Data(), Buffer.Size());
		}
	}
}


void CTeeHistorian::RecordConsoleCommand(const char* ClientNick, int ClientID, int FlagMask, const char *pCmd, IConsole::IResult * pResult)
{
	if (m_HistorianMode)
	{
		if (m_HistorianMode == MODE_SQLITE)
		{
			/*sqlitehistorian*/
			char *CmdArgs = (char*)malloc(512 * sizeof(char));
			for (int i = 0; i < pResult->NumArguments(); i++)
			{
				if (i == 0) {
					strcpy(CmdArgs, pResult->GetString(i));
				} else {
					strcat(CmdArgs, pResult->GetString(i));
				}
			}

			char *aDate = GetTimeStamp();


			char *Nick = (char*)malloc(MAX_NAME_LENGTH * sizeof(char));
			str_copy(Nick, ClientNick, MAX_NAME_LENGTH * sizeof(char));

			char *Command = (char*)malloc(512 * sizeof(char));
			str_copy(Command, pCmd, 512);

			// CmdArgs contains the given arguments.
			AddThread(new std::thread(&CTeeHistorian::InsertIntoRconActivityTable,
			                                 this,
			                                 Nick,
			                                 aDate,
			                                 Command,
			                                 CmdArgs));

		} else if (m_HistorianMode == MODE_TEE_HISTORIAN)
		{
			EnsureTickWritten();
			CPacker Buffer;
			Buffer.Reset();
			Buffer.AddInt(-TEEHISTORIAN_CONSOLE_COMMAND);
			Buffer.AddInt(ClientID);
			Buffer.AddInt(FlagMask);
			Buffer.AddString(pCmd, 0);
			Buffer.AddInt(pResult->NumArguments());
			for (int i = 0; i < pResult->NumArguments(); i++)
			{
				Buffer.AddString(pResult->GetString(i), 0);
			}

			if (m_Debug)
			{
				dbg_msg("teehistorian", "ccmd cid=%d cmd='%s'", ClientID, pCmd);
			}

			Write(Buffer.Data(), Buffer.Size());
		}
	}



}

void CTeeHistorian::RecordTestExtra()
{
	if (m_Debug)
	{
		dbg_msg("teehistorian", "test");
	}

	WriteExtra(UUID_TEEHISTORIAN_TEST, "", 0);
}

void CTeeHistorian::EndInputs()
{
	dbg_assert(m_State == STATE_INPUTS, "invalid teehistorian state");

	m_State = STATE_BEFORE_ENDTICK;
}

void CTeeHistorian::EndTick()
{
	dbg_assert(m_State == STATE_BEFORE_ENDTICK, "invalid teehistorian state");
	m_State = STATE_BEFORE_TICK;
}

void CTeeHistorian::RecordAuthInitial(int ClientID, int Level, const char *pAuthName)
{
	if (g_Config.m_SvSqliteHistorian)
	{
		/* code */
	} else {

		CPacker Buffer;
		Buffer.Reset();
		Buffer.AddInt(ClientID);
		Buffer.AddInt(Level);
		Buffer.AddString(pAuthName, 0);

		if (m_Debug)
		{
			dbg_msg("teehistorian", "auth_init cid=%d level=%d auth_name=%s", ClientID, Level, pAuthName);
		}

		WriteExtra(UUID_TEEHISTORIAN_AUTH_INIT, Buffer.Data(), Buffer.Size());
	}

}

void CTeeHistorian::RecordAuthLogin(const char* ClientNick, int ClientID, int Level, const char *pAuthName)
{
	if (g_Config.m_SvSqliteHistorian)
	{
		/* code */
	} else {
		/*teehistorian only*/
		CPacker Buffer;
		Buffer.Reset();
		Buffer.AddInt(ClientID);
		Buffer.AddInt(Level);
		Buffer.AddString(pAuthName, 0);

		if (m_Debug)
		{
			dbg_msg("teehistorian", "auth_login cid=%d level=%d auth_name=%s", ClientID, Level, pAuthName);
		}

		WriteExtra(UUID_TEEHISTORIAN_AUTH_LOGIN, Buffer.Data(), Buffer.Size());
	}

}

void CTeeHistorian::RecordAuthLogout(const char* ClientNick, int ClientID)
{
	if (g_Config.m_SvSqliteHistorian)
	{
		/* code */
	} else {

		CPacker Buffer;
		Buffer.Reset();
		Buffer.AddInt(ClientID);

		if (m_Debug)
		{
			dbg_msg("teehistorian", "auth_logout cid=%d", ClientID);
		}

		WriteExtra(UUID_TEEHISTORIAN_AUTH_LOGOUT, Buffer.Data(), Buffer.Size());
	}

}

void CTeeHistorian::Finish()
{
	if (m_HistorianMode) {
		dbg_assert(m_State == STATE_START || m_State == STATE_INPUTS || m_State == STATE_BEFORE_ENDTICK || m_State == STATE_BEFORE_TICK, "invalid teehistorian state");

		if (m_HistorianMode == MODE_SQLITE)
		{


		} else if (m_HistorianMode == MODE_TEE_HISTORIAN) {

			CPacker Buffer;
			Buffer.Reset();
			Buffer.AddInt(-TEEHISTORIAN_FINISH);

			if (m_Debug)
			{
				dbg_msg("teehistorian", "finish");
			}

			Write(Buffer.Data(), Buffer.Size());
		}

		if (m_State == STATE_INPUTS)
		{
			EndInputs();
		}

		if (m_State == STATE_BEFORE_ENDTICK)
		{
			EndTick();
		}


	}

}




void CTeeHistorian::CreateDatabase(const char* filename) {
	sqlite_lock(&m_SqliteMutex);

	int err = sqlite_open(filename, &m_SqliteDB);

	if (err == SQLITE_OK) {

		CreatePlayerMovementTable();
		CreatePlayerInputTable();
		CreateRconActivityTable();
		CreatePlayerConnectedStateTable();

		BeginTransaction();
		OptimizeDatabase();

	} else {
		dbg_msg("SQLiteHistorian", "Error in CreateDatabase");
		m_HistorianMode = MODE_NONE;
	}
	sqlite_unlock(&m_SqliteMutex);
}

void CTeeHistorian::CreateRconActivityTable() {
	char* ErrMsg;
	int err = sqlite_exec(m_SqliteDB,
	                      "BEGIN; \
			CREATE TABLE IF NOT EXISTS RconActivity( \
				NickName VARCHAR(20), \
				TimeStamp VARCHAR(25), \
				Command VARCHAR(128), \
				Arguments TEXT\
			); \
			CREATE INDEX IF NOT EXISTS RconActivity_NickName_index ON RconActivity (NickName); \
			CREATE INDEX IF NOT EXISTS RconActivity_TimeStamp_index ON RconActivity (TimeStamp); \
			CREATE INDEX IF NOT EXISTS RconActivity_Command_index ON RconActivity (Command); \
			CREATE INDEX IF NOT EXISTS RconActivity_Arguments_index ON RconActivity (Arguments); \
			COMMIT;", &ErrMsg);

	/* check for error */
	if (err != SQLITE_OK) {
		dbg_msg("ERROR SQLITE", "CreateRconActivityTable:");
		dbg_msg("SQLiteHistorian", "SQL error (#%d): %s\n", err, ErrMsg);
		sqlite3_free(ErrMsg);
		m_HistorianMode = MODE_NONE;
	}

}

void CTeeHistorian::CreatePlayerMovementTable() {
	char* ErrMsg;
	int err = sqlite_exec(m_SqliteDB,
	                      "BEGIN; \
			CREATE TABLE IF NOT EXISTS PlayerMovement( \
				JoinHash INT, \
				TimeStamp VARCHAR(25), \
				Tick UNSIGNED BIG INT, \
				X INT, \
				Y INT, \
				OldX INT, \
				OldY INT\
			); \
			CREATE INDEX IF NOT EXISTS PlayerMovement_JoinHash_index ON PlayerMovement (JoinHash); \
			CREATE INDEX IF NOT EXISTS PlayerMovement_TimeStamp_index ON PlayerMovement (TimeStamp); \
			CREATE INDEX IF NOT EXISTS PlayerMovement_Tick_index ON PlayerMovement (Tick); \
			CREATE INDEX IF NOT EXISTS PlayerMovement_X_index ON PlayerMovement (X); \
			CREATE INDEX IF NOT EXISTS PlayerMovement_Y_index ON PlayerMovement (Y); \
			CREATE INDEX IF NOT EXISTS PlayerMovement_OldX_index ON PlayerMovement (OldX); \
			CREATE INDEX IF NOT EXISTS PlayerMovement_OldY_index ON PlayerMovement (OldY); \
			COMMIT;", &ErrMsg);

	/* check for error */
	if (err != SQLITE_OK) {
		dbg_msg("ERROR SQLITE", "CreatePlayerMovementTable:");
		dbg_msg("SQLiteHistorian", "SQL error (#%d): %s\n", err, ErrMsg);
		sqlite3_free(ErrMsg);
		m_HistorianMode = MODE_NONE;
	}

}
void CTeeHistorian::CreatePlayerInputTable() {
	char* ErrMsg;
	int err = sqlite_exec(m_SqliteDB,
	                      "BEGIN; \
			CREATE TABLE IF NOT EXISTS PlayerInput( \
				JoinHash INT, \
				TimeStamp VARCHAR(25), \
				Tick UNSIGNED BIG INT, \
				Direction TINYINT, \
				TargetX SMALLINT, \
				TargetY SMALLINT, \
				Jump TINYINT, \
				Fire SMALLINT, \
				Hook TINYINT, \
				PlayerFlags INT, \
				WantedWeapon TINYINT, \
				NextWeapon TINYINT, \
				PrevWeapon TINYINT \
			); \
			CREATE INDEX IF NOT EXISTS PlayerInput_JoinHash_index ON PlayerInput (JoinHash); \
			CREATE INDEX IF NOT EXISTS PlayerInput_TimeStamp_index ON PlayerInput (TimeStamp); \
			CREATE INDEX IF NOT EXISTS PlayerInput_Tick_index ON PlayerInput (Tick); \
			CREATE INDEX IF NOT EXISTS PlayerInput_Direction_index ON PlayerInput (Direction); \
			CREATE INDEX IF NOT EXISTS PlayerInput_TargetX_index ON PlayerInput (TargetX); \
			CREATE INDEX IF NOT EXISTS PlayerInput_TargetY_index ON PlayerInput (TargetY); \
			CREATE INDEX IF NOT EXISTS PlayerInput_Jump_index ON PlayerInput (Jump); \
			CREATE INDEX IF NOT EXISTS PlayerInput_Fire_index ON PlayerInput (Fire); \
			CREATE INDEX IF NOT EXISTS PlayerInput_Hook_index ON PlayerInput (Hook); \
			CREATE INDEX IF NOT EXISTS PlayerInput_PlayerFlags_index ON PlayerInput (PlayerFlags); \
			CREATE INDEX IF NOT EXISTS PlayerInput_WantedWeapon_index ON PlayerInput (WantedWeapon); \
			CREATE INDEX IF NOT EXISTS PlayerInput_NextWeapon_index ON PlayerInput (NextWeapon); \
			CREATE INDEX IF NOT EXISTS PlayerInput_PrevWeapon_index ON PlayerInput (PrevWeapon); \
			COMMIT;" , &ErrMsg);

	/* check for error */
	if (err != SQLITE_OK) {
		dbg_msg("ERROR SQLITE", "CreatePlayerInputTable:");
		dbg_msg("SQLiteHistorian", "SQL error (#%d): %s\n", err, ErrMsg);
		sqlite3_free(ErrMsg);
		m_HistorianMode = MODE_NONE;
	}

}


void CTeeHistorian::CreatePlayerConnectedStateTable() {
	char* ErrMsg;
	int err = sqlite_exec(m_SqliteDB ,
	                      "BEGIN; \
			CREATE TABLE IF NOT EXISTS PlayerConnectedState( \
				JoinHash INT, \
				NickName VARCHAR(20), \
				ClientID TINYINT, \
				TimeStamp VARCHAR(25), \
				Tick UNSIGNED BIG INT, \
				ConnectedState VARCHAR(64), \
				Reason VARCHAR(128) \
	                      ); \
	        CREATE INDEX IF NOT EXISTS PlayerConnectedState_JoinHash_index ON PlayerConnectedState (JoinHash); \
			CREATE INDEX IF NOT EXISTS PlayerConnectedState_NickName_index ON PlayerConnectedState (NickName); \
			CREATE INDEX IF NOT EXISTS PlayerConnectedState_ClientID_index ON PlayerConnectedState (ClientID); \
			CREATE INDEX IF NOT EXISTS PlayerConnectedState_TimeStamp_index ON PlayerConnectedState (TimeStamp); \
			CREATE INDEX IF NOT EXISTS PlayerConnectedState_Tick_index ON PlayerConnectedState (Tick); \
			CREATE INDEX IF NOT EXISTS PlayerConnectedState_ConnectedState_index ON PlayerConnectedState (ConnectedState); \
			CREATE INDEX IF NOT EXISTS PlayerConnectedState_Reason_index ON PlayerConnectedState (Reason); \
			COMMIT;" , &ErrMsg);

	/* check for error */
	if (err != SQLITE_OK) {
		dbg_msg("ERROR SQLITE", "CreatePlayerInputTable:");
		dbg_msg("SQLiteHistorian", "SQL error (#%d): %s\n", err, ErrMsg);
		sqlite3_free(ErrMsg);
		m_HistorianMode = MODE_NONE;
	}
}


void CTeeHistorian::InsertIntoRconActivityTable(char* NickName, char *TimeStamp, char *Command, char *Arguments) {
	/* prepare */
	const char *zTail;
	const char *zSql = "\
		INSERT OR REPLACE INTO RconActivity (NickName, TimeStamp, Command, Arguments) \
		VALUES ( trim(?1), ?2, ?3, ?4) \
		;";
	sqlite3_stmt *pStmt;
	int rc = sqlite_prepare_statement(m_SqliteDB, zSql, &pStmt, &zTail);

	if (rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite_bind_text(pStmt, 1, NickName);
		sqlite_bind_text(pStmt, 2, TimeStamp);
		sqlite_bind_text(pStmt, 3, Command);
		sqlite_bind_text(pStmt, 4, Arguments);

		/* lock database access in this process */
		sqlite_lock(&m_SqliteMutex);
		if (!m_SqliteDB) {
			dbg_msg("SQLiteHistorian", "SQL database does not exist anymore.");
			sqlite_unlock(&m_SqliteMutex);
			free(NickName);
			free(TimeStamp);
			free(Command);
			free(Arguments);
			sqlite_finalize(pStmt);
			return;
		}

		/* when another process uses the database, wait up to 1 minute */
		sqlite_busy_timeout(m_SqliteDB, 60000);

		/* save to database */
		switch (sqlite_step(pStmt))
		{
		case SQLITE_DONE:
			/* nothing */
			break;
		case SQLITE_BUSY:
			dbg_msg("SQLiteHistorian", "Error: could not save records (timeout).");
			break;
		default:
			dbg_msg("SQLiteHistorian", "SQL error (#%d): %s", rc, sqlite_errmsg(m_SqliteDB));
		}

		/* unlock database access */
		sqlite_unlock(&m_SqliteMutex);
	}
	else
	{
		dbg_msg("SQLiteHistorian", "SQL error (#%d): %s", rc, sqlite_errmsg(m_SqliteDB));
	}

	free(NickName);
	free(TimeStamp);
	free(Command);
	free(Arguments);
	sqlite_finalize(pStmt);
}

void CTeeHistorian::InsertIntoPlayerMovementTable(int ClientJoinHash, char *TimeStamp, int Tick, int x, int y, int old_x, int old_y) {

	/* prepare */
	const char *zTail;
	const char *zSql = "\
		INSERT OR REPLACE INTO PlayerMovement (JoinHash, TimeStamp, Tick, X, Y, OldX, OldY) \
		VALUES ( ?1, trim(?2), ?3, ?4, ?5, ?6, ?7 ) \
		;";
	sqlite3_stmt *pStmt;
	int rc = sqlite_prepare_statement(m_SqliteDB, zSql, &pStmt, &zTail);

	if (rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite_bind_int(pStmt, 1, ClientJoinHash);
		sqlite_bind_text(pStmt, 2, TimeStamp);
		sqlite_bind_int(pStmt, 3, Tick);
		sqlite_bind_int(pStmt, 4, x);
		sqlite_bind_int(pStmt, 5, y);
		sqlite_bind_int(pStmt, 6, old_x);
		sqlite_bind_int(pStmt, 7, old_y);
		/* lock database access in this process */
		sqlite_lock(&m_SqliteMutex);
		if (!m_SqliteDB) {
			dbg_msg("SQLiteHistorian", "SQL database does not exist anymore.");
			sqlite_unlock(&m_SqliteMutex);
			free(TimeStamp);
			sqlite_finalize(pStmt);
			return;
		}


		/* when another process uses the database, wait up to 1 minute */
		sqlite_busy_timeout(m_SqliteDB, 60000);

		/* save to database */
		switch (sqlite_step(pStmt))
		{
		case SQLITE_DONE:
			/* nothing */
			break;
		case SQLITE_BUSY:
			dbg_msg("SQLiteHistorian", "Error: could not save records (timeout).");
			break;
		default:
			dbg_msg("SQLiteHistorian", "SQL error (#%d): %s", rc, sqlite_errmsg(m_SqliteDB));
		}

		/* unlock database access */
		sqlite_unlock(&m_SqliteMutex);
	}
	else
	{
		dbg_msg("SQLiteHistorian", "SQL error (#%d): %s", rc, sqlite_errmsg(m_SqliteDB));
	}

	free(TimeStamp);
	sqlite_finalize(pStmt);
}
void CTeeHistorian::InsertIntoPlayerInputTable(int ClientJoinHash, char *TimeStamp, int Tick, int Direction, int TargetX, int TargetY, int Jump, int Fire, int Hook, int PlayerFlags, int WantedWeapon, int NextWeapon, int PrevWeapon) {
	/* prepare */
	const char *zTail;
	const char *zSql = "\
		INSERT OR REPLACE INTO PlayerInput (JoinHash, TimeStamp, Tick, Direction, TargetX, TargetY, Jump, Fire, Hook, PlayerFlags, WantedWeapon, NextWeapon, PrevWeapon) \
		VALUES ( ?1, trim(?2), ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13 ) \
		;";
	sqlite3_stmt *pStmt;
	int rc = sqlite_prepare_statement(m_SqliteDB, zSql, &pStmt, &zTail);

	if (rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite_bind_int(pStmt, 1, ClientJoinHash);
		sqlite_bind_text(pStmt, 2, TimeStamp);
		sqlite_bind_int(pStmt, 3, Tick);
		sqlite_bind_int(pStmt, 4, Direction);
		sqlite_bind_int(pStmt, 5, TargetX);
		sqlite_bind_int(pStmt, 6, TargetY);
		sqlite_bind_int(pStmt, 7, Jump);
		sqlite_bind_int(pStmt, 8, Fire);
		sqlite_bind_int(pStmt, 9, Hook);
		sqlite_bind_int(pStmt, 10, PlayerFlags);
		sqlite_bind_int(pStmt, 11, WantedWeapon);
		sqlite_bind_int(pStmt, 12, NextWeapon);
		sqlite_bind_int(pStmt, 13, PrevWeapon);

		/* lock database access in this process */
		sqlite_lock(&m_SqliteMutex);
		if (!m_SqliteDB) {
			dbg_msg("SQLiteHistorian", "SQL database doe snot exist anymore.");
			sqlite_unlock(&m_SqliteMutex);
			free(TimeStamp);
			sqlite_finalize(pStmt);
			return;
		}

		/* when another process uses the database, wait up to 1 minute */
		sqlite_busy_timeout(m_SqliteDB, 60000);

		/* save to database */
		switch (sqlite_step(pStmt))
		{
		case SQLITE_DONE:
			/* nothing */
			break;
		case SQLITE_BUSY:
			dbg_msg("SQLiteHistorian", "Error: could not save records (timeout).");
			break;
		default:
			dbg_msg("SQLiteHistorian", "SQL error (#%d): %s", rc, sqlite_errmsg(m_SqliteDB));
		}

		/* unlock database access */
		sqlite_unlock(&m_SqliteMutex);
	}
	else
	{
		dbg_msg("SQLiteHistorian", "SQL error (#%d): %s", rc, sqlite_errmsg(m_SqliteDB));
	}


	free(TimeStamp);
	sqlite_finalize(pStmt);
}


void CTeeHistorian::InsertIntoPlayerConnectedStateTable(int ClientJoinHash, char  *NickName, int ClientID, char *TimeStamp, int Tick, bool ConnectedState, char *Reason) {
	/* prepare */
	const char *zTail;
	const char *zSql = "\
		INSERT OR REPLACE INTO PlayerConnectedState (JoinHash, NickName, ClientID, TimeStamp, Tick, ConnectedState, Reason) \
		VALUES ( ?1, trim(?2), ?3, ?4, ?5, ?6, ?7) \
		;";
	sqlite3_stmt *pStmt;
	int rc = sqlite_prepare_statement(m_SqliteDB, zSql, &pStmt, &zTail);

	if (rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite_bind_int(pStmt, 1, ClientJoinHash);
		sqlite_bind_text(pStmt, 2, NickName);
		sqlite_bind_int(pStmt, 3, ClientID);
		sqlite_bind_text(pStmt, 4, TimeStamp);
		sqlite_bind_int(pStmt, 5, Tick);
		sqlite_bind_text(pStmt, 6, ConnectedState ? "joined" : "left");
		sqlite_bind_text(pStmt, 7, Reason);

		/* lock database access in this process */
		sqlite_lock(&m_SqliteMutex);
		if (!m_SqliteDB) {
			dbg_msg("SQLiteHistorian", "SQL database does not exist anymore.");
			sqlite_unlock(&m_SqliteMutex);
			free(NickName);
			free(TimeStamp);
			free(Reason);
			sqlite_finalize(pStmt);
			return;
		}

		/* when another process uses the database, wait up to 1 minute */
		sqlite_busy_timeout(m_SqliteDB, 60000);

		/* save to database */
		switch (sqlite_step(pStmt))
		{
		case SQLITE_DONE:
			/* nothing */
			break;
		case SQLITE_BUSY:
			dbg_msg("SQLiteHistorian", "Error: could not save records (timeout).");
			break;
		default:
			dbg_msg("SQLiteHistorian", "SQL error (#%d): %s", rc, sqlite_errmsg(m_SqliteDB));
		}

		/* unlock database access */
		sqlite_unlock(&m_SqliteMutex);
	}
	else
	{
		dbg_msg("SQLiteHistorian", "SQL error (#%d): %s", rc, sqlite_errmsg(m_SqliteDB));
	}

	free(NickName);
	free(TimeStamp);
	free(Reason);
	sqlite_finalize(pStmt);

}

void CTeeHistorian::CloseDatabase() {
	//performance test

	sqlite_lock(&m_SqliteMutex);
	
	EndTransaction();

	int err = sqlite_close(m_SqliteDB);
	if (err != SQLITE_OK) {
		dbg_msg("SQLiteHistorian", "Error on closing sqlite database(%li).(%d)", (long)m_SqliteDB ,err);
	}

	sqlite_unlock(&m_SqliteMutex);
}

char* CTeeHistorian::GetTimeStamp() {
	char aDate[20];

	timeval curTime;
	gettimeofday(&curTime, NULL);
	long milli = curTime.tv_usec / 1000;
	strftime(aDate, 20, "%Y-%m-%d %H:%M:%S", localtime(&curTime.tv_sec));
	char *aBuf = (char*)malloc(25 * sizeof(char));
	str_format(aBuf, 25 * sizeof(char), "%s.%ld", aDate, milli);
	return aBuf;
}
void CTeeHistorian::OptimizeDatabase() {
	if (m_SqliteDB) {
		char* ErrMsg = 0;
		sqlite_lock(&m_SqliteMutex, 1000);
		sqlite_busy_timeout(m_SqliteDB, 3000);
		switch (g_Config.m_SvSqlitePerformance)
		{
		case 1: sqlite3_exec(m_SqliteDB, "PRAGMA synchronous = OFF", NULL, NULL, &ErrMsg); break;
		case 2: sqlite3_exec(m_SqliteDB, "PRAGMA journal_mode = MEMORY", NULL, NULL, &ErrMsg); break;
		case 3: sqlite3_exec(m_SqliteDB, "PRAGMA synchronous = OFF", NULL, NULL, &ErrMsg);
			sqlite3_exec(m_SqliteDB, "PRAGMA journal_mode = MEMORY", NULL, NULL, &ErrMsg);
			break;
		default: break;
			/* code */
		}
		sqlite_unlock(&m_SqliteMutex);
		sqlite3_free(ErrMsg);
	}
}

/**
 * @brief Non Locking
 * @details [long description]
 */
void CTeeHistorian::BeginTransaction() {
	if (m_SqliteDB) {
		char* ErrMsg;

		sqlite_busy_timeout(m_SqliteDB, 3000);
		sqlite_exec(m_SqliteDB, "BEGIN TRANSACTION", &ErrMsg);

		sqlite3_free(ErrMsg);
	}

}

/**
 * @brief pushes previous transactions to the database.
 * @details [long description]
 */
void CTeeHistorian::EndTransaction() {
	if (m_SqliteDB) {
		char* ErrMsg;
		//sqlite_lock(&m_SqliteMutex, 1000);
		sqlite_busy_timeout(m_SqliteDB, 3000);
		sqlite_exec(m_SqliteDB, "END TRANSACTION", &ErrMsg);
		//sqlite_unlock(&m_SqliteMutex);

		if (ErrMsg) {
			dbg_msg("SQLiteHistorian", "SQL error: %s", sqlite_errmsg(m_SqliteDB));
		}
		sqlite3_free(ErrMsg);
	}
}

void CTeeHistorian::MiddleTransaction() {
	if (m_SqliteDB) {
		char* ErrMsg;
		/**
		 * lock mutex - this needs to be locked for
		 * both transactions in order to have a consistent database.
		 */
		sqlite_lock(&m_SqliteMutex, 1500);
		sqlite_busy_timeout(m_SqliteDB, 2500);

		/*execute writing previous transactions to database*/
		sqlite_exec(m_SqliteDB, "END TRANSACTION", &ErrMsg);
		if (ErrMsg) {
			dbg_msg("SQLiteHistorian", "SQL error: %s", sqlite_errmsg(m_SqliteDB));
		}
		sqlite3_free(ErrMsg);

		/*Begin next transaction before tick treshold is reached*/
		sqlite_exec(m_SqliteDB, "BEGIN TRANSACTION", &ErrMsg);
		sqlite_unlock(&m_SqliteMutex);
		if (ErrMsg) {
			dbg_msg("SQLiteHistorian", "SQL error: %s", sqlite_errmsg(m_SqliteDB));
		}
		sqlite3_free(ErrMsg);
	}

}






