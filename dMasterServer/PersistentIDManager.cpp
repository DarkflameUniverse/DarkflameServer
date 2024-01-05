#include "PersistentIDManager.h"

// Custom Classes
#include "Database.h"
#include "Logger.h"
#include "Game.h"

namespace {	
	uint32_t CurrentPersistentID = 1;               //!< The highest current persistent ID in use
};

//! Initializes the manager
void PersistentIDManager::Initialize() {
	try {
		auto lastObjectId = Database::Get()->GetCurrentPersistentId();

		if (!lastObjectId) {
			Database::Get()->InsertDefaultPersistentId();
		} else {
			CurrentPersistentID = lastObjectId.value();
		}

		LOG("Current persistent ID: %u", CurrentPersistentID);

		if (CurrentPersistentID <= 0) {
			LOG("Invalid persistent object ID in database. Aborting to prevent bad id generation.");
			throw std::runtime_error("Invalid persistent object ID in database. Aborting to prevent bad id generation.");
		}
	} catch (sql::SQLException& e) {
		LOG("Unable to fetch max persistent object ID in use. This will cause issues. Aborting to prevent collisions.");
		LOG("SQL error: %s", e.what());
		throw e;
	}
}

//! Generates a new persistent ID
uint32_t PersistentIDManager::GeneratePersistentID() {
	uint32_t toReturn = ++CurrentPersistentID;

	SaveToDatabase();

	return toReturn;
}

void PersistentIDManager::SaveToDatabase() {
	Database::Get()->UpdatePersistentId(CurrentPersistentID);
}
