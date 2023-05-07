#ifndef PACKETUTILS_H
#define PACKETUTILS_H

#include <MessageIdentifiers.h>
#include <BitStream.h>
#include <string>

enum class eConnectionType : uint16_t;

namespace PacketUtils {
	template<typename T>
	void WriteHeader(RakNet::BitStream& bitStream, eConnectionType connectionType, T internalPacketID) {
		bitStream.Write<uint8_t>(MessageID(ID_USER_PACKET_ENUM));
		bitStream.Write<eConnectionType>(connectionType);
		bitStream.Write<uint32_t>(static_cast<uint32_t>(internalPacketID));
		bitStream.Write<uint8_t>(0);
	}

	uint16_t ReadPacketU16(uint32_t startLoc, Packet* packet);
	uint32_t ReadPacketU32(uint32_t startLoc, Packet* packet);
	uint64_t ReadPacketU64(uint32_t startLoc, Packet* packet);
	int64_t ReadPacketS64(uint32_t startLoc, Packet* packet);
	std::string ReadString(uint32_t startLoc, Packet* packet, bool wide, uint32_t maxLen = 33);

	void WritePacketString(const std::string& string, uint32_t maxSize, RakNet::BitStream* bitStream);
	void WriteString(RakNet::BitStream& bitStream, const std::string& s, uint32_t maxSize);
	void WriteWString(RakNet::BitStream& bitStream, const std::string& string, uint32_t maxSize);
	void WriteWString(RakNet::BitStream& bitStream, const std::u16string& string, uint32_t maxSize);
	void WritePacketWString(const std::string& string, uint32_t maxSize, RakNet::BitStream* bitStream);

	void SavePacket(const std::string& filename, const char* data, size_t length);
};

#endif // PACKETUTILS_H
