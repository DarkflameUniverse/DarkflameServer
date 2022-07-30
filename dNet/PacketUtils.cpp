#include "PacketUtils.h"
#include <MessageIdentifiers.h>
#include <vector>
#include <fstream>
#include "dLogger.h"
#include "Game.h"

void PacketUtils::WriteHeader(RakNet::BitStream& bitStream, uint16_t connectionType, uint32_t internalPacketID) {
	bitStream.Write(MessageID(ID_USER_PACKET_ENUM));
	bitStream.Write(connectionType);
	bitStream.Write(internalPacketID);
	bitStream.Write(uint8_t(0));
}

uint16_t PacketUtils::ReadPacketU16(uint32_t startLoc, Packet* packet) {
	if (startLoc + 2 > packet->length) return 0;

	std::vector<unsigned char> t;
	for (uint32_t i = startLoc; i < startLoc + 2; i++) t.push_back(packet->data[i]);
	return *(uint16_t*)t.data();
}

uint32_t PacketUtils::ReadPacketU32(uint32_t startLoc, Packet* packet) {
	if (startLoc + 4 > packet->length) return 0;

	std::vector<unsigned char> t;
	for (uint32_t i = startLoc; i < startLoc + 4; i++) {
		t.push_back(packet->data[i]);
	}
	return *(uint32_t*)t.data();
}

uint64_t PacketUtils::ReadPacketU64(uint32_t startLoc, Packet* packet) {
	if (startLoc + 8 > packet->length) return 0;

	std::vector<unsigned char> t;
	for (uint32_t i = startLoc; i < startLoc + 8; i++) t.push_back(packet->data[i]);
	return *(uint64_t*)t.data();
}

int64_t PacketUtils::ReadPacketS64(uint32_t startLoc, Packet* packet) {
	if (startLoc + 8 > packet->length) return 0;

	std::vector<unsigned char> t;
	for (size_t i = startLoc; i < startLoc + 8; i++) t.push_back(packet->data[i]);
	return *(int64_t*)t.data();
}

std::string PacketUtils::ReadString(uint32_t startLoc, Packet* packet, bool wide, uint32_t maxLen) {
	std::string readString = "";

	if (wide) maxLen *= 2;

	if (packet->length > startLoc) {
		uint32_t i = 0;
		while (packet->data[startLoc + i] != '\0' && packet->length > (uint32_t)(startLoc + i) && maxLen > i) {
			readString.push_back(packet->data[startLoc + i]);

			if (wide) {
				i += 2;     // Wide-char string
			} else {
				i++;        // Regular string
			}
		}
	}

	return readString;
}

void PacketUtils::WritePacketString(const std::string& string, uint32_t maxSize, RakNet::BitStream* bitStream) {
	uint32_t size = static_cast<uint32_t>(string.size());
	uint32_t remSize = static_cast<uint32_t>(maxSize - size);

	if (size > maxSize) size = maxSize;

	for (uint32_t i = 0; i < size; ++i) {
		bitStream->Write(static_cast<char>(string[i]));
	}

	for (uint32_t j = 0; j < remSize; ++j) {
		bitStream->Write(static_cast<char>(0));
	}
}

void PacketUtils::WriteString(RakNet::BitStream& bitStream, const std::string& s, uint32_t maxSize) {
	uint32_t size = s.size();
	uint32_t emptySize = maxSize - size;

	if (size > maxSize) size = maxSize;

	for (uint32_t i = 0; i < size; i++) {
		bitStream.Write((char)s[i]);
	}

	for (uint32_t i = 0; i < emptySize; i++) {
		bitStream.Write((char)0);
	}
}

void PacketUtils::WriteWString(RakNet::BitStream& bitStream, const std::string& string, uint32_t maxSize) {
	uint32_t size = static_cast<uint32_t>(string.length());
	uint32_t remSize = static_cast<uint32_t>(maxSize - size);

	if (size > maxSize) size = maxSize;

	for (uint32_t i = 0; i < size; ++i) {
		bitStream.Write(static_cast<uint16_t>(string[i]));
	}

	for (uint32_t j = 0; j < remSize; ++j) {
		bitStream.Write(static_cast<uint16_t>(0));
	}
}

void PacketUtils::WriteWString(RakNet::BitStream& bitStream, const std::u16string& string, uint32_t maxSize) {
	uint32_t size = static_cast<uint32_t>(string.length());
	uint32_t remSize = static_cast<uint32_t>(maxSize - size);

	if (size > maxSize) size = maxSize;

	for (uint32_t i = 0; i < size; ++i) {
		bitStream.Write(static_cast<uint16_t>(string[i]));
	}

	for (uint32_t j = 0; j < remSize; ++j) {
		bitStream.Write(static_cast<uint16_t>(0));
	}
}

void PacketUtils::WritePacketWString(const std::string& string, uint32_t maxSize, RakNet::BitStream* bitStream) {
	uint32_t size = static_cast<uint32_t>(string.length());
	uint32_t remSize = static_cast<uint32_t>(maxSize - size);

	if (size > maxSize) size = maxSize;

	for (uint32_t i = 0; i < size; ++i) {
		bitStream->Write(static_cast<uint16_t>(string[i]));
	}

	for (uint32_t j = 0; j < remSize; ++j) {
		bitStream->Write(static_cast<uint16_t>(0));
	}
}

//! Saves a packet to the filesystem
void PacketUtils::SavePacket(const std::string& filename, const char* data, size_t length) {
	//If we don't log to the console, don't save the bin files either. This takes up a lot of time.
	if (!Game::logger->GetIsLoggingToConsole()) return;

	std::string path = "packets/" + filename;

	std::ofstream file(path, std::ios::binary);
	if (!file.is_open()) return;

	file.write(data, length);
	file.close();
}
