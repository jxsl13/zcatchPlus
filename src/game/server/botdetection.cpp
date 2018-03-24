#include "botdetection.h"
#include <cstdlib>
#include <limits>








CBotDetection::CBotDetection(CGameContext *GameServer)
{
	m_GameContext = GameServer;
}

CBotDetection::~CBotDetection() {

}

void CBotDetection::OnTick(){
	CalculateDistanceToEveryPlayer();

	ManageLastInputQueue();
	ResetCurrentTick();
}

void CBotDetection::ManageLastInputQueue() {
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{

		if (m_GameContext->m_apPlayers[i] && m_GameContext->m_apPlayers[i]->GetCharacter())
		{

			if (m_aPlayersCurrentTick[i].m_InputAvailable && m_aPlayersCurrentTick[i].m_CoreAvailable)
			{
				m_aPlayerInputBacklog[i].push(m_aPlayersCurrentTick[i]);
			}
			if (!m_aPlayersCurrentTick[i].m_InputAvailable && m_aPlayerInputBacklog[i].size() >= 1)
			{
				TickPlayer temp = m_aPlayerInputBacklog[i].front();

				m_aPlayersCurrentTick[i].m_InputAvailable = true;
				m_aPlayersCurrentTick[i].m_Input_Direction = temp.m_Input_Direction;
				m_aPlayersCurrentTick[i].m_Input_TargetX = temp.m_Input_TargetX;
				m_aPlayersCurrentTick[i].m_Input_TargetY = temp.m_Input_TargetY;
				m_aPlayersCurrentTick[i].m_Input_Jump = temp.m_Input_Jump;
				m_aPlayersCurrentTick[i].m_Input_Fire = temp.m_Input_Fire;
				m_aPlayersCurrentTick[i].m_Input_Hook = temp.m_Input_Hook;
				m_aPlayersCurrentTick[i].m_Input_PlayerFlags = temp.m_Input_PlayerFlags;
				m_aPlayersCurrentTick[i].m_Input_WantedWeapon = temp.m_Input_WantedWeapon;
				m_aPlayersCurrentTick[i].m_Input_NextWeapon = temp.m_Input_NextWeapon;
				m_aPlayersCurrentTick[i].m_Input_PrevWeapon = temp.m_Input_PrevWeapon;

				m_aPlayerInputBacklog[i].pop();
				m_aPlayerInputBacklog[i].push(m_aPlayersCurrentTick[i]);
			}

			while (m_aPlayerInputBacklog[i].size() > 1) {
				m_aPlayerInputBacklog[i].pop();
			}

		}
	}
}



void CBotDetection::AddPlayerCore(int ClientID, int JoinHash, CNetObj_CharacterCore *PlayerCore) {
	if (m_GameContext->m_apPlayers[ClientID])
	{
		m_aPlayersCurrentTick[ClientID] = {

			.m_CoreAvailable = true,
			.m_JoinHash = JoinHash,
			.m_ClientID = ClientID,
			.m_Core_Tick = PlayerCore->m_Tick,
			.m_Core_X = PlayerCore->m_X,
			.m_Core_Y = PlayerCore->m_Y,
			.m_Core_VelX = PlayerCore->m_VelX,
			.m_Core_VelY = PlayerCore->m_VelY,
			.m_Core_Angle = PlayerCore->m_Angle,
			.m_Core_Direction = PlayerCore->m_Direction,
			.m_Core_Jumped = PlayerCore->m_Jumped,
			.m_Core_HookedPlayer = PlayerCore->m_HookedPlayer,
			.m_Core_HookState = PlayerCore->m_HookState,
			.m_Core_HookTick = PlayerCore->m_HookTick,
			.m_Core_HookX = PlayerCore->m_HookX,
			.m_Core_HookY = PlayerCore->m_HookY,
			.m_Core_HookDx = PlayerCore->m_HookDx,
			.m_Core_HookDy = PlayerCore->m_HookDy
		};
	}

}

void CBotDetection::AddPlayerInput(int ClientID, int JoinHash, CNetObj_PlayerInput *PlayerInput) {
	if (m_GameContext->m_apPlayers[ClientID])
	{
		m_aPlayersCurrentTick[ClientID] = {

			.m_InputAvailable = true,
			.m_Input_Direction = PlayerInput->m_Direction,
			.m_Input_TargetX = PlayerInput->m_TargetX,
			.m_Input_TargetY = PlayerInput->m_TargetY,
			.m_Input_Jump = PlayerInput->m_Jump,
			.m_Input_Fire = PlayerInput->m_Fire,
			.m_Input_Hook = PlayerInput->m_Hook,
			.m_Input_PlayerFlags = PlayerInput->m_PlayerFlags,
			.m_Input_WantedWeapon = PlayerInput->m_WantedWeapon,
			.m_Input_NextWeapon = PlayerInput->m_NextWeapon,
			.m_Input_PrevWeapon = PlayerInput->m_PrevWeapon,
		};
	}
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
		m_aPlayersCurrentTick[i] = {
			.m_CoreAvailable = false,
			.m_JoinHash = 0,
			.m_ClientID = -1,
			.m_Core_Tick = -1,
			.m_Core_X = -1,
			.m_Core_Y = -1,
			.m_Core_VelX = -1,
			.m_Core_VelY = -1,
			.m_Core_Angle = -1,
			.m_Core_Direction = 0,
			.m_Core_Jumped = -1,
			.m_Core_HookedPlayer = -1,
			.m_Core_HookState = -1,
			.m_Core_HookTick = -1,
			.m_Core_HookX = -1,
			.m_Core_HookY = -1,
			.m_Core_HookDx = -1,
			.m_Core_HookDy = -1,
			.m_InputAvailable = false,
			.m_Input_Direction = -1,
			.m_Input_TargetX = -1,
			.m_Input_TargetY = -1,
			.m_Input_Jump = -1,
			.m_Input_Fire = -1,
			.m_Input_Hook = -1,
			.m_Input_PlayerFlags = -1,
			.m_Input_WantedWeapon = -1,
			.m_Input_NextWeapon = -1,
			.m_Input_PrevWeapon = -1,
		};
	}
}



char* CBotDetection::GetInfoString(int ClientID) {
	char *aBuf = (char*)malloc(sizeof(char) * 512);
	str_format(aBuf, 512, " %s : CD: %.2f CCD: %.2f", m_GameContext->Server()->ClientName(ClientID),
	           m_ClosestDistanceToCurrentIDCT[ClientID], m_ClosestIDToCursorDistanceCT[ClientID]);
	return aBuf;
}









































































































































































































































































































































































