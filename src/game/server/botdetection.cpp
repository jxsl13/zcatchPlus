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
	CalculateDistanceToEveryPlayer();
	ManageLastInputQueue();
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
					SetInput(m_aPlayersCurrentTick[i], temp);

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

					SetCore(m_aPlayersCurrentTick[i], temp);

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
					SetCore(m_aPlayersCurrentTick[i], m_aPlayerCoreBacklog[i].front());
					SetInput(m_aPlayersCurrentTick[i], m_aPlayerInputBacklog[i].front());
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

			//dbg_msg("TEST", "Input: %ld Movement: %ld Player: %ld", m_aPlayerInputBacklog[i].size(), m_aPlayerCoreBacklog[i].size(), m_aPlayerBacklog[i].size());
			if (m_aPlayerBacklog[i].size() > 0)
			{
				dbg_msg("TEST", "m_Core_X: %d, m_Core_Y: %d. m_Input_TargetX: %d, m_Input_TargetY: %d",
				        m_aPlayerBacklog[i].front().m_Core_X,
				        m_aPlayerBacklog[i].front().m_Core_Y,
				        m_aPlayerBacklog[i].front().m_Input_TargetX,
				        m_aPlayerBacklog[i].front().m_Input_TargetY);
			}
			//dbg_msg("TEST", "Input: %d Movement: %d", m_aPlayersCurrentTick[i].m_InputAvailable, m_aPlayersCurrentTick[i].m_CoreAvailable);

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

void CBotDetection::SetInput(TickPlayer Target, TickPlayer Source) {
	Target.m_InputAvailable = Source.m_InputAvailable;
	Target.m_Input_Direction = Source.m_Input_Direction;
	Target.m_Input_TargetX = Source.m_Input_TargetX;
	Target.m_Input_TargetY = Source.m_Input_TargetY;
	Target.m_Input_Jump = Source.m_Input_Jump;
	Target.m_Input_Fire = Source.m_Input_Fire;
	Target.m_Input_Hook = Source.m_Input_Hook;
	Target.m_Input_PlayerFlags = Source.m_Input_PlayerFlags;
	Target.m_Input_WantedWeapon = Source.m_Input_WantedWeapon;
	Target.m_Input_NextWeapon = Source.m_Input_NextWeapon;
	Target.m_Input_PrevWeapon = Source.m_Input_PrevWeapon;
}

void CBotDetection::SetCore(TickPlayer Target, TickPlayer Source) {
	Target.m_CoreAvailable = Source.m_CoreAvailable;
	Target.m_JoinHash = Source.m_JoinHash;
	Target.m_ClientID = Source.m_ClientID;
	Target.m_Core_Tick = Source.m_Core_Tick;
	Target.m_Core_X = Source.m_Core_X;
	Target.m_Core_Y = Source.m_Core_Y;
	Target.m_Core_VelX = Source.m_Core_VelX;
	Target.m_Core_VelY = Source.m_Core_VelY;
	Target.m_Core_Angle = Source.m_Core_Angle;
	Target.m_Core_Direction = Source.m_Core_Direction;
	Target.m_Core_Jumped = Source.m_Core_Jumped;
	Target.m_Core_HookedPlayer = Source.m_Core_HookedPlayer;
	Target.m_Core_HookState = Source.m_Core_HookState;
	Target.m_Core_HookTick = Source.m_Core_HookTick;
	Target.m_Core_HookX = Source.m_Core_HookX;
	Target.m_Core_HookY = Source.m_Core_HookY;
	Target.m_Core_HookDx = Source.m_Core_HookDx;
	Target.m_Core_HookDy = Source.m_Core_HookDy;

}

void CBotDetection::CalculateDistanceToEveryPlayer() {
	int x, y, x2, y2;


	// actual distance  of cursor to above defined id.
	double m_ClosestIDToCursorDistanceCT[MAX_CLIENTS];

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		TickPlayer current = m_aPlayersCurrentTick[i];
		x = current.m_Core_X;
		y = current.m_Core_Y;
		int curentCursorX = x + current.m_Input_TargetX;
		int currentCursorY = y + current.m_Input_TargetY;

		// initial min search init with max value
		m_ClosestDistanceToCurrentIDCT[i] = std::numeric_limits<double>::max();
		m_ClosestIDToCursorCT[i] = i;
		m_ClosestIDToCursorDistanceCT[i] = std::numeric_limits<double>::max();
		for (int j = 0; j < MAX_CLIENTS; ++j)
		{
			TickPlayer other = m_aPlayersCurrentTick[j];
			x2 = other.m_Core_X;
			y2 = other.m_Core_Y;

			if (i != j)
			{
				// fill distance matrix
				m_PlayerDistanceCT[i][j] = sqrt((x - x2) * (x - x2) + (y - y2) * (y - y2));
				if (m_PlayerDistanceCT[i][j] < m_ClosestDistanceToCurrentIDCT[i])
				{
					// fill closest distance array with closest player ids
					m_ClosestDistanceToCurrentIDCT[i] = m_PlayerDistanceCT[i][j];
					// fill distance array with distances
					m_ClosestIDToCurrentIDCT[i] = j;
					// update player cursor position's distance to closest player (to player position)
					m_CursorToClosestDistanceIDCT[i] = sqrt((curentCursorX - x2) * (curentCursorX - x2) + (currentCursorY - y2) * (currentCursorY - y2));
				}

			} else {
				m_PlayerDistanceCT[i][j] = 0;

			}


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
	str_format(aBuf, 512, " %s : CD: %.2f CCD: %.2f", m_GameContext->Server()->ClientName(ClientID),
	           m_ClosestDistanceToCurrentIDCT[ClientID], m_ClosestIDToCursorDistanceCT[ClientID]);
	return aBuf;
}

void CBotDetection::OnPlayerConnect(int ClientID){


}

void CBotDetection::OnPlayerDisconnect(int ClientID){

}









































































































































































































































































































































































