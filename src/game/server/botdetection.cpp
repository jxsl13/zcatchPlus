#include "botdetection.h"
#include <cstdlib>
#include <limits>
#include <cmath>






CBotDetection::CBotDetection(CGameContext *GameServer)
{
	m_GameContext = GameServer;
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		ResetID(i);
		ResetAverages(i);
	}
}

CBotDetection::~CBotDetection() {

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

			// cursor distances from body
			m_InputCount[i]++;
			m_CurrentDistanceFromBody[i] = Distance(0, 0, m_aPlayersCurrentTick[i].m_Input_TargetX, m_aPlayersCurrentTick[i].m_Input_TargetY);
			m_AvgDistanceSum[i] += m_CurrentDistanceFromBody[i];
			m_AvgDistanceFromBody[i] = m_InputCount[i] / m_AvgDistanceSum[i];
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
				if (m_GameContext->m_apPlayers[j])
				{
					if (i != j)
					{
						// ID j's positions
						PosX2 = m_aPlayersCurrentTick[j].m_Core_X;
						PosY2 = m_aPlayersCurrentTick[j].m_Core_Y;
						// player distance
						PosDistance = Distance(PosX, PosY, PosX2, PosY2);
						// cursor of i to player j distance
						MyCursorToPosDistance = Distance(CursorPosX, CursorPosY, PosX2, PosY2);


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
						m_AngleToNearestPlayer[i] = Angle(PosX, PosY, PosX2, PosY2);
					}

					if (MyCursorToPosDistance < m_ClosestIDToCursorDistanceCT[i])
					{
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
							m_SumCursorToPlayerIfInSightInAreaDistance[1][i] += diff;;
							m_CountPlayerInSightInArea[1][i]++;
							m_AvgCursorToPlayerIfInSightInAreaDistance[1][i] = m_SumCursorToPlayerIfInSightInAreaDistance[1][i] / m_CountPlayerInSightInArea[1][i]++;
							break;
						case 2:
							m_SumCursorToPlayerIfInSightInAreaDistance[2][i] += diff;;
							m_CountPlayerInSightInArea[2][i]++;
							m_AvgCursorToPlayerIfInSightInAreaDistance[2][i] = m_SumCursorToPlayerIfInSightInAreaDistance[2][i] / m_CountPlayerInSightInArea[2][i]++;
							break;
						case 3:
							m_SumCursorToPlayerIfInSightInAreaDistance[3][i] += diff;;
							m_CountPlayerInSightInArea[3][i]++;
							m_AvgCursorToPlayerIfInSightInAreaDistance[3][i] = m_SumCursorToPlayerIfInSightInAreaDistance[3][i] / m_CountPlayerInSightInArea[3][i]++;
							break;
						case 4:
							m_SumCursorToPlayerIfInSightInAreaDistance[4][i] += diff;;
							m_CountPlayerInSightInArea[4][i]++;
							m_AvgCursorToPlayerIfInSightInAreaDistance[4][i] = m_SumCursorToPlayerIfInSightInAreaDistance[4][i] / m_CountPlayerInSightInArea[4][i]++;
							break;
						case 5:
							m_SumCursorToPlayerIfInSightInAreaDistance[5][i] += diff;;
							m_CountPlayerInSightInArea[5][i]++;
							m_AvgCursorToPlayerIfInSightInAreaDistance[5][i] = m_SumCursorToPlayerIfInSightInAreaDistance[5][i] / m_CountPlayerInSightInArea[5][i]++;
							break;
						case 6:
							m_SumCursorToPlayerIfInSightInAreaDistance[6][i] += diff;;
							m_CountPlayerInSightInArea[6][i]++;
							m_AvgCursorToPlayerIfInSightInAreaDistance[6][i] = m_SumCursorToPlayerIfInSightInAreaDistance[6][i] / m_CountPlayerInSightInArea[6][i]++;
							break;
						case 7:
							m_SumCursorToPlayerIfInSightInAreaDistance[7][i] += diff;;
							m_CountPlayerInSightInArea[7][i]++;
							m_AvgCursorToPlayerIfInSightInAreaDistance[7][i] = m_SumCursorToPlayerIfInSightInAreaDistance[7][i] / m_CountPlayerInSightInArea[7][i]++;
							break;
						case 8:
							m_SumCursorToPlayerIfInSightInAreaDistance[8][i] += diff;;
							m_CountPlayerInSightInArea[8][i]++;
							m_AvgCursorToPlayerIfInSightInAreaDistance[8][i] = m_SumCursorToPlayerIfInSightInAreaDistance[8][i] / m_CountPlayerInSightInArea[8][i]++;
							break;
						case 9:
							m_SumCursorToPlayerIfInSightInAreaDistance[9][i] += diff;;
							m_CountPlayerInSightInArea[9][i]++;
							m_AvgCursorToPlayerIfInSightInAreaDistance[9][i] = m_SumCursorToPlayerIfInSightInAreaDistance[9][i] / m_CountPlayerInSightInArea[9][i]++;
							break;
						default: break;
						}

					} else {
						m_CursorAngleToClosestAngleDiffPlayerInSight[i] = -1;
						m_CursorAngleDifferenceToPlayerInSight[i] = 361;
					}


				}


			}
			// check if only you are online
			if (m_ClosestDistanceToCurrentIDCT[i] == std::numeric_limits<double>::max())
			{
				m_ClosestDistanceToCurrentIDCT[i] = -1;
				m_ClosestIDToCurrentIDCT[i] = -1;
				m_AngleToNearestPlayer[i] = -1;

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
	str_format(aBuf, 512, " %16s : CD: %.2f CCD: %.2f CDC: %.2f DMI: %.2f DMA: %.2f DA: %.2f DC: %.2f, ACD: %.2f 1: %.2f 2: %.2f 3: %.2f 4: %.2f 5: %.2f 6: %.2f 7: %.2f 8: %.2f 9: %.2f",
	           m_GameContext->Server()->ClientName(ClientID),
	           m_ClosestDistanceToCurrentIDCT[ClientID],
	           m_ClosestIDToCursorDistanceCT[ClientID],
	           m_ClosestIDToCursorDistanceCT[ClientID],
	           m_MinDistanceFromBody[ClientID],
	           m_MaxDistanceFromBody[ClientID],
	           m_AvgDistanceFromBody[ClientID],
	           m_CurrentDistanceFromBody[ClientID],
	           m_AngleToNearestPlayer[ClientID],
	           m_AvgCursorToPlayerIfInSightInAreaDistance[1][ClientID],
	           m_AvgCursorToPlayerIfInSightInAreaDistance[2][ClientID],
	           m_AvgCursorToPlayerIfInSightInAreaDistance[3][ClientID],
	           m_AvgCursorToPlayerIfInSightInAreaDistance[4][ClientID],
	           m_AvgCursorToPlayerIfInSightInAreaDistance[5][ClientID],
	           m_AvgCursorToPlayerIfInSightInAreaDistance[6][ClientID],
	           m_AvgCursorToPlayerIfInSightInAreaDistance[7][ClientID],
	           m_AvgCursorToPlayerIfInSightInAreaDistance[8][ClientID],
	           m_AvgCursorToPlayerIfInSightInAreaDistance[9][ClientID]
	           );

	return aBuf;
}

void CBotDetection::OnPlayerConnect(int ClientID) {
	ResetID(ClientID);
	ResetAverages(ClientID);

}

void CBotDetection::OnPlayerDisconnect(int ClientID) {
	ResetID(ClientID);
	ResetAverages(ClientID);
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

void CBotDetection::ResetAverages(int ClientID) {
	m_AvgDistanceSum[ClientID] = 0;
	m_AvgDistanceFromBody[ClientID] = 0;
	m_InputCount[ClientID] = 0;

	for (int i = 0; i < 10; ++i)
	{
		m_AvgCursorToPlayerIfInSightInAreaDistance[i][ClientID] = 0;
		m_SumCursorToPlayerIfInSightInAreaDistance[i][ClientID] = 0;
		m_CountPlayerInSightInArea[i][ClientID] = 0;
	}

}









































































































































































































































































































































































