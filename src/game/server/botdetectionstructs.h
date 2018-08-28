#ifndef BOT_DETECTION_STRUCTS
#define BOT_DETECTION_STRUCTS

#include <cmath>
#include <sstream>
#include <string>
#include <vector>
#include <game/generated/protocol.h>


struct TickPlayer
{
	int m_JoinHash; // not used for hash / equality
	int m_ClientID; // not used for hash / equality
	int m_JoinTick{ -1}; // not used for hash / equality
	int m_Tick{ -1};

	bool m_CoreAvailable; // not used for hash / equality
	int m_Core_Tick; // not used for hash / equality

	int m_Core_X;
	int m_Core_Y;

	int m_Core_VelX;
	int m_Core_VelY;
	int m_Core_Angle;
	int m_Core_Direction;
	int m_Core_Jumped;
	int m_Core_HookedPlayer;
	int m_Core_HookState;
	int m_Core_HookTick; // not used for hash / equality
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
	// next ones are object to maybe also not being used
	// for hash / equality checking
	int m_Input_PlayerFlags;
	int m_Input_WantedWeapon;
	int m_Input_NextWeapon;
	int m_Input_PrevWeapon;

	void ResetAllData() {
		m_JoinHash = -1;
		m_ClientID = -1;
		m_JoinTick = -1;
		ResetTickData();
	}
	void ResetTickData() {
		m_Tick = 0;
		m_CoreAvailable = false;
		m_Core_Tick = -1;
		m_Core_X = -1;
		m_Core_Y = -1;
		m_Core_VelX = -1;
		m_Core_VelY = -1;
		m_Core_Angle = -1;
		m_Core_Direction = -2;
		m_Core_Jumped = -1;
		m_Core_HookedPlayer = -1;
		m_Core_HookState = -1;
		m_Core_HookTick = -1;
		m_Core_HookX = -1;
		m_Core_HookY = -1;
		m_Core_HookDx = -1;
		m_Core_HookDy = -1;

		m_InputAvailable = false;
		m_Input_Direction = -2;

		m_Input_TargetX = 0;
		m_Input_TargetY = 0;

		m_Input_Jump = -1;
		m_Input_Fire = -1;
		m_Input_Hook = -1;
		m_Input_PlayerFlags = -1;
		m_Input_WantedWeapon = -1;
		m_Input_NextWeapon = -1;
		m_Input_PrevWeapon = -1;
	}
	void Init(int ClientID, int JoinHash, int JoinTick) {
		m_ClientID = ClientID;
		m_JoinHash = JoinHash;
		m_JoinTick = JoinTick;
	}
	void FillCore(CNetObj_CharacterCore *PlayerCore) {
		if (!PlayerCore)
		{
			return;
		}
		m_CoreAvailable = true;
		m_Core_Tick = PlayerCore->m_Tick;
		m_Core_X = PlayerCore->m_X;
		m_Core_Y = PlayerCore->m_Y;
		m_Core_VelX = PlayerCore->m_VelX;
		m_Core_VelY = PlayerCore->m_VelY;
		m_Core_Angle = PlayerCore->m_Angle;
		m_Core_Direction = PlayerCore->m_Direction;
		m_Core_Jumped = PlayerCore->m_Jumped;
		m_Core_HookedPlayer = PlayerCore->m_HookedPlayer;
		m_Core_HookState = PlayerCore->m_HookState;
		m_Core_HookTick = PlayerCore->m_HookTick;
		m_Core_HookX = PlayerCore->m_HookX;
		m_Core_HookY = PlayerCore->m_HookY;
		m_Core_HookDx = PlayerCore->m_HookDx;
		m_Core_HookDy = PlayerCore->m_HookDy;
	}

	void SetTick(int tick) {
		m_Tick = tick;
	}
	void FillInput(CNetObj_PlayerInput *PlayerInput) {
		if (!PlayerInput) {
			return;
		}
		m_InputAvailable = true;
		m_Input_Direction = PlayerInput->m_Direction;
		m_Input_TargetX = PlayerInput->m_TargetX;
		m_Input_TargetY = PlayerInput->m_TargetY;
		m_Input_Jump = PlayerInput->m_Jump;
		m_Input_Fire = PlayerInput->m_Fire;
		m_Input_Hook = PlayerInput->m_Hook;
		m_Input_PlayerFlags = PlayerInput->m_PlayerFlags;
		m_Input_WantedWeapon = PlayerInput->m_WantedWeapon;
		m_Input_NextWeapon = PlayerInput->m_NextWeapon;
		m_Input_PrevWeapon = PlayerInput->m_PrevWeapon;
	}
	bool IsFull() {
		return m_CoreAvailable && m_InputAvailable;
	}

	bool IsEmpty() {
		return !m_CoreAvailable && !m_InputAvailable;
	}

	std::vector<std::string> toStringVector() {
		std::vector<std::string> ret;
		std::stringstream s;
		s << "m_Core_Tick = " << m_Core_Tick;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_X = " << m_Core_X;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_Y = " << m_Core_Y;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_VelX = " << m_Core_VelX;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_VelY = " << m_Core_VelY;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_Angle = " << m_Core_Angle;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_Direction = " << m_Core_Direction;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_Jumped = " << m_Core_Jumped;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_HookedPlayer = " << m_Core_HookedPlayer;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_HookState = " << m_Core_HookState;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_HookTick = " << m_Core_HookTick;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_HookX = " << m_Core_HookX;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_HookY = " << m_Core_HookY;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_HookDx = " << m_Core_HookDx;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Core_HookDy = " << m_Core_HookDy;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Input_Direction = " << m_Input_Direction;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Input_TargetX = " << m_Input_TargetX;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Input_TargetY = " << m_Input_TargetY;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Input_Jump = " << m_Input_Jump;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Input_Fire = " << m_Input_Fire;
		ret.push_back(s.str());
		s.str(std::string());
		s << "m_Input_Hook = " << m_Input_Hook;
		ret.push_back(s.str());
		s.str(std::string());
		return ret;
	}

