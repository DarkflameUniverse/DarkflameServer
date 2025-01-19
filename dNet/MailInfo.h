#ifndef __MAILINFO_H__
#define __MAILINFO_H__

#include <string>
#include <cstdint>
#include "dCommonVars.h"

namespace RakNet {
	class BitStream;
}

struct MailInfo {
	std::string senderUsername;
	std::string recipient;
	std::string subject;
	std::string body;
	uint64_t id{};
	uint32_t senderId{};
	uint32_t receiverId{};
	uint64_t timeSent{};
	bool wasRead{};
	struct {
		LWOOBJID itemID{};
		int32_t itemCount{};
		LOT itemLOT{};
		LWOOBJID itemSubkey{};
	};

	void Serialize(RakNet::BitStream& bitStream) const {}
	bool Deserialize(RakNet::BitStream& bitStream) { return true; }
};

#endif // __MAILINFO_H__
