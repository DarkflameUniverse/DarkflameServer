#include "PacketUtils.h"
#include <fstream>
#include "Logger.h"
#include "Game.h"

//! Saves a packet to the filesystem
void PacketUtils::SavePacket(const std::string& filename, const char* data, size_t length) {
	//If we don't log to the console, don't save the bin files either. This takes up a lot of time.
	if (!Game::logger->GetLogToConsole()) return;

	std::string path = "packets/" + filename;

	std::ofstream file(path, std::ios::binary);
	if (!file.is_open()) return;

	file.write(data, length);
	file.close();
}