	const std::string toString() {
		std::vector<std::string> v = toStringVector();
		std::stringstream s;
		s << "\n";
		for (size_t i = 0; i < v.size(); ++i)
		{
			s << v.at(i) << "\n";
		}
		return s.str();
	}

	bool operator==(const TickPlayer& other) {
		if ( !m_CoreAvailable || !other.m_CoreAvailable || !m_InputAvailable || !other.m_InputAvailable)
		{
			return false;
		}
		return m_Core_X == other.m_Core_X &&
		       m_Core_Y == other.m_Core_Y &&
		       m_Core_VelX == other.m_Core_VelX &&
		       m_Core_VelY == other.m_Core_VelY &&
		       m_Core_Angle == other.m_Core_Angle &&
		       m_Core_Direction == other.m_Core_Direction &&
		       m_Core_Jumped == other.m_Core_Jumped &&
		       m_Core_HookedPlayer == other.m_Core_HookedPlayer &&
		       m_Core_HookState == other.m_Core_HookState &&
		       m_Core_HookX == other.m_Core_HookX &&
		       m_Core_HookY == other.m_Core_HookY &&
		       m_Core_HookDx == other.m_Core_HookDx &&
		       m_Core_HookDy == other.m_Core_HookDy &&
		       m_Input_Direction == other.m_Input_Direction &&
		       m_Input_TargetX == other.m_Input_TargetX &&
		       m_Input_TargetY == other.m_Input_TargetY &&
		       m_Input_Jump == other.m_Input_Jump &&
		       m_Input_Fire == other.m_Input_Fire &&
		       m_Input_Hook == other.m_Input_Hook &&
		       m_Input_WantedWeapon == other.m_Input_WantedWeapon &&
		       m_Input_NextWeapon == other.m_Input_NextWeapon &&
		       m_Input_PrevWeapon == other.m_Input_PrevWeapon;
	}
	bool equalInput(const TickPlayer &other) {
		return m_Input_TargetX == other.m_Input_TargetX &&
		       m_Input_TargetY == other.m_Input_TargetY &&
		       m_Input_Jump == other.m_Input_Jump &&
		       m_Input_Fire == other.m_Input_Fire &&
		       m_Input_Hook == other.m_Input_Hook &&
		       m_Input_WantedWeapon == other.m_Input_WantedWeapon &&
		       m_Input_NextWeapon == other.m_Input_NextWeapon &&
		       m_Input_PrevWeapon == other.m_Input_PrevWeapon;
	}

	bool equalCore(const TickPlayer &other) {
		return m_Core_X == other.m_Core_X &&
		       m_Core_Y == other.m_Core_Y &&
		       m_Core_VelX == other.m_Core_VelX &&
		       m_Core_VelY == other.m_Core_VelY &&
		       m_Core_Angle == other.m_Core_Angle &&
		       m_Core_Direction == other.m_Core_Direction &&
		       m_Core_Jumped == other.m_Core_Jumped &&
		       m_Core_HookedPlayer == other.m_Core_HookedPlayer &&
		       m_Core_HookState == other.m_Core_HookState &&
		       m_Core_HookX == other.m_Core_HookX &&
		       m_Core_HookY == other.m_Core_HookY &&
		       m_Core_HookDx == other.m_Core_HookDx &&
		       m_Core_HookDy == other.m_Core_HookDy;
	}
	bool operator!=(TickPlayer& other) {
		return !( *this == other);
	}
	bool operator <(const TickPlayer& other) const
	{
		return m_Tick < other.m_Tick;
	}
};

namespace std {

template <>
struct hash<TickPlayer>
{
	std::size_t operator()(const TickPlayer& k) const
	{
		using std::size_t;
		using std::hash;
		using std::string;

		// Compute individual hash values for first,
		// second and third and combine them using XOR
		// and bit shifting:
		// TODO:: implement hashing - create a set with unique inputs!!!!!!!!!!!!!!!!!!!!!
		long ret = (k.m_Core_X << 2)
		           ^ (k.m_Core_Y << 3) * 3
		           ^ (k.m_Core_VelX << 4) * 5
		           ^ (k.m_Core_VelY << 5) * 7
		           ^ (k.m_Core_Angle << 6) * 11
		           ^ (k.m_Core_Direction << 5) * 13
		           ^ (k.m_Core_Jumped << 7) * 17
		           ^ (k.m_Core_HookedPlayer << 8) * 23
		           ^ (k.m_Core_HookState << 9) * 29
		           ^ (k.m_Core_HookX << 10) * 31
		           ^ (k.m_Core_HookY << 11) * 37
		           ^ (k.m_Core_HookDx << 12) * 41
		           ^ (k.m_Core_HookDy << 13) * 43
		           ^ (k.m_Input_Direction << 14) * 1879
		           ^ (k.m_Input_TargetX << 15) * 47
		           ^ (k.m_Input_TargetY << 16) * 53
		           ^ (k.m_Input_Jump << 17) * 2593
		           ^ (k.m_Input_Fire << 18) * 2999
		           ^ (k.m_Input_Hook << 19) * 3989
		           ^ (k.m_Input_WantedWeapon << 21) * 57
		           ^ (k.m_Input_NextWeapon << 22) * 61
		           ^ (k.m_Input_PrevWeapon << 23) * 67;
		return abs(ret);
	}
};

}


struct DataPlayer
{

};


#endif