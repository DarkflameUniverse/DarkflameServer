#ifndef __POSITIONUPDATE__H__
#define __POSITIONUPDATE__H__

#include "NiPoint3.h"
#include "NiQuaternion.h"


struct RemoteInputInfo {
	RemoteInputInfo() {
		m_RemoteInputX = 0;
		m_RemoteInputY = 0;
		m_IsPowersliding = false;
		m_IsModified = false;
	}

	void operator=(const RemoteInputInfo& other) {
		m_RemoteInputX = other.m_RemoteInputX;
		m_RemoteInputY = other.m_RemoteInputY;
		m_IsPowersliding = other.m_IsPowersliding;
		m_IsModified = other.m_IsModified;
	}

	bool operator==(const RemoteInputInfo& other) {
		return m_RemoteInputX == other.m_RemoteInputX && m_RemoteInputY == other.m_RemoteInputY && m_IsPowersliding == other.m_IsPowersliding && m_IsModified == other.m_IsModified;
	}

	float m_RemoteInputX;
	float m_RemoteInputY;
	bool m_IsPowersliding;
	bool m_IsModified;
};

struct LocalSpaceInfo {
	LWOOBJID objectId = LWOOBJID_EMPTY;
	NiPoint3 position = NiPoint3Constant::ZERO;
	NiPoint3 linearVelocity = NiPoint3Constant::ZERO;
};

struct PositionUpdate {
	NiPoint3 position = NiPoint3Constant::ZERO;
	NiQuaternion rotation = NiQuaternionConstant::IDENTITY;
	bool onGround = false;
	bool onRail = false;
	NiPoint3 velocity = NiPoint3Constant::ZERO;
	NiPoint3 angularVelocity = NiPoint3Constant::ZERO;
	LocalSpaceInfo localSpaceInfo;
	RemoteInputInfo remoteInputInfo;
};

#endif  //!__POSITIONUPDATE__H__
