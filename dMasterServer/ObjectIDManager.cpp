#include "ObjectIDManager.h"

// Custom Classes
#include "Database.h"
#include "Logger.h"
#include "Game.h"

// Static Variables
ObjectIDManager* ObjectIDManager::m_Address = nullptr;

//! Initializes the manager
void ObjectIDManager::Initialize(Logger* logger) {
	this->mLogger = logger;
	this->currentPersistentID = 1;

	try {
		auto lastObjectId = Database::Get()->GetCurrentPersistentId();

		if (!lastObjectId) {
			Database::Get()->InsertDefaultPersistentId();
			return;
		} else {
			this->currentPersistentID = lastObjectId.value();
		}

		if (this->currentPersistentID <= 0) {
			LOG("Invalid persistent object ID in database. Aborting to prevent bad id generation.");
			throw std::runtime_error("Invalid persistent object ID in database. Aborting to prevent bad id generation.");
		}
	} catch (sql::SQLException& e) {
		LOG("Unable to fetch max persistent object ID in use. This will cause issues. Aborting to prevent collisions.");
		LOG("SQL error: %s", e.what());
		throw;
	}
}

//! Generates a new persistent ID
uint32_t ObjectIDManager::GeneratePersistentID() {
	uint32_t toReturn = ++this->currentPersistentID;

	SaveToDatabase();

	return toReturn;
}

void ObjectIDManager::SaveToDatabase() {
	Database::Get()->UpdatePersistentId(this->currentPersistentID);
}
