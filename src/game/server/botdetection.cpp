#include "botdetection.h"
#include <cstdlib>
#include <limits>
#include <cmath>






CBotDetection::CBotDetection(CGameContext *GameServer)
{
	m_DbMode = MODE_SQLITE;
	m_GameContext = GameServer;
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		ResetID(i);
		ResetLongTimeData(i);

	}
	CreateDatabase("botdetection.db");
}

CBotDetection::~CBotDetection() {
	CloseDatabase();
}

void CBotDetection::OnTick() {
	//m_aPlayersCurrentTick needs to have some proper values first
	ManageLastInputQueue();

	// then calculate stuff
	CalculatePlayerRelations();



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
double CBotDetection::Distance(int x, int y, int x2, int y2) {
	return sqrt(pow(x - x2 , 2) + pow(y - y2, 2));
}

double CBotDetection::Angle(int x, int y, int x2, int y2) {

	static const double TWOPI = 6.2831853071795865;
	static const double RAD2DEG = 57.2957795130823209;
	if (x == x2 && y == y2)
		return -1;

	double theta = atan2(x2 - x, y2 - y);
	if (theta < 0.0)
		theta += TWOPI;
	return RAD2DEG * theta;
}



void CBotDetection::CalculatePlayerRelations() {
	int PosX = -1, PosY = -1, PosX2 = -1, PosY2 = -1;
	int CursorPosX = -1, CursorPosY = -1;
	double PosDistance = std::numeric_limits<double>::max(), MyCursorToPosDistance = std::numeric_limits<double>::max();

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_GameContext->m_apPlayers[i] && m_aPlayersCurrentTick[i].m_Core_Tick - m_aPlayersCurrentTick[i].m_JoinTick > 250)
		{
			// ID i's positions
			PosX = m_aPlayersCurrentTick[i].m_Core_X;
			PosY = m_aPlayersCurrentTick[i].m_Core_Y;
			CursorPosX = PosX + m_aPlayersCurrentTick[i].m_Input_TargetX;
			CursorPosY = PosY + m_aPlayersCurrentTick[i].m_Input_TargetY;

			m_ClosestIDToCurrentIDCT[i] = -1;
			m_ClosestDistanceToCurrentIDCT[i] = std::numeric_limits<double>::max();
			m_ClosestIDToCursorDistanceCT[i] = std::numeric_limits<double>::max();

			// cursor distances from body
			m_InputCount[i]++;
			m_CurrentDistanceFromBody[i] = Distance(0, 0, m_aPlayersCurrentTick[i].m_Input_TargetX, m_aPlayersCurrentTick[i].m_Input_TargetY);
			m_AvgDistanceSum[i] += m_CurrentDistanceFromBody[i];
			m_AvgDistanceFromBody[i] = m_AvgDistanceSum[i] / m_InputCount[i];

			if (m_CurrentDistanceFromBody[i] < m_MinDistanceFromBody[i])
			{
				m_MinDistanceFromBody[i] = m_CurrentDistanceFromBody[i];
			}
			if (m_CurrentDistanceFromBody[i] > m_MaxDistanceFromBody[i])
			{
				m_MaxDistanceFromBody[i] = m_CurrentDistanceFromBody[i];
			}

			m_CursorAngle[i] = Angle(PosX, PosY, CursorPosX, CursorPosY);


			for (int j = 0; j < MAX_CLIENTS; j++)
			{
				if (m_GameContext->m_apPlayers[j] && i != j)
				{

					// ID j's positions
					PosX2 = m_aPlayersCurrentTick[j].m_Core_X;
					PosY2 = m_aPlayersCurrentTick[j].m_Core_Y;
					// player distance
					PosDistance = Distance(PosX, PosY, PosX2, PosY2);
					// cursor of i to player j distance
					MyCursorToPosDistance = Distance(CursorPosX, CursorPosY, PosX2, PosY2);

					// if player is someone else;
					m_PlayerDistanceCT[i][j] = PosDistance;



					// set player distances

					if (PosDistance < m_ClosestDistanceToCurrentIDCT[i])
					{
						// closest player distance
						m_ClosestDistanceToCurrentIDCT[i] = PosDistance;
						// closest player id
						m_ClosestIDToCurrentIDCT[i] = j;
						// cursor distance to this player
						m_CursorToClosestDistanceIDCT[i] = MyCursorToPosDistance;
						// angle to nearest player from player positions, not cursor.
						m_AngleToNearestPlayer[i] = Angle(PosX, PosY, PosX2, PosY2);
					}

					if (MyCursorToPosDistance < m_ClosestIDToCursorDistanceCT[i])
					{
						// closest distance from cursor position to any enemy player
						m_ClosestIDToCursorDistanceCT[i] = MyCursorToPosDistance;
						m_ClosestIDToCursorCT[i] = j;
					}


					int inSight = EnemyInSight(i, j);

					if (inSight)
					{
						int diff = abs(m_CursorAngle[i] - Angle(PosX, PosY, PosX2, PosY2));
						if (diff < m_CursorAngleDifferenceToPlayerInSight[i])
						{
							m_CursorAngleDifferenceToPlayerInSight[i] = diff;
							m_CursorAngleToClosestAngleDiffPlayerInSight[i] = j;
						}

						switch (inSight) {
						case 1:
							m_SumAngleToPlayerDifferenceInArea[1][i] += diff;;
							m_CountPlayerInSightInArea[1][i]++;
							m_AvgCursorAngleToPlayerAngleDifferenceInArea[1][i] = m_SumAngleToPlayerDifferenceInArea[1][i] / m_CountPlayerInSightInArea[1][i];
							m_SumCursorToPlayerIfInSightInAreaDistance[1][i] += MyCursorToPosDistance;
							m_AvgCursorToPlayerIfInSightInAreaDistance[1][i] = m_SumCursorToPlayerIfInSightInAreaDistance[1][i] / m_CountPlayerInSightInArea[1][i];
							if (diff < m_MinCursorAngleToPlayerAngleDifferenceInArea[1][i] )
							{
								m_MinCursorAngleToPlayerAngleDifferenceInArea[1][i] = diff;
							}

							if (m_aPlayersCurrentTick[i].m_Input_Fire)
							{
								m_NumShotsInArea[1][i]++;
								m_SumAngleDifferenceOnShot[1][i] += diff;
								m_AvgAngleDifferenceOnShot[1][i] = m_SumAngleDifferenceOnShot[1][i] / m_NumShotsInArea[1][i];

							}
							if (m_aPlayersCurrentTick[i].m_Input_Hook)
							{
								m_NumHooksInArea[1][i]++;
								m_SumAngleDifferenceOnHook[1][i] += diff;
								m_AvgAngleDifferenceOnHook[1][i] = m_SumAngleDifferenceOnHook[1][i] / m_NumHooksInArea[1][i];

							}

							break;
						case 2:
							m_SumAngleToPlayerDifferenceInArea[2][i] += diff;;
							m_CountPlayerInSightInArea[2][i]++;
							m_AvgCursorAngleToPlayerAngleDifferenceInArea[2][i] = m_SumAngleToPlayerDifferenceInArea[2][i] / m_CountPlayerInSightInArea[2][i];
							m_SumCursorToPlayerIfInSightInAreaDistance[2][i] += MyCursorToPosDistance;
							m_AvgCursorToPlayerIfInSightInAreaDistance[2][i] = m_SumCursorToPlayerIfInSightInAreaDistance[2][i] / m_CountPlayerInSightInArea[2][i];
							if (diff < m_MinCursorAngleToPlayerAngleDifferenceInArea[2][i] )
							{
								m_MinCursorAngleToPlayerAngleDifferenceInArea[2][i] = diff;
							}

							if (m_aPlayersCurrentTick[i].m_Input_Fire)
							{
								m_NumShotsInArea[2][i]++;
								m_SumAngleDifferenceOnShot[2][i] += diff;
								m_AvgAngleDifferenceOnShot[2][i] = m_SumAngleDifferenceOnShot[2][i] / m_NumShotsInArea[2][i];

							}
							if (m_aPlayersCurrentTick[i].m_Input_Hook)
							{
								m_NumHooksInArea[2][i]++;
								m_SumAngleDifferenceOnHook[2][i] += diff;
								m_AvgAngleDifferenceOnHook[2][i] = m_SumAngleDifferenceOnHook[2][i] / m_NumHooksInArea[2][i];

							}

							break;
						case 3:
							m_SumAngleToPlayerDifferenceInArea[3][i] += diff;;
							m_CountPlayerInSightInArea[3][i]++;
							m_AvgCursorAngleToPlayerAngleDifferenceInArea[3][i] = m_SumAngleToPlayerDifferenceInArea[3][i] / m_CountPlayerInSightInArea[3][i];
							m_SumCursorToPlayerIfInSightInAreaDistance[3][i] += MyCursorToPosDistance;
							m_AvgCursorToPlayerIfInSightInAreaDistance[3][i] = m_SumCursorToPlayerIfInSightInAreaDistance[3][i] / m_CountPlayerInSightInArea[3][i];
							if (diff < m_MinCursorAngleToPlayerAngleDifferenceInArea[3][i] )
							{
								m_MinCursorAngleToPlayerAngleDifferenceInArea[3][i] = diff;
							}

							if (m_aPlayersCurrentTick[i].m_Input_Fire)
							{
								m_NumShotsInArea[3][i]++;
								m_SumAngleDifferenceOnShot[3][i] += diff;
								m_AvgAngleDifferenceOnShot[3][i] = m_SumAngleDifferenceOnShot[3][i] / m_NumShotsInArea[3][i];

							}
							if (m_aPlayersCurrentTick[i].m_Input_Hook)
							{
								m_NumHooksInArea[3][i]++;
								m_SumAngleDifferenceOnHook[3][i] += diff;
								m_AvgAngleDifferenceOnHook[3][i] = m_SumAngleDifferenceOnHook[3][i] / m_NumHooksInArea[3][i];

							}

							break;
						case 4:
							m_SumAngleToPlayerDifferenceInArea[4][i] += diff;;
							m_CountPlayerInSightInArea[4][i]++;
							m_AvgCursorAngleToPlayerAngleDifferenceInArea[4][i] = m_SumAngleToPlayerDifferenceInArea[4][i] / m_CountPlayerInSightInArea[4][i];
							m_SumCursorToPlayerIfInSightInAreaDistance[4][i] += MyCursorToPosDistance;
							m_AvgCursorToPlayerIfInSightInAreaDistance[4][i] = m_SumCursorToPlayerIfInSightInAreaDistance[4][i] / m_CountPlayerInSightInArea[4][i];
							if (diff < m_MinCursorAngleToPlayerAngleDifferenceInArea[4][i] )
							{
								m_MinCursorAngleToPlayerAngleDifferenceInArea[4][i] = diff;
							}

							if (m_aPlayersCurrentTick[i].m_Input_Fire)
							{
								m_NumShotsInArea[4][i]++;
								m_SumAngleDifferenceOnShot[4][i] += diff;
								m_AvgAngleDifferenceOnShot[4][i] = m_SumAngleDifferenceOnShot[4][i] / m_NumShotsInArea[4][i];

							}
							if (m_aPlayersCurrentTick[i].m_Input_Hook)
							{
								m_NumHooksInArea[4][i]++;
								m_SumAngleDifferenceOnHook[4][i] += diff;
								m_AvgAngleDifferenceOnHook[4][i] = m_SumAngleDifferenceOnHook[4][i] / m_NumHooksInArea[4][i];

							}

							break;
						case 5:
							m_SumAngleToPlayerDifferenceInArea[5][i] += diff;;
							m_CountPlayerInSightInArea[5][i]++;
							m_AvgCursorAngleToPlayerAngleDifferenceInArea[5][i] = m_SumAngleToPlayerDifferenceInArea[5][i] / m_CountPlayerInSightInArea[5][i];
							m_SumCursorToPlayerIfInSightInAreaDistance[5][i] += MyCursorToPosDistance;
							m_AvgCursorToPlayerIfInSightInAreaDistance[5][i] = m_SumCursorToPlayerIfInSightInAreaDistance[5][i] / m_CountPlayerInSightInArea[5][i];
							if (diff < m_MinCursorAngleToPlayerAngleDifferenceInArea[5][i] )
							{
								m_MinCursorAngleToPlayerAngleDifferenceInArea[5][i] = diff;
							}

							if (m_aPlayersCurrentTick[i].m_Input_Fire)
							{
								m_NumShotsInArea[5][i]++;
								m_SumAngleDifferenceOnShot[5][i] += diff;
								m_AvgAngleDifferenceOnShot[5][i] = m_SumAngleDifferenceOnShot[5][i] / m_NumShotsInArea[5][i];

							}
							if (m_aPlayersCurrentTick[i].m_Input_Hook)
							{
								m_NumHooksInArea[5][i]++;
								m_SumAngleDifferenceOnHook[5][i] += diff;
								m_AvgAngleDifferenceOnHook[5][i] = m_SumAngleDifferenceOnHook[5][i] / m_NumHooksInArea[5][i];

							}

							break;
						case 6:
							m_SumAngleToPlayerDifferenceInArea[6][i] += diff;;
							m_CountPlayerInSightInArea[6][i]++;
							m_AvgCursorAngleToPlayerAngleDifferenceInArea[6][i] = m_SumAngleToPlayerDifferenceInArea[6][i] / m_CountPlayerInSightInArea[6][i];
							m_SumCursorToPlayerIfInSightInAreaDistance[6][i] += MyCursorToPosDistance;
							m_AvgCursorToPlayerIfInSightInAreaDistance[6][i] = m_SumCursorToPlayerIfInSightInAreaDistance[6][i] / m_CountPlayerInSightInArea[6][i];
							if (diff < m_MinCursorAngleToPlayerAngleDifferenceInArea[6][i] )
							{
								m_MinCursorAngleToPlayerAngleDifferenceInArea[6][i] = diff;
							}

							if (m_aPlayersCurrentTick[i].m_Input_Fire)
							{
								m_NumShotsInArea[6][i]++;
								m_SumAngleDifferenceOnShot[6][i] += diff;
								m_AvgAngleDifferenceOnShot[6][i] = m_SumAngleDifferenceOnShot[6][i] / m_NumShotsInArea[6][i];

							}
							if (m_aPlayersCurrentTick[i].m_Input_Hook)
							{
								m_NumHooksInArea[6][i]++;
								m_SumAngleDifferenceOnHook[6][i] += diff;
								m_AvgAngleDifferenceOnHook[6][i] = m_SumAngleDifferenceOnHook[6][i] / m_NumHooksInArea[6][i];

							}

							break;
						case 7:
							m_SumAngleToPlayerDifferenceInArea[7][i] += diff;;
							m_CountPlayerInSightInArea[7][i]++;
							m_AvgCursorAngleToPlayerAngleDifferenceInArea[7][i] = m_SumAngleToPlayerDifferenceInArea[7][i] / m_CountPlayerInSightInArea[7][i];
							m_SumCursorToPlayerIfInSightInAreaDistance[7][i] += MyCursorToPosDistance;
							m_AvgCursorToPlayerIfInSightInAreaDistance[7][i] = m_SumCursorToPlayerIfInSightInAreaDistance[7][i] / m_CountPlayerInSightInArea[7][i];
							if (diff < m_MinCursorAngleToPlayerAngleDifferenceInArea[7][i] )
							{
								m_MinCursorAngleToPlayerAngleDifferenceInArea[7][i] = diff;
							}

							if (m_aPlayersCurrentTick[i].m_Input_Fire)
							{
								m_NumShotsInArea[7][i]++;
								m_SumAngleDifferenceOnShot[7][i] += diff;
								m_AvgAngleDifferenceOnShot[7][i] = m_SumAngleDifferenceOnShot[7][i] / m_NumShotsInArea[7][i];

							}
							if (m_aPlayersCurrentTick[i].m_Input_Hook)
							{
								m_NumHooksInArea[7][i]++;
								m_SumAngleDifferenceOnHook[7][i] += diff;
								m_AvgAngleDifferenceOnHook[7][i] = m_SumAngleDifferenceOnHook[7][i] / m_NumHooksInArea[7][i];

							}

							break;
						case 8:
							m_SumAngleToPlayerDifferenceInArea[8][i] += diff;;
							m_CountPlayerInSightInArea[8][i]++;
							m_AvgCursorAngleToPlayerAngleDifferenceInArea[8][i] = m_SumAngleToPlayerDifferenceInArea[8][i] / m_CountPlayerInSightInArea[8][i];
							m_SumCursorToPlayerIfInSightInAreaDistance[8][i] += MyCursorToPosDistance;
							m_AvgCursorToPlayerIfInSightInAreaDistance[8][i] = m_SumCursorToPlayerIfInSightInAreaDistance[8][i] / m_CountPlayerInSightInArea[8][i];
							if (diff < m_MinCursorAngleToPlayerAngleDifferenceInArea[8][i] )
							{
								m_MinCursorAngleToPlayerAngleDifferenceInArea[8][i] = diff;
							}

							if (m_aPlayersCurrentTick[i].m_Input_Fire)
							{
								m_NumShotsInArea[8][i]++;
								m_SumAngleDifferenceOnShot[8][i] += diff;
								m_AvgAngleDifferenceOnShot[8][i] = m_SumAngleDifferenceOnShot[8][i] / m_NumShotsInArea[8][i];

							}
							if (m_aPlayersCurrentTick[i].m_Input_Hook)
							{
								m_NumHooksInArea[8][i]++;
								m_SumAngleDifferenceOnHook[8][i] += diff;
								m_AvgAngleDifferenceOnHook[8][i] = m_SumAngleDifferenceOnHook[8][i] / m_NumHooksInArea[8][i];

							}

							break;
						case 9:
							m_SumAngleToPlayerDifferenceInArea[9][i] += diff;;
							m_CountPlayerInSightInArea[9][i]++;
							m_AvgCursorAngleToPlayerAngleDifferenceInArea[9][i] = m_SumAngleToPlayerDifferenceInArea[9][i] / m_CountPlayerInSightInArea[9][i];
							m_SumCursorToPlayerIfInSightInAreaDistance[9][i] += MyCursorToPosDistance;
							m_AvgCursorToPlayerIfInSightInAreaDistance[9][i] = m_SumCursorToPlayerIfInSightInAreaDistance[9][i] / m_CountPlayerInSightInArea[9][i];
							if (diff < m_MinCursorAngleToPlayerAngleDifferenceInArea[9][i] )
							{
								m_MinCursorAngleToPlayerAngleDifferenceInArea[9][i] = diff;
							}

							if (m_aPlayersCurrentTick[i].m_Input_Fire)
							{
								m_NumShotsInArea[9][i]++;
								m_SumAngleDifferenceOnShot[9][i] += diff;
								m_AvgAngleDifferenceOnShot[9][i] = m_SumAngleDifferenceOnShot[9][i] / m_NumShotsInArea[9][i];

							}
							if (m_aPlayersCurrentTick[i].m_Input_Hook)
							{
								m_NumHooksInArea[9][i]++;
								m_SumAngleDifferenceOnHook[9][i] += diff;
								m_AvgAngleDifferenceOnHook[9][i] = m_SumAngleDifferenceOnHook[9][i] / m_NumHooksInArea[9][i];

							}

							break;
						case 10:
							m_SumAngleToPlayerDifferenceInArea[0][i] += diff;;
							m_CountPlayerInSightInArea[0][i]++;
							m_AvgCursorAngleToPlayerAngleDifferenceInArea[0][i] = m_SumAngleToPlayerDifferenceInArea[0][i] / m_CountPlayerInSightInArea[0][i];
							m_SumCursorToPlayerIfInSightInAreaDistance[0][i] += MyCursorToPosDistance;
							m_AvgCursorToPlayerIfInSightInAreaDistance[0][i] = m_SumCursorToPlayerIfInSightInAreaDistance[0][i] / m_CountPlayerInSightInArea[0][i];
							if (diff < m_MinCursorAngleToPlayerAngleDifferenceInArea[0][i] )
							{
								m_MinCursorAngleToPlayerAngleDifferenceInArea[0][i] = diff;
							}

							if (m_aPlayersCurrentTick[i].m_Input_Fire)
							{
								m_NumShotsInArea[0][i]++;
								m_SumAngleDifferenceOnShot[0][i] += diff;
								m_AvgAngleDifferenceOnShot[0][i] = m_SumAngleDifferenceOnShot[0][i] / m_NumShotsInArea[0][i];

							}
							if (m_aPlayersCurrentTick[i].m_Input_Hook)
							{
								m_NumHooksInArea[0][i]++;
								m_SumAngleDifferenceOnHook[0][i] += diff;
								m_AvgAngleDifferenceOnHook[0][i] = m_SumAngleDifferenceOnHook[0][i] / m_NumHooksInArea[0][i];

							}

							break;
						default: break;
						}

					}

				} else if (m_GameContext->m_apPlayers[j] && i == j)
				{
					/* code */
				}


			}

		}
	}

}

