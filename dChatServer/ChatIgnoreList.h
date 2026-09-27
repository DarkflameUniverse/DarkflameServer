#ifndef __CHATIGNORELIST__H__
#define __CHATIGNORELIST__H__

struct SystemAddress;

#include <cstdint>

#include "ChatPackets.h"

/**
 * @brief The ignore list allows players to ignore someone silently. Requests will generally be blocked by the client, but they should also be checked
 * on the server as well so the sender can get a generic error code in response.
 *
 */
namespace ChatIgnoreList {
	void GetIgnoreList(const ChatPackets::GetIgnoreList& request, const SystemAddress& sysAddr);
	void AddIgnore(const ChatPackets::AddIgnore& request, const SystemAddress& sysAddr);
	void RemoveIgnore(const ChatPackets::RemoveIgnore& request, const SystemAddress& sysAddr);
};

#endif  //!__CHATIGNORELIST__H__
