#ifndef BOT_DETECTION_H
#define BOT_DETECTION_H

#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "botdetectionstructs.h"
#include <queue>
#include <base/sqlite.h>
#include <sys/time.h>

class CBotDetection
{
public:
	CBotDetection(CGameContext * GameServer);
	~CBotDetection();

	void AddPlayerCore(int ClientID, int JoinHash, CNetObj_CharacterCore *PlayerCore);
	void AddPlayerInput(int ClientID, int JoinHash, CNetObj_PlayerInput *PlayerInput);
	void OnTick();
	void OnPlayerConnect(int ClientID);
	void OnPlayerDisconnect(int ClientID);

	void ManageLastInputQueue();
	void CalculatePlayerRelations();
	void ResetCurrentTick();
	void ResetAverages(int ClientID);
	char* GetInfoString(int ClientID);


private:
	sqlite3 *m_SqliteDB;

	CGameContext *m_GameContext;
	TickPlayer m_aPlayersCurrentTick[MAX_CLIENTS];
	char *m_TimeStampJoined[MAX_CLIENTS];

	std::queue<TickPlayer> m_aPlayerBacklog[MAX_CLIENTS];
	std::queue<TickPlayer> m_aPlayerInputBacklog[MAX_CLIENTS];
	std::queue<TickPlayer> m_aPlayerCoreBacklog[MAX_CLIENTS];
	// CT = current tick
	double m_PlayerDistanceCT[MAX_CLIENTS][MAX_CLIENTS];

	// player distances
	int m_ClosestIDToCurrentIDCT[MAX_CLIENTS];
	double m_ClosestDistanceToCurrentIDCT[MAX_CLIENTS];


	//cursor player distances
	// cursor distance to closest player in regard to own position, not cursor position
	int m_CursorToClosestDistanceIDCT[MAX_CLIENTS];

	// cursor position to closest player position
	int m_ClosestIDToCursorCT[MAX_CLIENTS];

	// actual distance  of cursor to above defined id.
	double m_ClosestIDToCursorDistanceCT[MAX_CLIENTS];

	// more or less mouse distances
	double m_CurrentDistanceFromBody[MAX_CLIENTS];
	double m_MinDistanceFromBody[MAX_CLIENTS];
	double m_MaxDistanceFromBody[MAX_CLIENTS];


	// averages
	double m_AvgDistanceSum[MAX_CLIENTS];
	double m_AvgDistanceFromBody[MAX_CLIENTS];
	int m_InputCount[MAX_CLIENTS];



	double m_AngleToNearestPlayer[MAX_CLIENTS];
	double m_CursorAngle[MAX_CLIENTS];


	double Angle(int x, int y, int x2, int y2);
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
	 *	Not in Sight: 0
	 */

	int m_CursorAngleToClosestAngleDiffPlayerInSight[MAX_CLIENTS];
	double m_CursorAngleDifferenceToPlayerInSight[MAX_CLIENTS];

	// averages:
	// angles
	double m_AvgCursorAngleToPlayerAngleDifferenceInArea[10][MAX_CLIENTS];
	double m_SumAngleToPlayerDifferenceInArea[10][MAX_CLIENTS];

	//distances
	double m_AvgCursorToPlayerIfInSightInAreaDistance[10][MAX_CLIENTS];
	double m_SumCursorToPlayerIfInSightInAreaDistance[10][MAX_CLIENTS];

	int m_CountPlayerInSightInArea[10][MAX_CLIENTS];



	int EnemyInSight(int ClientID, int EnemyID);

	// TODO: Cursor traveled distance after player is in sight

	static void SetCore(TickPlayer *Target, TickPlayer *Source);
	static void SetInput(TickPlayer *Target, TickPlayer *Source);
	static double Distance(int x, int y, int x2, int y2);
	void ResetID(int ClientID);


	//Database stuff
	int m_DbMode;
	enum
	{
		MODE_NONE = 0,
		MODE_SQLITE,
	};

	void CreateDatabase(const char* filename);
	void CloseDatabase();
	void CreatePlayerAvgTable();
	void InsertIntoPlayerAvgTable(int ClientID, int JoinHash, const char* NickName, char *TimeStampJoined, char *TimeStampLeft, double AngleAreas[10][MAX_CLIENTS], double CursorAreas[10][MAX_CLIENTS], double MinDistanceFromBody, double MaxDistanceFromBody, double AvgDistanceFromBody);
	char* GetTimeStamp();

};




































































































































































































































































































































































































#endif