/**
 * @brief Returns, in which sight area an enemy is, 0 if not in sight.
 * @details [long description]
 *
 * @param ClientID [description]
 * @param EnemyID [description]
 *
 * @return [description]
 */
int CBotDetection::EnemyInSight(int ClientID, int EnemyID) {
	if (ClientID == EnemyID)
	{
		return 0;
	}

	/**
		 * Definition: Sight Areas:
		 * __________ Max logical mouse distance is 405 with static cam __________
		 *	Area 1: Distance from Character: 50 Circular
		 *	Area 2: Distance from Character: 120 Circular
		 *	Area 3: Distance from Character: 240 Circular
		 *	Area 4: Distance from Caracter: 450 Circular and 500 in Dx
		 *	Area 5: Distance from Character: 450 Dy and 675 Dx
		 *	Area 6:	Distance from Character: 450 Dy and 775 Dx
		 *	Area 7: Distance from Character: 480 Dy and 840 Dx Last Static Area
		 *__________ Max logical mouse distance is 635 with dynamic cam __________
		 *	Area 8:	Distance from Character: 935 Dx and 650 Dy - normal dyn cam area
		 *	Area 9: Distance from Character: 995 Dx and 795 Dy - Very critical area
		 *	Area 10: Distance from Character: 1000 Dx and 800 Dy - impossible area
		 */

	double distX = abs(m_aPlayersCurrentTick[ClientID].m_Core_X - m_aPlayersCurrentTick[EnemyID].m_Core_X);
	double distY = abs(m_aPlayersCurrentTick[ClientID].m_Core_Y - m_aPlayersCurrentTick[EnemyID].m_Core_Y);
	double distAbs = Distance(m_aPlayersCurrentTick[ClientID].m_Core_X,
	                          m_aPlayersCurrentTick[ClientID].m_Core_Y,
	                          m_aPlayersCurrentTick[EnemyID].m_Core_X,
	                          m_aPlayersCurrentTick[EnemyID].m_Core_Y);

	// Collision check, if players can actually shoot at each other
	vec2 Pos0 = vec2(m_aPlayersCurrentTick[ClientID].m_Core_X, m_aPlayersCurrentTick[ClientID].m_Core_Y);
	vec2 Pos1 = vec2(m_aPlayersCurrentTick[EnemyID].m_Core_X, m_aPlayersCurrentTick[EnemyID].m_Core_Y);

	if (!m_GameContext->Collision()->IntersectLine(Pos0, Pos1, NULL, NULL)) {
		return 0;
	}
	// todo : else case try to use grenade projectile curvature to calculate positioning stuff.

	if (distAbs < 50.0)
	{
		return 1;
	} else if (distAbs < 120.0)
	{
		return 2;
	} else if (distAbs < 240.0)
	{
		return 3;
	} else if (distAbs < 450.0 && distX < 500.0)
	{
		return 4;
	} else if (distY < 450.0 && distX < 675.0)
	{
		return 5;
	} else if (distY < 450.0 && distX < 775.0)
	{
		return 6;
	} else if (distY < 480.0 && distX < 840.0)
	{
		return 7;
	} else if (distX < 935.0 && distY < 650.0)
	{
		return 8;
	} else if (distX < 995.0 && distY < 795.0)
	{
		return 9;
	} else if (distX <= 1000.0 && distY <= 800.0)
	{
		return 10;
	} else {
		return 0;
	}
}


