#include "PacketUtils.h"
#include <vector>
#include <fstream>
#include "dLogger.h"
#include "Game.h"

uint16_t PacketUtils::ReadU16(uint32_t startLoc, Packet* packet) {
	if (startLoc + 2 > packet->length) return 0;

	std::vector<unsigned char> t;
	for (uint32_t i = startLoc; i < startLoc + 2; i++) t.push_back(packet->data[i]);
	return *(uint16_t*)t.data();
}

uint32_t PacketUtils::ReadU32(uint32_t startLoc, Packet* packet) {
	if (startLoc + 4 > packet->length) return 0;

	std::vector<unsigned char> t;
	for (uint32_t i = startLoc; i < startLoc + 4; i++) {
		t.push_back(packet->data[i]);
	}
	return *(uint32_t*)t.data();
}

uint64_t PacketUtils::ReadU64(uint32_t startLoc, Packet* packet) {
	if (startLoc + 8 > packet->length) return 0;

	std::vector<unsigned char> t;
	for (uint32_t i = startLoc; i < startLoc + 8; i++) t.push_back(packet->data[i]);
	return *(uint64_t*)t.data();
}

int64_t PacketUtils::ReadS64(uint32_t startLoc, Packet* packet) {
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
