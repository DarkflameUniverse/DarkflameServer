#include "ObjectIDManager.h"

// Std
#include <random>

// Custom Classes
#include "MasterPackets.h"
#include "Database.h"
#include "dLogger.h"
#include "Game.h"

// Static Variables
ObjectIDManager* ObjectIDManager::m_Address = nullptr;
static std::uniform_int_distribution<int> uni(10000000, INT32_MAX);

//! Initializes the manager
void ObjectIDManager::Initialize(void) {
	//this->currentRequestID = 0;
	this->currentObjectID = uint32_t(1152921508165007067); //Initial value for this server's objectIDs
}

//! Requests a persistent ID
void ObjectIDManager::RequestPersistentID(std::function<void(uint32_t)> callback) {
	PersistentIDRequest* request = new PersistentIDRequest();
	request->requestID = ++this->currentRequestID;
	request->callback = callback;

	this->requests.push_back(request);

	MasterPackets::SendPersistentIDRequest(Game::server, request->requestID);
}

//! Handles a persistent ID response
void ObjectIDManager::HandleRequestPersistentIDResponse(uint64_t requestID, uint32_t persistentID) {
	for (uint32_t i = 0; i < this->requests.size(); ++i) {
		if (this->requests[i]->requestID == requestID) {

			// Call the callback function
			this->requests[i]->callback(persistentID);

			// Then delete the request
			delete this->requests[i];
			this->requests.erase(this->requests.begin() + i);
			return;
		}
	}
}

//! Handles cases where we have to get a unique object ID synchronously
uint32_t ObjectIDManager::GenerateRandomObjectID() {
	std::random_device rd;

	std::mt19937 rng(rd());

	return  uni(rng);
}


//! Generates an object ID server-sided (used for regular entities like smashables)
uint32_t ObjectIDManager::GenerateObjectID(void) {
	return ++this->currentObjectID;
}
