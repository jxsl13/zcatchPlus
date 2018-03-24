#include "botdetection.h"
#include <cstdlib>
#include <limits>







CBotDetection::CBotDetection(CGameContext *GameServer)
{
	m_GameContext = GameServer;
}

CBotDetection::~CBotDetection() {

}

void CBotDetection::OnTick() {
	//m_aPlayersCurrentTick needs to have some proper values first
	ManageLastInputQueue();

	// then calculate stuff
	CalculateDistanceToEveryPlayer();



	// at last reset current tick
	ResetCurrentTick();
}

void CBotDetection::ManageLastInputQueue() {
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{

		if (m_GameContext->m_apPlayers[i] &&  m_GameContext->m_apPlayers[i]->GetCharacter())
		{

			// has input and core
			if (m_aPlayersCurrentTick[i].m_InputAvailable && m_aPlayersCurrentTick[i].m_CoreAvailable)
			{
				// push in all three queues
				m_aPlayerBacklog[i].push(m_aPlayersCurrentTick[i]);
				m_aPlayerInputBacklog[i].push(m_aPlayersCurrentTick[i]);
				m_aPlayerCoreBacklog[i].push(m_aPlayersCurrentTick[i]);

			} else if (m_aPlayersCurrentTick[i].m_CoreAvailable && !m_aPlayersCurrentTick[i].m_InputAvailable)
			{
				// only core available, input not available
				if (m_aPlayerInputBacklog[i].size() > 0)
				{
					// we have previous input in out input backlog
					TickPlayer temp = m_aPlayerInputBacklog[i].front();
					m_aPlayerInputBacklog[i].pop();
					// no input, thus set previous input to current input
					SetInput(&m_aPlayersCurrentTick[i], &temp);

					// push combined current tick into all three queues
					m_aPlayerBacklog[i].push(m_aPlayersCurrentTick[i]);
					m_aPlayerInputBacklog[i].push(m_aPlayersCurrentTick[i]);
					m_aPlayerCoreBacklog[i].push(m_aPlayersCurrentTick[i]);
				} else {
					// no previous input available, put only the core into the core queue
					m_aPlayerCoreBacklog[i].push(m_aPlayersCurrentTick[i]);
				}
			} else if (m_aPlayersCurrentTick[i].m_InputAvailable && !m_aPlayersCurrentTick[i].m_CoreAvailable)
			{
				// input available, but no core available
				// we have previous core data in the core queue
				if (m_aPlayerCoreBacklog[i].size() > 0)
				{
					// set current tick player's core to previous core.
					TickPlayer temp = m_aPlayerCoreBacklog[i].front();
					m_aPlayerCoreBacklog[i].pop();

					SetCore(&m_aPlayersCurrentTick[i], &temp);

					// push complete tick player into all queues
					m_aPlayerBacklog[i].push(m_aPlayersCurrentTick[i]);
					m_aPlayerInputBacklog[i].push(m_aPlayersCurrentTick[i]);
					m_aPlayerCoreBacklog[i].push(m_aPlayersCurrentTick[i]);
				} else {
					// no previous core data available, but we get new input: push into input queue.
					m_aPlayerInputBacklog[i].push(m_aPlayersCurrentTick[i]);
				}
			} else if (!m_aPlayersCurrentTick[i].m_InputAvailable && !m_aPlayersCurrentTick[i].m_CoreAvailable)
			{
				// no input available for both, core and input
				if (m_aPlayerBacklog[i].size() > 0)
				{
					// we have some data from a previous tick for both, the input and core
					m_aPlayersCurrentTick[i] = m_aPlayerBacklog[i].front();
				} else if (m_aPlayerCoreBacklog[i].size() > 0 && m_aPlayerInputBacklog[i].size() > 0)
				{
					// we don't have both, but both as individuals
					SetCore(&m_aPlayersCurrentTick[i], &m_aPlayerCoreBacklog[i].front());
					SetInput(&m_aPlayersCurrentTick[i], &m_aPlayerInputBacklog[i].front());
					m_aPlayerCoreBacklog[i].pop();
					m_aPlayerInputBacklog[i].pop();
					// merge both into one and push it back to all three
					m_aPlayerCoreBacklog[i].push(m_aPlayersCurrentTick[i]);
					m_aPlayerInputBacklog[i].push(m_aPlayersCurrentTick[i]);
					m_aPlayerBacklog[i].push(m_aPlayersCurrentTick[i]);
				}

			}

			// Reduce queue to last input only
			while (m_aPlayerBacklog[i].size() > 1) {
				m_aPlayerBacklog[i].pop();
			}
			while (m_aPlayerInputBacklog[i].size() > 1) {
				m_aPlayerInputBacklog[i].pop();
			}
			while (m_aPlayerCoreBacklog[i].size() > 1) {
				m_aPlayerCoreBacklog[i].pop();
			}

		}
	}
}



