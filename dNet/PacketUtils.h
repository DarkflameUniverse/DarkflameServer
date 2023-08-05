#ifndef PACKETUTILS_H
#define PACKETUTILS_H

#include <string>
#include <cstdint>
#include "RakNetTypes.h"


namespace PacketUtils {
	void SavePacket(const std::string& filename, const char* data, size_t length);
};

#endif // PACKETUTILS_H
