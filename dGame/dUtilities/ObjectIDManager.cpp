#include "ObjectIDManager.h"

// Custom Classes
#include "MasterPackets.h"
#include "Database.h"
#include "Logger.h"
#include "Game.h"
#include "eObjectBits.h"

// should the spawners from vanity also have the CLIENT flag?

namespace {
	// Start the range in a way that it when first called it will fetch some new persistent IDs
	std::optional<IObjectIdTracker::Range> CurrentRange = std::nullopt;
	uint32_t CurrentObjectID = uint32_t(1152921508165007067); // The current object ID (this should really start at the highest current ID in the world, then increment from there)
};

uint64_t ObjectIDManager::GetPersistentID() {
	if (!CurrentRange.has_value() || CurrentRange->minID > CurrentRange->maxID) {
		CurrentRange = Database::Get()->GetPersistentIdRange();
		// We're getting close to being out of IDs in this range, log a warning
		const auto WARNING_RANGE = 70368744100000ULL;
		if (CurrentRange->minID >= 70368744100000ULL) {
			LOG("WARNING: Your server is running low on persistent IDs, please consider an ID squash in the near future.");
		}

		LOG("Reserved object ID range: %llu - %llu", CurrentRange->minID, CurrentRange->maxID);
	}

	const auto usedID = CurrentRange->minID++;
	auto toReturn = usedID;
	// Any IDs gotten from persistent IDs use the CHARACTER bit
	GeneralUtils::SetBit(toReturn, eObjectBits::CHARACTER);
	LOG("Using ID: %llu:%llu", toReturn, usedID);
	return toReturn;
}

// Generates an object ID server-sided (used for regular entities like smashables)
uint32_t ObjectIDManager::GenerateObjectID() {
	return ++CurrentObjectID;
}
