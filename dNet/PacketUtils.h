#ifndef PACKETUTILS_H
#define PACKETUTILS_H

#include <MessageIdentifiers.h>
#include <BitStream.h>
#include <string>

enum class eConnectionType : uint16_t;

namespace PacketUtils {
	uint16_t ReadU16(uint32_t startLoc, Packet* packet);
	uint32_t ReadU32(uint32_t startLoc, Packet* packet);
	uint64_t ReadU64(uint32_t startLoc, Packet* packet);
	int64_t ReadS64(uint32_t startLoc, Packet* packet);
	std::string ReadString(uint32_t startLoc, Packet* packet, bool wide, uint32_t maxLen = 33);
	void SavePacket(const std::string& filename, const char* data, size_t length);
};

#endif // PACKETUTILS_H
