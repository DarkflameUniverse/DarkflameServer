#ifndef __CHEATDETECTION__H__
#define __CHEATDETECTION__H__

#include "dCommonVars.h"

struct SystemAddress;

enum class CheckType : uint8_t {
	User,
	Entity,
};

namespace CheatDetection {
	/**
	 * @brief Verify that the object ID provided in this function is in someway connected to the system address who sent it.
	 * 
	 * @param id The object ID to check ownership of
	 * @param sysAddr The system address which sent the packet
	 * @param checkType The check type to perform
	 * @param messageIfNotSender The message to log if the sender is not the owner of the object ID
	 * @param ... format args
	 * @return true If the sender is the owner of the object ID
	 * @return false If the sender is not the owner of the object ID
	 */
	bool VerifyLwoobjidIsSender(const LWOOBJID& id, const SystemAddress& sysAddr, const CheckType checkType, const char* messageIfNotSender, ...);
	void ReportCheat(User* user, const SystemAddress& sysAddr, const char* messageIfNotSender, ...);
};

#endif  //!__CHEATDETECTION__H__

