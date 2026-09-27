#pragma once

#include <map>
#include <vector>

#include "IUgcLookup.h"
#include "json.hpp"

// Finding players' creations and showing them where they are: the UGC search, and the UGC server's icons and models on
// the property and character pages (for whoever may view those pages)
namespace UgcLinks {
	void RegisterRoutes();

	// Web thread: where each of these creations is (by id): placed on properties, attached to mails, or in its creator's
	// inventories (looked in only for those not placed or mailed, and for at most a few creators). [{type: property|mail|inventory, ...}]
	std::map<LWOOBJID, nlohmann::json> Whereabouts(const std::vector<IUgcLookup::UgcEntry>& entries);
}