void CBotDetection::AddPlayerCore(int ClientID, int JoinHash, CNetObj_CharacterCore *PlayerCore) {
	if (m_GameContext->m_apPlayers[ClientID])
	{


		m_aPlayersCurrentTick[ClientID].m_CoreAvailable = true;
		m_aPlayersCurrentTick[ClientID].m_JoinHash = JoinHash;
		m_aPlayersCurrentTick[ClientID].m_ClientID = ClientID;
		m_aPlayersCurrentTick[ClientID].m_Core_Tick = PlayerCore->m_Tick;
		m_aPlayersCurrentTick[ClientID].m_Core_X = PlayerCore->m_X;
		m_aPlayersCurrentTick[ClientID].m_Core_Y = PlayerCore->m_Y;
		m_aPlayersCurrentTick[ClientID].m_Core_VelX = PlayerCore->m_VelX;
		m_aPlayersCurrentTick[ClientID].m_Core_VelY = PlayerCore->m_VelY;
		m_aPlayersCurrentTick[ClientID].m_Core_Angle = PlayerCore->m_Angle;
		m_aPlayersCurrentTick[ClientID].m_Core_Direction = PlayerCore->m_Direction;
		m_aPlayersCurrentTick[ClientID].m_Core_Jumped = PlayerCore->m_Jumped;
		m_aPlayersCurrentTick[ClientID].m_Core_HookedPlayer = PlayerCore->m_HookedPlayer;
		m_aPlayersCurrentTick[ClientID].m_Core_HookState = PlayerCore->m_HookState;
		m_aPlayersCurrentTick[ClientID].m_Core_HookTick = PlayerCore->m_HookTick;
		m_aPlayersCurrentTick[ClientID].m_Core_HookX = PlayerCore->m_HookX;
		m_aPlayersCurrentTick[ClientID].m_Core_HookY = PlayerCore->m_HookY;
		m_aPlayersCurrentTick[ClientID].m_Core_HookDx = PlayerCore->m_HookDx;
		m_aPlayersCurrentTick[ClientID].m_Core_HookDy = PlayerCore->m_HookDy;

	}

}

void CBotDetection::AddPlayerInput(int ClientID, int JoinHash, CNetObj_PlayerInput *PlayerInput) {
	if (m_GameContext->m_apPlayers[ClientID])
	{
		m_aPlayersCurrentTick[ClientID].m_InputAvailable = true;
		m_aPlayersCurrentTick[ClientID].m_Input_Direction = PlayerInput->m_Direction;
		m_aPlayersCurrentTick[ClientID].m_Input_TargetX = PlayerInput->m_TargetX;
		m_aPlayersCurrentTick[ClientID].m_Input_TargetY = PlayerInput->m_TargetY;
		m_aPlayersCurrentTick[ClientID].m_Input_Jump = PlayerInput->m_Jump;
		m_aPlayersCurrentTick[ClientID].m_Input_Fire = PlayerInput->m_Fire;
		m_aPlayersCurrentTick[ClientID].m_Input_Hook = PlayerInput->m_Hook;
		m_aPlayersCurrentTick[ClientID].m_Input_PlayerFlags = PlayerInput->m_PlayerFlags;
		m_aPlayersCurrentTick[ClientID].m_Input_WantedWeapon = PlayerInput->m_WantedWeapon;
		m_aPlayersCurrentTick[ClientID].m_Input_NextWeapon = PlayerInput->m_NextWeapon;
		m_aPlayersCurrentTick[ClientID].m_Input_PrevWeapon = PlayerInput->m_PrevWeapon;

	}
}

