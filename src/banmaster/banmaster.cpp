/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <cstdlib> /* atexit() */

#include <engine/console.h>
#include <engine/storage.h>
#include <engine/config.h>

#include <engine/shared/config.h>
#include <engine/shared/netban.h>
#include <engine/shared/network.h>
#include <engine/shared/econ.h>
#include <engine/shared/packer.h>

#include "banmaster.h"

enum 
{
	BAN_REREAD_TIME=300,
	MAX_BAN_ENTRIES = 10,
	MAX_REASON_LENGTH = 128,
};

static const char BANMASTER_BANFILE[] = "bans.cfg";
static const char BANMASTER_BANSSAVEFILE[] = "saved_bans.cfg";

class CBanmasterBan : public CNetBan
{
public:
	//returns true if address is banned

	bool GetBanInfo(const NETADDR *pAddr, char *pReason, int BufferSize, int *pExpires)
	{
		CNetHash aHash[17];
		int Length = CNetHash::MakeHashArray(pAddr, aHash);

		// check ban adresses
		CBanAddr *pBan = m_BanAddrPool.Find(pAddr, &aHash[Length]);
		if(pBan)
		{
			str_format(pReason, BufferSize, "%s", pBan->m_Info.m_aReason);
			*pExpires = pBan->m_Info.m_Expires;
			return true;
		}
		// check ban ranges
		for(int i = Length-1; i >= 0; --i)
		{
			for(CBanRange *pBan = m_BanRangePool.First(&aHash[i]); pBan; pBan = pBan->m_pHashNext)
			{
				if(NetMatch(&pBan->m_Data, pAddr, i, Length))
				{
					str_format(pReason, BufferSize, "%s", pBan->m_Info.m_aReason);
					*pExpires = pBan->m_Info.m_Expires;
					return true;
				}
			}
		}

		return false;
	}
};

struct CBanInfo {
	char m_aName[16];
	char m_aReason[MAX_REASON_LENGTH];
	NETADDR m_Addr;
	int m_RecvTime;
};

struct CStats {
	unsigned int m_StartTime;
	int m_HitsBannedPlayers;
	int m_NumRecvBans;
	int m_NumRequests;
};

CEcon m_Econ;
CNetClient m_Net;
CBanmasterBan m_NetBan;
CBanInfo m_RecvBans[MAX_BAN_ENTRIES];
CStats m_Stats;

IConsole *m_pConsole;
IStorage *m_pStorage;
IKernel *m_pKernel;
IConfig *m_pConfig;

void CleanUp()
{
	// Better: RAII?
	m_Econ.Shutdown();

	delete m_pConsole;
	delete m_pConfig;
	delete m_pKernel;
	delete m_pStorage;
}

int SendResponse(NETADDR *pAddr, NETADDR *pCheck)
{
	static char aIpBan[sizeof(BANMASTER_IPBAN) + NETADDR_MAXSTRSIZE] = { 0 };
	static char *pIpBanContent = aIpBan + sizeof(BANMASTER_IPBAN);

	if (!aIpBan[0])
		mem_copy(aIpBan, BANMASTER_IPBAN, sizeof(BANMASTER_IPBAN));

	static CNetChunk p;

	p.m_ClientID = -1;
	p.m_Address = *pAddr;
	p.m_Flags = NETSENDFLAG_CONNLESS;

	int Expires;
	char aReason[128];

	if(m_NetBan.GetBanInfo(pCheck, aReason, sizeof(aReason), &Expires))
	{
		net_addr_str(pCheck, pIpBanContent, NETADDR_MAXSTRSIZE, false);
		char *pIpBanReason = pIpBanContent + (str_length(pIpBanContent) + 1);
		str_copy(pIpBanReason, aReason, 256);
		
		p.m_pData = aIpBan;
		p.m_DataSize = sizeof(BANMASTER_IPBAN) + str_length(pIpBanContent) + 1 + str_length(pIpBanReason) + 1;
		m_Net.Send(&p);
		return 1;
	}

	return 0;
}

