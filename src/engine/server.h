/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* Modified by Teelevision for zCatch/TeeVi, see readme.txt and license.txt.                 */
#ifndef ENGINE_SERVER_H
#define ENGINE_SERVER_H
#include "kernel.h"
#include "message.h"

#include <string>

class IServer : public IInterface
{
	MACRO_INTERFACE("server", 0)
protected:
	int m_CurrentGameTick;
	int m_TickSpeed;

public:
	/*
		Structure: CClientInfo
	*/
	struct CClientInfo
	{
		const char *m_pName;
		int m_Latency;
	};



	int Tick() const { return m_CurrentGameTick; }
	int TickSpeed() const { return m_TickSpeed; }

	virtual int MaxClients() const = 0;
	virtual const char *ClientName(int ClientID) = 0;
	virtual const char *ClientClan(int ClientID) = 0;
	virtual int ClientCountry(int ClientID) = 0;
	virtual bool ClientIngame(int ClientID) = 0;
	virtual int GetClientInfo(int ClientID, CClientInfo *pInfo) = 0;
	virtual void GetClientAddr(int ClientID, char *pAddrStr, int Size, bool Port = false) = 0;
	virtual int ClientVotebannedTime(int ClientID) = 0;
	virtual int ClientJoinHash(int ClientID) = 0;

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID) = 0;

	template<class T>
	int SendPackMsg(T *pMsg, int Flags, int ClientID)
	{
		CMsgPacker Packer(pMsg->MsgID());
		if (pMsg->Pack(&Packer))
			return -1;
		return SendMsg(&Packer, Flags, ClientID);
	}

	virtual void SetClientName(int ClientID, char const *pName) = 0;
	virtual void SetClientClan(int ClientID, char const *pClan) = 0;
	virtual void SetClientCountry(int ClientID, int Country) = 0;
	virtual void SetClientScore(int ClientID, int Score) = 0;
	virtual void SetClientJoinHash(int ClientID, int ClientJoinHash) = 0;

	virtual const char* GetAuthName(int ClientID) = 0;
	virtual int GetAuthLevel(int ClientID) = 0;

	/*teehistorian*/
	virtual void GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, int *pMapCrc) = 0;
	/*teehistorian end*/

	virtual int SnapNewID() = 0;
	virtual void SnapFreeID(int ID) = 0;
	virtual void *SnapNewItem(int Type, int ID, int Size) = 0;

	virtual void SnapSetStaticsize(int ItemType, int Size) = 0;

	enum
	{
		RCON_CID_SERV = -1,
		RCON_CID_VOTE = -2,
	};
	virtual void SetRconCID(int ClientID) = 0;
	virtual bool IsAuthed(int ClientID) = 0;
	virtual void Kick(int ClientID, const char *pReason) = 0;

	/*teehistorian*/
	virtual void SetErrorShutdown(const char *pReason) = 0;

	virtual void DemoRecorder_HandleAutoStart() = 0;
	virtual bool DemoRecorder_IsRecording() = 0;
	//zCatch
	virtual void MapReload() = 0;

	virtual int GetInfoTextIntervalPause() = 0;
	virtual int GetInfoTextMsgInterval() = 0;
	virtual int GetInfoTextInterval() = 0;
	virtual std::string GetNextInfoText() = 0;

	virtual int GetNumLoggedInAdmins() = 0;
	virtual class CServerBan *GetBanServer() = 0;
};

class IGameServer : public IInterface
{
	MACRO_INTERFACE("gameserver", 0)
protected:
public:

	virtual void OnInit() = 0;
	virtual void OnConsoleInit() = 0;
	virtual void OnShutdown() = 0;

	virtual void OnTick() = 0;
	virtual void OnPreSnap() = 0;
	virtual void OnSnap(int ClientID) = 0;
	virtual void OnPostSnap() = 0;

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) = 0;

	virtual void OnClientConnected(int ClientID) = 0;
	virtual void OnClientEnter(int ClientID) = 0;
	virtual void OnClientDrop(int ClientID, const char *pReason) = 0;
	virtual void OnClientDirectInput(const char* ClientName, int ClientID, void *pInput) = 0;
	virtual void OnClientPredictedInput(int ClientID, void *pInput) = 0;

	virtual bool IsClientReady(int ClientID) = 0;
	virtual bool IsClientPlayer(int ClientID) = 0;

	virtual const char *GameType() = 0;
	virtual const char *Version() = 0;
	virtual const char *NetVersion() = 0;

	virtual void OnSetAuthed(int ClientID, int Level) = 0;

	// old bot detection
	/*
	virtual bool IsClientAimBot(int ClientID) = 0;
	*/
	virtual void InformPlayers(const char *pText) = 0;

	/*teehistorian*/
	virtual void OnClientEngineJoin(int ClientID) = 0;
	virtual void OnClientEngineDrop(int ClientID, const char *pReason) = 0;
};

extern IGameServer *CreateGameServer();
#endif
