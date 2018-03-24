#ifndef BOT_DETECTION_STRUCTS
#define BOT_DETECTION_STRUCTS



struct TickPlayer
{
	int m_JoinHash;
	int m_ClientID;

	bool m_CoreAvailable;
	int m_Core_Tick;

	int m_Core_X;
	int m_Core_Y;

	int m_Core_VelX;
	int m_Core_VelY;
	int m_Core_Angle;
	int m_Core_Direction;
	int m_Core_Jumped;
	int m_Core_HookedPlayer;
	int m_Core_HookState;
	int m_Core_HookTick;
	int m_Core_HookX;
	int m_Core_HookY;
	int m_Core_HookDx;
	int m_Core_HookDy;

	bool m_InputAvailable;
	int m_Input_Direction;

	int m_Input_TargetX;
	int m_Input_TargetY;

	int m_Input_Jump;
	int m_Input_Fire;
	int m_Input_Hook;
	int m_Input_PlayerFlags;
	int m_Input_WantedWeapon;
	int m_Input_NextWeapon;
	int m_Input_PrevWeapon;

};


struct DataPlayer
{

};


#endif