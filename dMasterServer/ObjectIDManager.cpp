#include "ObjectIDManager.h"

// Custom Classes
#include "Database.h"
#include "dLogger.h"

// Static Variables
ObjectIDManager* ObjectIDManager::m_Address = nullptr;

//! Initializes the manager
void ObjectIDManager::Initialize(dLogger* logger) {
	this->mLogger = logger;
	this->currentPersistentID = Database::Connection->GetObjectIDTracker();
}

//! Generates a new persistent ID
uint32_t ObjectIDManager::GeneratePersistentID(void) {
	uint32_t toReturn = ++this->currentPersistentID;

	Database::Connection->SetObjectIDTracker(toReturn);

	return toReturn;
}

void ObjectIDManager::SaveToDatabase() {
	Database::Connection->SetObjectIDTracker(this->currentPersistentID);
}
