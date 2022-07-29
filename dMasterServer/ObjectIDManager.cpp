#include "ObjectIDManager.h"

// Custom Classes
#include "Database.h"
#include "dLogger.h"

// Static Variables
ObjectIDManager* ObjectIDManager::m_Address = nullptr;

//! Initializes the manager
void ObjectIDManager::Initialize(dLogger* logger) {
	this->mLogger = logger;
	this->currentPersistentID = 1;

	try {
		sql::PreparedStatement* stmt = Database::CreatePreppedStmt(
			"SELECT last_object_id FROM object_id_tracker");

		sql::ResultSet* result = stmt->executeQuery();
		auto next = result->next();

		if (!next) {
			sql::PreparedStatement* insertStmt = Database::CreatePreppedStmt(
				"INSERT INTO object_id_tracker VALUES (1)");

			insertStmt->execute();

			delete insertStmt;

			return;
		}

		while (next) {
			this->currentPersistentID =
				result->getInt(1) > 0 ? result->getInt(1) : 1;
			next = result->next();
		}

		delete result;
		delete stmt;
	} catch (sql::SQLException& e) {
		mLogger->Log("ObjectIDManager", "Unable to fetch max persistent object "
			"ID in use. Defaulting to 1.");
		mLogger->Log("ObjectIDManager", "SQL error: %s", e.what());
		this->currentPersistentID = 1;
	}
}

//! Generates a new persistent ID
uint32_t ObjectIDManager::GeneratePersistentID(void) {
	uint32_t toReturn = ++this->currentPersistentID;

	// So we peroidically save our ObjID to the database:
	if (toReturn % 25 == 0) { // TEMP: DISABLED FOR DEBUG / DEVELOPMENT!
		sql::PreparedStatement* stmt = Database::CreatePreppedStmt(
			"UPDATE object_id_tracker SET last_object_id=?");
		stmt->setUInt(1, toReturn);
		stmt->execute();
		delete stmt;
	}

	return toReturn;
}

void ObjectIDManager::SaveToDatabase() {
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt(
		"UPDATE object_id_tracker SET last_object_id=?");
	stmt->setUInt(1, currentPersistentID);
	stmt->execute();
	delete stmt;
}