void CBotDetection::SetInput(TickPlayer *Target, TickPlayer* Source) {
	Target->m_InputAvailable = Source->m_InputAvailable;
	Target->m_Input_Direction = Source->m_Input_Direction;
	Target->m_Input_TargetX = Source->m_Input_TargetX;
	Target->m_Input_TargetY = Source->m_Input_TargetY;
	Target->m_Input_Jump = Source->m_Input_Jump;
	Target->m_Input_Fire = Source->m_Input_Fire;
	Target->m_Input_Hook = Source->m_Input_Hook;
	Target->m_Input_PlayerFlags = Source->m_Input_PlayerFlags;
	Target->m_Input_WantedWeapon = Source->m_Input_WantedWeapon;
	Target->m_Input_NextWeapon = Source->m_Input_NextWeapon;
	Target->m_Input_PrevWeapon = Source->m_Input_PrevWeapon;
}

void CBotDetection::SetCore(TickPlayer *Target, TickPlayer *Source) {
	Target->m_CoreAvailable = Source->m_CoreAvailable;
	Target->m_JoinHash = Source->m_JoinHash;
	Target->m_ClientID = Source->m_ClientID;
	Target->m_Core_Tick = Source->m_Core_Tick;
	Target->m_Core_X = Source->m_Core_X;
	Target->m_Core_Y = Source->m_Core_Y;
	Target->m_Core_VelX = Source->m_Core_VelX;
	Target->m_Core_VelY = Source->m_Core_VelY;
	Target->m_Core_Angle = Source->m_Core_Angle;
	Target->m_Core_Direction = Source->m_Core_Direction;
	Target->m_Core_Jumped = Source->m_Core_Jumped;
	Target->m_Core_HookedPlayer = Source->m_Core_HookedPlayer;
	Target->m_Core_HookState = Source->m_Core_HookState;
	Target->m_Core_HookTick = Source->m_Core_HookTick;
	Target->m_Core_HookX = Source->m_Core_HookX;
	Target->m_Core_HookY = Source->m_Core_HookY;
	Target->m_Core_HookDx = Source->m_Core_HookDx;
	Target->m_Core_HookDy = Source->m_Core_HookDy;

}

void CBotDetection::CalculateDistanceToEveryPlayer() {
	int PosX = -1, PosY = -1, PosX2 = -1, PosY2 = -1;
	int CursorPosX = -1, CursorPosY = -1;
	double PosDistance = std::numeric_limits<double>::max(), MyCursorToPosDistance = std::numeric_limits<double>::max();

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_GameContext->m_apPlayers[i])
		{
			// ID i's positions
			PosX = m_aPlayersCurrentTick[i].m_Core_X;
			PosY = m_aPlayersCurrentTick[i].m_Core_Y;
			CursorPosX = PosX + m_aPlayersCurrentTick[i].m_Input_TargetX;
			CursorPosY = PosY + m_aPlayersCurrentTick[i].m_Input_TargetY;

			m_ClosestIDToCurrentIDCT[i] = -1;
			m_ClosestDistanceToCurrentIDCT[i] = std::numeric_limits<double>::max();
			m_ClosestIDToCursorDistanceCT[i] = std::numeric_limits<double>::max();

			for (int j = 0; j < MAX_CLIENTS; j++)
			{
				if (m_GameContext->m_apPlayers[j])
				{
					if (i != j)
					{
						// ID j's positions
						PosX2 = m_aPlayersCurrentTick[j].m_Core_X;
						PosY2 = m_aPlayersCurrentTick[j].m_Core_Y;
						// player distance
						PosDistance = sqrt(pow(PosX - PosX2, 2) + pow(PosY - PosY2, 2));
						// cursor of i to player j distance
						MyCursorToPosDistance = sqrt(pow(CursorPosX - PosX2, 2) + pow(CursorPosY - PosY2, 2));

					} else {
						PosDistance = std::numeric_limits<double>::max();
						MyCursorToPosDistance = std::numeric_limits<double>::max();
					}

					// set player distances
					if (PosDistance == std::numeric_limits<double>::max())
					{
						// if player is actually yourself
						m_PlayerDistanceCT[i][j] = -1;
					}
						// if player is someone else;
						m_PlayerDistanceCT[i][j] = PosDistance;

					if (PosDistance < m_ClosestDistanceToCurrentIDCT[i])
					{
						m_ClosestDistanceToCurrentIDCT[i] = PosDistance;
						m_ClosestIDToCurrentIDCT[i] = j;
						m_CursorToClosestDistanceIDCT[i] = MyCursorToPosDistance;
					}

					if (MyCursorToPosDistance < m_ClosestIDToCursorDistanceCT[i])
					{
						m_ClosestIDToCursorDistanceCT[i] = MyCursorToPosDistance;
						m_ClosestIDToCursorCT[i] = j;
					}


				}


			}
			// check if only you are online
			if (m_ClosestDistanceToCurrentIDCT[i] == std::numeric_limits<double>::max())
			{
				m_ClosestDistanceToCurrentIDCT[i] = -1;
				m_ClosestIDToCurrentIDCT[i] = -1;

			}
			// check if you are alone on the server, thus setting distance to -1
			if (m_ClosestIDToCursorDistanceCT[i] == std::numeric_limits<double>::max())
			{
				m_ClosestIDToCursorDistanceCT[i] = -1;
						m_ClosestIDToCursorCT[i] = -1;
			}
			// set cursor distance to closest player(by position distance) to -1 if you are alone on the server.
			if (m_CursorToClosestDistanceIDCT[i] ==  std::numeric_limits<double>::max())
			{
				m_CursorToClosestDistanceIDCT[i] = -1;
			}

			// reset variables
			PosX = -1;
			PosY = -1;
			CursorPosX = -1;
			CursorPosX = -1;
			PosX2 = -1;
			PosY2 = -1;
			PosDistance = -1;
			MyCursorToPosDistance = -1;
		}
	}



}




