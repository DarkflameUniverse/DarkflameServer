#ifndef __DATACHANGED__H__
#define __DATACHANGED__H__

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "BitStream.h"
#include "dCommonVars.h"

/**
 * DATA_CHANGED payload: the tables and rows a game server just wrote. The dashboard turns each entry into a
 * table_changed event so open pages refresh without waiting for its periodic database check.
 */
struct DataChanged {
	struct Entry {
		std::string table; // dashboard table name, e.g. "characters", "mail", "economy"
		LWOOBJID id{};     // row the change is about, 0 for the table as a whole
	};

	static constexpr size_t MAX_ENTRIES = 1024;
	static constexpr size_t MAX_TABLE_NAME = 32;

	std::vector<Entry> entries;

	void Serialize(RakNet::BitStream& stream) const {
		const auto count = static_cast<uint16_t>(std::min(entries.size(), MAX_ENTRIES));
		stream.Write(count);
		for (size_t i = 0; i < count; i++) {
			const auto& entry = entries[i];
			const auto length = static_cast<uint8_t>(std::min(entry.table.size(), MAX_TABLE_NAME));
			stream.Write(length);
			stream.Write(entry.table.data(), length);
			stream.Write(entry.id);
		}
	}

	bool Deserialize(RakNet::BitStream& stream) {
		uint16_t count{};
		if (!stream.Read(count) || count > MAX_ENTRIES) return false;
		entries.clear();
		entries.reserve(count);
		for (uint16_t i = 0; i < count; i++) {
			uint8_t length{};
			if (!stream.Read(length) || length > MAX_TABLE_NAME) return false;
			Entry entry;
			entry.table.resize(length);
			if (length > 0 && !stream.Read(entry.table.data(), length)) return false;
			if (!stream.Read(entry.id)) return false;
			entries.push_back(std::move(entry));
		}
		return true;
	}
};

#endif  //!__DATACHANGED__H__
