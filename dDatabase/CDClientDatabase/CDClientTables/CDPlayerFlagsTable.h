#ifndef CDPLAYERFLAGSTABLE_H
#define CDPLAYERFLAGSTABLE_H

#include <map>
#include <optional>

namespace CDPlayerFlagsTable {
	struct Entry {
		bool sessionOnly{};
		bool onlySetByServer{};
		bool sessionZoneOnly{};
	};

	using FlagId = uint32_t;
	using Table = std::map<FlagId, std::optional<Entry>>;

	void LoadValuesFromDatabase();
	const std::optional<Entry> GetEntry(const FlagId flagId);
};

#endif  //!CDPLAYERFLAGSTABLE_H