void CBotDetection::ResetCurrentTick() {
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{

		m_aPlayersCurrentTick[i].m_CoreAvailable = false;
		m_aPlayersCurrentTick[i].m_JoinHash = 0;
		m_aPlayersCurrentTick[i].m_ClientID = -1;
		m_aPlayersCurrentTick[i].m_Core_Tick = -1;
		m_aPlayersCurrentTick[i].m_Core_X = 0;
		m_aPlayersCurrentTick[i].m_Core_Y = 0;
		m_aPlayersCurrentTick[i].m_Core_VelX = 0;
		m_aPlayersCurrentTick[i].m_Core_VelY = 0;
		m_aPlayersCurrentTick[i].m_Core_Angle = 0;
		m_aPlayersCurrentTick[i].m_Core_Direction = 0;
		m_aPlayersCurrentTick[i].m_Core_Jumped = 0;
		m_aPlayersCurrentTick[i].m_Core_HookedPlayer = 0;
		m_aPlayersCurrentTick[i].m_Core_HookState = 0;
		m_aPlayersCurrentTick[i].m_Core_HookTick = 0;
		m_aPlayersCurrentTick[i].m_Core_HookX = 0;
		m_aPlayersCurrentTick[i].m_Core_HookY = 0;
		m_aPlayersCurrentTick[i].m_Core_HookDx = 0;
		m_aPlayersCurrentTick[i].m_Core_HookDy = 0;
		m_aPlayersCurrentTick[i].m_InputAvailable = false;
		m_aPlayersCurrentTick[i].m_Input_Direction = 0;
		m_aPlayersCurrentTick[i].m_Input_TargetX = 0;
		m_aPlayersCurrentTick[i].m_Input_TargetY = 0;
		m_aPlayersCurrentTick[i].m_Input_Jump = -1;
		m_aPlayersCurrentTick[i].m_Input_Fire = -1;
		m_aPlayersCurrentTick[i].m_Input_Hook = -1;
		m_aPlayersCurrentTick[i].m_Input_PlayerFlags = 0;
		m_aPlayersCurrentTick[i].m_Input_WantedWeapon = 0;
		m_aPlayersCurrentTick[i].m_Input_NextWeapon = 0;
		m_aPlayersCurrentTick[i].m_Input_PrevWeapon = 0;
	}
}



char* CBotDetection::GetInfoString(int ClientID) {
	char *aBuf = (char*)malloc(sizeof(char) * 512);
	str_format(aBuf, 512, " %16s : CD: %.2f CCD: %.2f CDC: %.2f", m_GameContext->Server()->ClientName(ClientID),
	          m_ClosestDistanceToCurrentIDCT[ClientID], m_ClosestIDToCursorDistanceCT[ClientID], m_ClosestIDToCursorDistanceCT[ClientID]);

	return aBuf;
}

void CBotDetection::OnPlayerConnect(int ClientID) {


}

void CBotDetection::OnPlayerDisconnect(int ClientID) {

}









































































































































































































































































































































































