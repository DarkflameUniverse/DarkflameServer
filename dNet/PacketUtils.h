#ifndef PACKETUTILS_H
#define PACKETUTILS_H

#include "MessageIdentifiers.h"
#include "BitStream.h"
#include <string>

enum class eConnectionType : uint16_t;

namespace PacketUtils {
	void SavePacket(const std::string& filename, const char* data, size_t length);
};

#endif // PACKETUTILS_H
