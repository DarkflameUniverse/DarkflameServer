#include "ObjectIDManager.h"

// Custom Classes
#include "MasterPackets.h"
#include "Database.h"
#include "Logger.h"
#include "Game.h"

 //! The persistent ID request
struct PersistentIDRequest {
	PersistentIDRequest(const uint64_t& requestID, const std::function<void(uint32_t)>& callback) : requestID(requestID), callback(callback) {}
	uint64_t requestID;

	std::function<void(uint32_t)> callback;
};

namespace {
	std::vector<PersistentIDRequest> Requests; //!< All outstanding persistent ID requests
	uint64_t CurrentRequestID = 0;                  //!< The current request ID
	uint32_t CurrentObjectID = uint32_t(1152921508165007067);                   //!< The current object ID
	std::uniform_int_distribution<int> Uni(10000000, INT32_MAX);
};

//! Requests a persistent ID
void ObjectIDManager::RequestPersistentID(const std::function<void(uint32_t)> callback) {
	const auto& request = Requests.emplace_back(++CurrentRequestID, callback);

	MasterPackets::SendPersistentIDRequest(Game::server, request.requestID);
}

//! Handles a persistent ID response
void ObjectIDManager::HandleRequestPersistentIDResponse(const uint64_t requestID, const uint32_t persistentID) {
	auto it = std::find_if(Requests.begin(), Requests.end(), [requestID](const PersistentIDRequest& request) {
		return request.requestID == requestID;
	});

	if (it == Requests.end()) return;

	it->callback(persistentID);
	Requests.erase(it);
}

//! Handles cases where we have to get a unique object ID synchronously
uint32_t ObjectIDManager::GenerateRandomObjectID() {
	return Uni(Game::randomEngine);
}

//! Generates an object ID server-sided (used for regular entities like smashables)
uint32_t ObjectIDManager::GenerateObjectID() {
	return ++CurrentObjectID;
}
