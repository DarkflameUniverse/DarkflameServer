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
	LWOOBJID senderId{};
	LWOOBJID receiverId{};
	uint64_t timeSent{};
	bool wasRead{};
	uint16_t languageCode{};
	struct {
		LWOOBJID itemID{};
		int16_t itemCount{};
		LOT itemLOT{};
		LWOOBJID itemSubkey{};
	};

	void Serialize(RakNet::BitStream& bitStream) const;
	bool Deserialize(RakNet::BitStream& bitStream);
};

#endif // __MAILINFO_H__