void CBotDetection::ResetCurrentTick() {
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{

		m_aPlayersCurrentTick[i].m_CoreAvailable = false;
		//joinhash and id do not belong in here
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
	// str_format(aBuf, 512, " %16s : CD: %.2f CCD: %.2f CDC: %.2f DMI: %.2f DMA: %.2f DA: %.2f DC: %.2f, ACD: %.2f 1: %.2f 2: %.2f 3: %.2f 4: %.2f 5: %.2f 6: %.2f 7: %.2f 8: %.2f 9: %.2f",
	//            m_GameContext->Server()->ClientName(ClientID),
	//            m_ClosestDistanceToCurrentIDCT[ClientID],
	//            m_ClosestIDToCursorDistanceCT[ClientID],
	//            m_ClosestIDToCursorDistanceCT[ClientID],
	//            m_MinDistanceFromBody[ClientID],
	//            m_MaxDistanceFromBody[ClientID],
	//            m_AvgDistanceFromBody[ClientID],
	//            m_CurrentDistanceFromBody[ClientID],
	//            m_AngleToNearestPlayer[ClientID],
	//            m_AvgCursorToPlayerIfInSightInAreaDistance[1][ClientID],
	//            m_AvgCursorToPlayerIfInSightInAreaDistance[2][ClientID],
	//            m_AvgCursorToPlayerIfInSightInAreaDistance[3][ClientID],
	//            m_AvgCursorToPlayerIfInSightInAreaDistance[4][ClientID],
	//            m_AvgCursorToPlayerIfInSightInAreaDistance[5][ClientID],
	//            m_AvgCursorToPlayerIfInSightInAreaDistance[6][ClientID],
	//            m_AvgCursorToPlayerIfInSightInAreaDistance[7][ClientID],
	//            m_AvgCursorToPlayerIfInSightInAreaDistance[8][ClientID],
	//            m_AvgCursorToPlayerIfInSightInAreaDistance[9][ClientID]
	//           );
	vec2 Pos0 = vec2(m_aPlayerBacklog[ClientID].front().m_Core_X, m_aPlayerBacklog[ClientID].front().m_Core_Y),
	     Pos1 = vec2(m_aPlayerBacklog[1].front().m_Core_X, m_aPlayerBacklog[1].front().m_Core_Y);
	vec2 *Out0 = new vec2, *Out1 = new vec2;


	str_format(aBuf, 512, " %16s : Collision: %d Pos0.x: %.2f Pos0.y: %.2f Pos1.x: %.2f Pos1.y: %.2f Out0.x: %.2f Out0.y: %.2f Out1.x: %.2f Out1.y: %.2f", m_GameContext->Server()->ClientName(ClientID), m_GameContext->Collision()->IntersectLine(Pos0, Pos1, Out0, Out1), Pos0.x, Pos0.y, Pos1.x, Pos1.y, Out0->x, Out0->y, Out1->x, Out1->y);
	// delete Out0;
	// delete Out1;

	return aBuf;
}

void CBotDetection::OnPlayerConnect(int ClientID) {
	ResetID(ClientID);
	ResetLongTimeData(ClientID);
	m_TimeStampJoined[ClientID] = GetTimeStamp();
}

void CBotDetection::OnPlayerDisconnect(int ClientID) {
	InsertIntoPlayerAvgTable(ClientID, m_GameContext->Server()->ClientJoinHash(ClientID),
	                         m_GameContext->Server()->ClientName(ClientID),
	                         m_TimeStampJoined[ClientID],
	                         GetTimeStamp(),
	                         m_AvgCursorAngleToPlayerAngleDifferenceInArea,
	                         m_AvgAngleDifferenceOnShot,
	                         m_AvgAngleDifferenceOnHook,
	                         m_MinCursorAngleToPlayerAngleDifferenceInArea,
	                         m_AvgCursorToPlayerIfInSightInAreaDistance,
	                         m_MinDistanceFromBody[ClientID],
	                         m_MaxDistanceFromBody[ClientID],
	                         m_AvgDistanceFromBody[ClientID]);
	ResetID(ClientID);
	ResetLongTimeData(ClientID);
	free(m_TimeStampJoined[ClientID]);
}

void CBotDetection::ResetID(int ClientID) {

	m_CursorAngleToClosestAngleDiffPlayerInSight[ClientID] = 361;
	m_CursorAngleDifferenceToPlayerInSight[ClientID] = -1;

	m_AngleToNearestPlayer[ClientID] = -1;
	m_CursorAngle[ClientID] = -1;

	m_CurrentDistanceFromBody[ClientID] = 0;
	m_MinDistanceFromBody[ClientID] = std::numeric_limits<double>::max();
	m_MaxDistanceFromBody[ClientID] = 0;

	m_aPlayersCurrentTick[ClientID].m_CoreAvailable = false;
	m_aPlayersCurrentTick[ClientID].m_JoinHash = 0;
	m_aPlayersCurrentTick[ClientID].m_ClientID = -1;
	m_aPlayersCurrentTick[ClientID].m_Core_Tick = -1;
	m_aPlayersCurrentTick[ClientID].m_Core_X = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_Y = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_VelX = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_VelY = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_Angle = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_Direction = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_Jumped = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_HookedPlayer = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_HookState = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_HookTick = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_HookX = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_HookY = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_HookDx = 0;
	m_aPlayersCurrentTick[ClientID].m_Core_HookDy = 0;
	m_aPlayersCurrentTick[ClientID].m_InputAvailable = false;
	m_aPlayersCurrentTick[ClientID].m_Input_Direction = 0;
	m_aPlayersCurrentTick[ClientID].m_Input_TargetX = 0;
	m_aPlayersCurrentTick[ClientID].m_Input_TargetY = 0;
	m_aPlayersCurrentTick[ClientID].m_Input_Jump = -1;
	m_aPlayersCurrentTick[ClientID].m_Input_Fire = -1;
	m_aPlayersCurrentTick[ClientID].m_Input_Hook = -1;
	m_aPlayersCurrentTick[ClientID].m_Input_PlayerFlags = 0;
	m_aPlayersCurrentTick[ClientID].m_Input_WantedWeapon = 0;
	m_aPlayersCurrentTick[ClientID].m_Input_NextWeapon = 0;
	m_aPlayersCurrentTick[ClientID].m_Input_PrevWeapon = 0;

}

void CBotDetection::ResetLongTimeData(int ClientID) {
	m_AvgDistanceSum[ClientID] = 0;
	m_AvgDistanceFromBody[ClientID] = 0;
	m_InputCount[ClientID] = 0;
	m_aPlayersCurrentTick[ClientID].m_JoinTick = m_aPlayersCurrentTick[ClientID].m_Core_Tick;

	for (int i = 0; i < 10; ++i)
	{
		m_AvgCursorAngleToPlayerAngleDifferenceInArea[i][ClientID] = 0;
		m_SumAngleToPlayerDifferenceInArea[i][ClientID] = 0;

		//distances
		m_AvgCursorToPlayerIfInSightInAreaDistance[i][ClientID] = 0;
		m_SumCursorToPlayerIfInSightInAreaDistance[i][ClientID] = 0;

		m_CountPlayerInSightInArea[i][ClientID] = 0;

		m_MinCursorAngleToPlayerAngleDifferenceInArea[i][ClientID] = std::numeric_limits<double>::max();

		// angles on shots
		m_AvgAngleDifferenceOnShot[i][ClientID] = 0;
		m_SumAngleDifferenceOnShot[i][ClientID] = 0;
		m_NumShotsInArea[i][ClientID] = 0;

		m_AvgAngleDifferenceOnHook[i][ClientID] = 0;
		m_SumAngleDifferenceOnHook[i][ClientID] = 0;
		m_NumHooksInArea[i][ClientID] = 0;

	}

}



/// Database stuff ///

void CBotDetection::CreateDatabase(const char* filename) {

	int err = sqlite_open(filename, &m_SqliteDB);

	if (err == SQLITE_OK) {

		CreatePlayerAvgTable();

	} else {
		dbg_msg("SQLiteHistorian", "Error in CreateDatabase");
		m_DbMode = MODE_NONE;
	}
}

void CBotDetection::CreatePlayerAvgTable() {
	if (m_DbMode)
	{
		char* ErrMsg;
		int err = sqlite_exec(m_SqliteDB,
		                      "BEGIN; \
			CREATE TABLE IF NOT EXISTS PlayerAvg( \
				JoinHash INT, \
				NickName VARCHAR(20), \
				TimeStampJoined VARCHAR(25), \
				TimeStampLeft VARCHAR(25), \
				AngleDiffArea1 DOUBLE, \
				AngleDiffArea2 DOUBLE, \
				AngleDiffArea3 DOUBLE, \
				AngleDiffArea4 DOUBLE, \
				AngleDiffArea5 DOUBLE, \
				AngleDiffArea6 DOUBLE, \
				AngleDiffArea7 DOUBLE, \
				AngleDiffArea8 DOUBLE, \
				AngleDiffArea9 DOUBLE, \
				AngleDiffArea10 DOUBLE, \
				AngleDiffOnShotArea1 DOUBLE, \
				AngleDiffOnShotArea2 DOUBLE, \
				AngleDiffOnShotArea3 DOUBLE, \
				AngleDiffOnShotArea4 DOUBLE, \
				AngleDiffOnShotArea5 DOUBLE, \
				AngleDiffOnShotArea6 DOUBLE, \
				AngleDiffOnShotArea7 DOUBLE, \
				AngleDiffOnShotArea8 DOUBLE, \
				AngleDiffOnShotArea9 DOUBLE,	\
				AngleDiffOnShotArea10 DOUBLE, \
				AngleDiffOnHookArea1 DOUBLE, \
				AngleDiffOnHookArea2 DOUBLE, \
				AngleDiffOnHookArea3 DOUBLE, \
				AngleDiffOnHookArea4 DOUBLE, \
				AngleDiffOnHookArea5 DOUBLE, \
				AngleDiffOnHookArea6 DOUBLE, \
				AngleDiffOnHookArea7 DOUBLE, \
				AngleDiffOnHookArea8 DOUBLE, \
				AngleDiffOnHookArea9 DOUBLE,	\
				AngleDiffOnHookArea10 DOUBLE, \
				MinAngleDiffArea1 DOUBLE, \
				MinAngleDiffArea2 DOUBLE, \
				MinAngleDiffArea3 DOUBLE, \
				MinAngleDiffArea4 DOUBLE, \
				MinAngleDiffArea5 DOUBLE, \
				MinAngleDiffArea6 DOUBLE, \
				MinAngleDiffArea7 DOUBLE, \
				MinAngleDiffArea8 DOUBLE, \
				MinAngleDiffArea9 DOUBLE, \
				MinAngleDiffArea10 DOUBLE, \
				CursorDistArea1 DOUBLE, \
				CursorDistArea2 DOUBLE, \
				CursorDistArea3 DOUBLE, \
				CursorDistArea4 DOUBLE, \
				CursorDistArea5 DOUBLE, \
				CursorDistArea6 DOUBLE, \
				CursorDistArea7 DOUBLE, \
				CursorDistArea8 DOUBLE, \
				CursorDistArea9 DOUBLE,	\
				CursorDistArea10 DOUBLE,	\
				MinDistanceFromBody DOUBLE,	\
				MaxDistanceFromBody DOUBLE,	\
				AvgDistanceFromBody DOUBLE	\
			); \
			COMMIT;", &ErrMsg);

		/* check for error */
		if (err != SQLITE_OK) {
			dbg_msg("ERROR SQLITE", "CreatePlayerAvgTable:");
			dbg_msg("BotDetection", "SQL error (#%d): %s\n", err, ErrMsg);
			sqlite_free(ErrMsg);
			m_DbMode = MODE_NONE;
		}

	}

}

void CBotDetection::InsertIntoPlayerAvgTable(int ClientID, int JoinHash, const char* NickName, char *TimeStampJoined, char *TimeStampLeft, double AngleAreas[10][MAX_CLIENTS], double AngleAreasOnShot[10][MAX_CLIENTS], double AngleAreasOnHook[10][MAX_CLIENTS], double MinAngleAreas[10][MAX_CLIENTS], double CursorAreas[10][MAX_CLIENTS], double MinDistanceFromBody, double MaxDistanceFromBody, double AvgDistanceFromBody) {
	/* prepare */
	const char *zTail;
	const char *zSql = "\
		INSERT OR REPLACE INTO PlayerAvg ( \
	            JoinHash,  \
				NickName,  \
				TimeStampJoined,  \
				TimeStampLeft, \
				AngleDiffArea1, \
				AngleDiffArea2, \
				AngleDiffArea3, \
				AngleDiffArea4, \
				AngleDiffArea5, \
				AngleDiffArea6, \
				AngleDiffArea7, \
				AngleDiffArea8, \
				AngleDiffArea9,	\
				AngleDiffArea10, \
				AngleDiffOnShotArea1, \
				AngleDiffOnShotArea2, \
				AngleDiffOnShotArea3, \
				AngleDiffOnShotArea4, \
				AngleDiffOnShotArea5, \
				AngleDiffOnShotArea6, \
				AngleDiffOnShotArea7, \
				AngleDiffOnShotArea8, \
				AngleDiffOnShotArea9,	\
				AngleDiffOnShotArea10, \
				AngleDiffOnHookArea1, \
				AngleDiffOnHookArea2, \
				AngleDiffOnHookArea3, \
				AngleDiffOnHookArea4, \
				AngleDiffOnHookArea5, \
				AngleDiffOnHookArea6, \
				AngleDiffOnHookArea7, \
				AngleDiffOnHookArea8, \
				AngleDiffOnHookArea9,	\
				AngleDiffOnHookArea10, \
				MinAngleDiffArea1, \
				MinAngleDiffArea2, \
				MinAngleDiffArea3, \
				MinAngleDiffArea4, \
				MinAngleDiffArea5, \
				MinAngleDiffArea6, \
				MinAngleDiffArea7, \
				MinAngleDiffArea8, \
				MinAngleDiffArea9, \
				MinAngleDiffArea10, \
				CursorDistArea1, \
				CursorDistArea2, \
				CursorDistArea3, \
				CursorDistArea4, \
				CursorDistArea5, \
				CursorDistArea6, \
				CursorDistArea7, \
				CursorDistArea8, \
				CursorDistArea9, \
				CursorDistArea10, \
				MinDistanceFromBody, \
				MaxDistanceFromBody, \
				AvgDistanceFromBody \
				) \
		VALUES  \
		(?1, \
		trim(?2), \
		?3, \
		?4, \
		?5, \
		?6, \
		?7, \
		?8, \
		?9, \
		?10, \
		?11, \
		?12, \
		?13, \
		?14, \
		?15, \
		?16, \
		?17, \
		?18, \
		?19, \
		?20, \
		?21, \
		?22, \
		?23, \
		?24, \
		?25, \
		?26, \
		?27, \
		?28, \
		?29, \
		?30, \
		?31, \
		?32, \
		?33, \
		?34, \
		?35, \
		?36, \
		?37, \
		?38, \
		?39, \
		?40, \
		?41, \
		?42, \
		?43, \
		?44, \
		?45, \
		?46, \
		?47, \
		?48, \
		?49, \
		?50, \
		?51, \
		?52, \
		?53, \
		?54, \
		?55, \
		?56, \
		?57 \
		) \
		;";
	sqlite3_stmt *pStmt;
	int rc = sqlite_prepare_statement(m_SqliteDB, zSql, &pStmt, &zTail);

	if (rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite_bind_int(pStmt, 1, JoinHash);
		sqlite_bind_text(pStmt, 2, NickName);
		sqlite_bind_text(pStmt, 3, TimeStampJoined);
		sqlite_bind_text(pStmt, 4, TimeStampLeft);

		sqlite_bind_double(pStmt, 5, AngleAreas[1][ClientID]);
		sqlite_bind_double(pStmt, 6, AngleAreas[2][ClientID]);
		sqlite_bind_double(pStmt, 7, AngleAreas[3][ClientID]);
		sqlite_bind_double(pStmt, 8, AngleAreas[4][ClientID]);
		sqlite_bind_double(pStmt, 9, AngleAreas[5][ClientID]);
		sqlite_bind_double(pStmt, 10, AngleAreas[6][ClientID]);
		sqlite_bind_double(pStmt, 11, AngleAreas[7][ClientID]);
		sqlite_bind_double(pStmt, 12, AngleAreas[8][ClientID]);
		sqlite_bind_double(pStmt, 13, AngleAreas[9][ClientID]);
		sqlite_bind_double(pStmt, 14, AngleAreas[0][ClientID]);

		sqlite_bind_double(pStmt, 15, AngleAreasOnShot[1][ClientID]);
		sqlite_bind_double(pStmt, 16, AngleAreasOnShot[2][ClientID]);
		sqlite_bind_double(pStmt, 17, AngleAreasOnShot[3][ClientID]);
		sqlite_bind_double(pStmt, 18, AngleAreasOnShot[4][ClientID]);
		sqlite_bind_double(pStmt, 19, AngleAreasOnShot[5][ClientID]);
		sqlite_bind_double(pStmt, 20, AngleAreasOnShot[6][ClientID]);
		sqlite_bind_double(pStmt, 21, AngleAreasOnShot[7][ClientID]);
		sqlite_bind_double(pStmt, 22, AngleAreasOnShot[8][ClientID]);
		sqlite_bind_double(pStmt, 23, AngleAreasOnShot[9][ClientID]);
		sqlite_bind_double(pStmt, 24, AngleAreasOnShot[0][ClientID]);

		sqlite_bind_double(pStmt, 25, AngleAreasOnHook[1][ClientID]);
		sqlite_bind_double(pStmt, 26, AngleAreasOnHook[2][ClientID]);
		sqlite_bind_double(pStmt, 27, AngleAreasOnHook[3][ClientID]);
		sqlite_bind_double(pStmt, 28, AngleAreasOnHook[4][ClientID]);
		sqlite_bind_double(pStmt, 29, AngleAreasOnHook[5][ClientID]);
		sqlite_bind_double(pStmt, 30, AngleAreasOnHook[6][ClientID]);
		sqlite_bind_double(pStmt, 31, AngleAreasOnHook[7][ClientID]);
		sqlite_bind_double(pStmt, 32, AngleAreasOnHook[8][ClientID]);
		sqlite_bind_double(pStmt, 33, AngleAreasOnHook[9][ClientID]);
		sqlite_bind_double(pStmt, 34, AngleAreasOnHook[0][ClientID]);

		sqlite_bind_double(pStmt, 35, MinAngleAreas[1][ClientID]);
		sqlite_bind_double(pStmt, 36, MinAngleAreas[2][ClientID]);
		sqlite_bind_double(pStmt, 37, MinAngleAreas[3][ClientID]);
		sqlite_bind_double(pStmt, 38, MinAngleAreas[4][ClientID]);
		sqlite_bind_double(pStmt, 39, MinAngleAreas[5][ClientID]);
		sqlite_bind_double(pStmt, 40, MinAngleAreas[6][ClientID]);
		sqlite_bind_double(pStmt, 41, MinAngleAreas[7][ClientID]);
		sqlite_bind_double(pStmt, 42, MinAngleAreas[8][ClientID]);
		sqlite_bind_double(pStmt, 43, MinAngleAreas[9][ClientID]);
		sqlite_bind_double(pStmt, 44, MinAngleAreas[0][ClientID]);

		sqlite_bind_double(pStmt, 45, CursorAreas[1][ClientID]);
		sqlite_bind_double(pStmt, 46, CursorAreas[2][ClientID]);
		sqlite_bind_double(pStmt, 47, CursorAreas[3][ClientID]);
		sqlite_bind_double(pStmt, 48, CursorAreas[4][ClientID]);
		sqlite_bind_double(pStmt, 49, CursorAreas[5][ClientID]);
		sqlite_bind_double(pStmt, 50, CursorAreas[6][ClientID]);
		sqlite_bind_double(pStmt, 51, CursorAreas[7][ClientID]);
		sqlite_bind_double(pStmt, 52, CursorAreas[8][ClientID]);
		sqlite_bind_double(pStmt, 53, CursorAreas[9][ClientID]);
		sqlite_bind_double(pStmt, 54, CursorAreas[0][ClientID]);

		sqlite_bind_double(pStmt, 55, MinDistanceFromBody);
		sqlite_bind_double(pStmt, 56, MaxDistanceFromBody);
		sqlite_bind_double(pStmt, 57, AvgDistanceFromBody);

		/* save to database */
		switch (sqlite3_step(pStmt))
		{
		case SQLITE_DONE:
			/* nothing */
			break;
		case SQLITE_BUSY:
			dbg_msg("BotDetection", "Could not save data, database is busy.");
			break;
		default:
			dbg_msg("BotDetection", "Could not save data: SQL error (#%d): %s", rc, sqlite3_errmsg(m_SqliteDB));

		}

	}
	else
	{
		dbg_msg("BotDetection", "SQL error (#%d): %s", rc, sqlite_errmsg(m_SqliteDB));
	}

	free(TimeStampLeft);
	sqlite_finalize(pStmt);
}

void CBotDetection::CloseDatabase() {
	int err = sqlite_close(m_SqliteDB);
	if (err != SQLITE_OK) {
		dbg_msg("SQLiteHistorian", "Error on closing sqlite database(%li).(%d)", (long)m_SqliteDB , err);
	}
	m_DbMode = MODE_NONE;

}


char* CBotDetection::GetTimeStamp() {
	char aDate[20];

	timeval curTime;
	gettimeofday(&curTime, NULL);
	long milli = curTime.tv_usec / 1000;
	strftime(aDate, 20, "%Y-%m-%d %H:%M:%S", localtime(&curTime.tv_sec));
	char *aBuf = (char*)malloc(25 * sizeof(char));
	str_format(aBuf, 25 * sizeof(char), "%s.%ld", aDate, milli);
	return aBuf;
}









































































































































































































































































































































