void AddRecvBan(NETADDR *pFromAddr, unsigned char *pData, int Size)
{
	CUnpacker Up;
	const char *pIP, *pName, *pReason;

	Up.Reset(pData + sizeof(BANMASTER_IPREPORT), Size - sizeof(BANMASTER_IPREPORT));

	pName = Up.GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
	pIP = Up.GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
	pReason = Up.GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);

	NETADDR ReportAddr;
	if(net_addr_from_str(&ReportAddr, pIP) == 0 && pName[0] && pReason[0])
	{
		for(int i = MAX_BAN_ENTRIES-1; i > 0; i--)
			mem_copy(&m_RecvBans[i], &m_RecvBans[i-1], sizeof(m_RecvBans[i]));

		m_RecvBans[0].m_Addr = ReportAddr;
		str_copy(m_RecvBans[0].m_aName, pName, sizeof(m_RecvBans[0].m_aName));
		str_copy(m_RecvBans[0].m_aReason, pReason, sizeof(m_RecvBans[0].m_aReason));
		m_RecvBans[0].m_RecvTime = time_timestamp();

		char aAddr[NETADDR_MAXSTRSIZE];
		char aBuf[256];
		net_addr_str(pFromAddr, aAddr, sizeof(aAddr), false);
		str_format(aBuf, sizeof(aBuf), "Received ban from '%s', Name: %s, IP: %s, Reason: %s", aAddr, pName, pIP, pReason);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "banmaster", aBuf);
		m_Stats.m_NumRecvBans++;
	}
	else
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "banmaster", "dropped weird ban message");
}

void ReloadBans()
{
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "bans_save %s", BANMASTER_BANSSAVEFILE);
	m_pConsole->ExecuteLine(aBuf);
}

void ConRecvBans(IConsole::IResult *pResult, void *pUser)
{
	int Count = 0;
	char aBuf[256];
	for(int i = MAX_BAN_ENTRIES-1; i >= 0; i--)
	{
		if(!m_RecvBans[i].m_aName[0])
			continue;

		char aIP[NETADDR_MAXSTRSIZE];
		net_addr_str(&m_RecvBans[i].m_Addr, aIP, sizeof(aIP), false);
		int RecvTime = time_timestamp() - m_RecvBans[i].m_RecvTime;
		str_format(aBuf, sizeof(aBuf), "#%d - Name: %s | Reason: %s | IP: %s | Received %d min and %d sec. ago", i, m_RecvBans[i].m_aName, m_RecvBans[i].m_aReason, aIP, RecvTime/60, RecvTime%60);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "banmaster", aBuf);
		Count++;
	}
	str_format(aBuf, sizeof(aBuf), "%d Ban(s)", Count);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "banmaster", aBuf);
}

void ConAddBan(IConsole::IResult *pResult, void *pUser)
{
	int Index = pResult->GetInteger(0);
	if(Index >= 0 && Index < MAX_BAN_ENTRIES && m_RecvBans[Index].m_aName[0])
	{
		m_NetBan.BanAddr(&m_RecvBans[Index].m_Addr, pResult->GetInteger(1)*60, pResult->GetString(2));
	}
	else
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "banmaster", "Invalid index");
}

void ConStatus(IConsole::IResult *pResult, void *pUser)
{
	unsigned int Running = time_timestamp() - m_Stats.m_StartTime;
	int Hours = Running / 3600;
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Running since %d hours %d min and %d sec. Total received requests: %d (%d hits). %d received bans (%.3f/hour).",
			Hours, (Running-Hours*3600) / 60, Running % 60, m_Stats.m_NumRequests, m_Stats.m_HitsBannedPlayers, m_Stats.m_NumRecvBans, (Hours != 0) ? (float)m_Stats.m_NumRecvBans/Hours : m_Stats.m_NumRecvBans);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "banmaster", aBuf);
}

