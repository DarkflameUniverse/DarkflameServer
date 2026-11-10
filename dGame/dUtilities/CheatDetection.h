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

	/**
	 * @brief Whether an object is the player at a system address or the object that player possesses, such as their vehicle.
	 * Unlike VerifyLwoobjidIsSender this is false for every other object, so it is for messages only a player sends about themselves.
	 *
	 * @param id The object ID a message was sent for
	 * @param sysAddr The system address which sent the message
	 */
	bool IsSendersPlayer(const LWOOBJID& id, const SystemAddress& sysAddr);
};

#endif  //!__CHEATDETECTION__H__

