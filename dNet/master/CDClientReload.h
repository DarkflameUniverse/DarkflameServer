#ifndef __CDCLIENTRELOAD__H__
#define __CDCLIENTRELOAD__H__

#include <cstdint>
#include <string>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "InstanceMigration.h"
#include "MessageType/Master.h"
#include "dCommonVars.h"

/**
 * CDCLIENT_RELOAD payload.
 *
 * Master -> every server: the client's cdclient.fdb changed. fdb and sqlite name the new copy and its CDServer.sqlite
 * in resServer (FdbSnapshot.h); each server switches to them between frames.
 *
 * World or dashboard -> master, with no names: check the client's fdb now (a GM's /reloadcdclient or the dashboard).
 * requesterId is the character who asked (told the result in chat), 0 for the dashboard.
 */
struct CDClientReload : public LUBitStream {
	CDClientReload() : LUBitStream(ServiceType::MASTER, MessageType::Master::CDCLIENT_RELOAD) {}

	static constexpr uint16_t MAX_NAME = 64;

	LWOOBJID requesterId{};
	std::string fdb;
	std::string sqlite;

	[[nodiscard]] bool IsRequest() const { return fdb.empty(); }

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(requesterId);
		InstanceMigration::WriteText(stream, fdb, MAX_NAME);
		InstanceMigration::WriteText(stream, sqlite, MAX_NAME);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		if (!stream.Read(requesterId)) return false;
		if (!InstanceMigration::ReadText(stream, fdb, MAX_NAME) || !InstanceMigration::ReadText(stream, sqlite, MAX_NAME)) return false;
		// Names in resServer only
		for (const auto* name : { &fdb, &sqlite }) {
			if (name->find_first_of("/\\") != std::string::npos || *name == "..") return false;
		}
		return fdb.empty() == sqlite.empty();
	}
};

#endif  //!__CDCLIENTRELOAD__H__