int main(int argc, const char **argv) // ignore_convention
{
	atexit(CleanUp);
	int64 LastUpdate = time_get();

	dbg_logger_stdout();
	net_init();

	m_pKernel = IKernel::Create();
	m_pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_BASIC, argc, argv); // ignore_convention
	m_pConfig = CreateConfig();

	m_pConsole = CreateConsole(CFGFLAG_BANMASTER|CFGFLAG_ECON);

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !m_pKernel->RegisterInterface(m_pConsole);
		RegisterFail = RegisterFail || !m_pKernel->RegisterInterface(m_pStorage);
		RegisterFail = RegisterFail || !m_pKernel->RegisterInterface(m_pConfig);
		
		if(RegisterFail)
			return -1;
	}

	//Reset strings
	m_pConfig->Init();

	// Register Commands
	m_pConsole->Register("recvbans", "", CFGFLAG_BANMASTER, ConRecvBans, 0, "Show the last received bans");
	m_pConsole->Register("addban", "iir", CFGFLAG_BANMASTER, ConAddBan, 0, "Ban IP by index");
	m_pConsole->Register("status", "", CFGFLAG_BANMASTER, ConStatus, 0, "Show some statistics");


	m_NetBan.Init(m_pConsole, m_pStorage);
	m_pConsole->ExecuteFile(BANMASTER_BANFILE);
	m_Econ.Init(m_pConsole, &m_NetBan);

	m_pConsole->ExecuteFile(BANMASTER_BANSSAVEFILE);

	m_pConsole->StoreCommands(false);

	mem_zero(&m_RecvBans, sizeof(CBanInfo)*MAX_BAN_ENTRIES);
	mem_zero(&m_Stats, sizeof(CStats));
	m_Stats.m_StartTime = time_timestamp();

	NETADDR BindAddr;
	if(g_Config.m_Bindaddr[0] && net_host_lookup(g_Config.m_Bindaddr, &BindAddr, NETTYPE_ALL) == 0)
		BindAddr.port = BANMASTER_PORT;
	else
	{
		mem_zero(&BindAddr, sizeof(BindAddr));
		BindAddr.type = NETTYPE_ALL;
		BindAddr.port = BANMASTER_PORT;
	}

	if(!m_Net.Open(BindAddr, 0))
	{
		dbg_msg("banmaster", "couldn't start network");
		return -1;
	}

	dbg_msg("banmaster", "started");
	
	while(1)
	{
		m_Net.Update();

		// process m_Packets
		CNetChunk Packet;
		while(m_Net.Recv(&Packet))
		{
			char aAddressStr[NETADDR_MAXSTRSIZE];
			net_addr_str(&Packet.m_Address, aAddressStr, sizeof(aAddressStr), false);

			if(Packet.m_DataSize >= (int)sizeof(BANMASTER_IPCHECK) && mem_comp(Packet.m_pData, BANMASTER_IPCHECK, sizeof(BANMASTER_IPCHECK)) == 0)
			{
				char *pAddr = (char *)Packet.m_pData + sizeof(BANMASTER_IPCHECK);
				NETADDR CheckAddr;
				if(net_addr_from_str(&CheckAddr, pAddr))
				{
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "dropped weird message, ip='%s' checkaddr='%s'", aAddressStr, pAddr);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "banmaster", aBuf);
				}
				else
				{
					CheckAddr.port = 0;

					int Banned = SendResponse(&Packet.m_Address, &CheckAddr);

					char aIP[NETADDR_MAXSTRSIZE];
					char aBuf[256];
					net_addr_str(&CheckAddr, aIP, sizeof(aIP), false);
					str_format(aBuf, sizeof(aBuf), "responded to checkmsg, ip='%s' checkaddr='%s' result=%s", aAddressStr, aIP, (Banned) ? "ban" : "ok");
					if(Banned)
					{
						m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "banmaster", aBuf);
						m_Stats.m_HitsBannedPlayers++;
					}
					else
						m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "banmaster", aBuf);

					m_Stats.m_NumRequests++;
				}
			}
			else if(Packet.m_DataSize >= (int)sizeof(BANMASTER_IPREPORT) && mem_comp(Packet.m_pData, BANMASTER_IPREPORT, sizeof(BANMASTER_IPREPORT)) == 0)
			{
				//info of a banned client
				AddRecvBan(&Packet.m_Address, (unsigned char*)Packet.m_pData, Packet.m_DataSize);
			}
			else
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "dropped weird packet, ip='%s'", aAddressStr, (char *)Packet.m_pData);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "banmaster", aBuf);
			}
		}

		if(time_get() - LastUpdate > time_freq() * BAN_REREAD_TIME)
		{
			ReloadBans();
			LastUpdate = time_get();
		}

		m_NetBan.Update();
		m_Econ.Update();
		
		// be nice to the CPU
		thread_sleep(1);
	}

	return 0;
}
