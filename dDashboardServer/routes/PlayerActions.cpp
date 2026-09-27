#include "PlayerActions.h"
#include "MasterPackets.h"

#include <chrono>
#include <deque>
#include <map>

#include "BitStreamUtils.h"
#include "Game.h"
#include "Logger.h"
#include "MessageType/Master.h"
#include "ServiceType.h"
#include "Web.h"
#include "dServer.h"

namespace {
	// Master answers within its own 3s timeout; this only covers a lost connection
	constexpr auto DASHBOARD_TIMEOUT = std::chrono::seconds(10);
	constexpr size_t MAX_REMEMBERED_RESULTS = 200;

	struct Pending {
		PlayerActions::Completion onComplete;
		ePlayerAction action{};
		std::chrono::steady_clock::time_point deadline;
		uint32_t owner{};
	};

	struct ResultData {
		uint32_t owner{};
		nlohmann::json data;
	};
	std::map<uint32_t, ResultData> g_ResultData;

	uint32_t g_NextRequestId = 1;
	std::map<uint32_t, Pending> g_Pending;
	struct StoredResult {
		uint32_t owner{};
		nlohmann::json status;
	};
	std::map<uint32_t, StoredResult> g_Results;
	std::deque<uint32_t> g_ResultOrder;

	void Complete(uint32_t requestId, const PlayerActionResult& result, bool job = false) {
		const auto it = g_Pending.find(requestId);
		if (it == g_Pending.end()) return;
		auto completion = std::move(it->second.onComplete);
		const auto owner = it->second.owner;
		g_Pending.erase(it);

		PlayerActions::Outcome outcome;
		try {
			outcome = completion(result);
		} catch (const std::exception& ex) {
			LOG("Player action %u completion failed: %s", requestId, ex.what());
			outcome = { false, "The action failed to complete" };
		}

		nlohmann::json status{
			{"requestId", requestId},
			{"status", "done"},
			{"success", outcome.success},
			{"message", outcome.message},
			{"timedOut", result.timedOut}
		};
		if (outcome.affected) status["affected"] = *outcome.affected;
		else if (!job) status["affected"] = result.affected;
		status["hasData"] = !outcome.data.is_null();
		g_Results[requestId] = { owner, status };
		if (!outcome.data.is_null()) g_ResultData[requestId] = { owner, outcome.data };
		g_ResultOrder.push_back(requestId);
		if (g_ResultOrder.size() > MAX_REMEMBERED_RESULTS) {
			g_Results.erase(g_ResultOrder.front());
			g_ResultData.erase(g_ResultOrder.front());
			g_ResultOrder.pop_front();
		}
		// Only the account that started it hears the outcome (messages can name players or email addresses)
		Game::web.SendWSMessageToAccount("action_result", status, owner);
	}
}

uint32_t PlayerActions::Request(PlayerActionRequest request, uint32_t ownerAccountId, Completion onComplete) {
	request.requestId = g_NextRequestId++;
	g_Pending[request.requestId] = { std::move(onComplete), request.action, std::chrono::steady_clock::now() + DASHBOARD_TIMEOUT, ownerAccountId };

	// Without master no world server is running, so nobody can be online
	if (!Game::server || !Game::server->GetIsConnectedToMaster()) {
		PlayerActionResult result;
		result.requestId = request.requestId;
		result.action = request.action;
		Complete(request.requestId, result);
		return request.requestId;
	}

	MasterPackets::SendToMaster(request);
	return request.requestId;
}

void PlayerActions::HandleResult(const PlayerActionResult& result) {
	Complete(result.requestId, result);
}

void PlayerActions::Update() {
	const auto now = std::chrono::steady_clock::now();
	std::vector<std::pair<uint32_t, ePlayerAction>> expired;
	for (const auto& [id, pending] : g_Pending) {
		if (now >= pending.deadline) expired.emplace_back(id, pending.action);
	}
	for (const auto& [id, action] : expired) {
		PlayerActionResult result;
		result.requestId = id;
		result.action = action;
		result.timedOut = true;
		Complete(id, result);
	}
}

uint32_t PlayerActions::Begin(uint32_t ownerAccountId, std::chrono::seconds timeout) {
	const auto requestId = g_NextRequestId++;
	// Completed by Finish; the deadline only guards against work that never reports back
	g_Pending[requestId] = { [](const PlayerActionResult&) { return Outcome{ false, "Timed out" }; }, ePlayerAction::KICK_ACCOUNT,
		std::chrono::steady_clock::now() + timeout, ownerAccountId };
	return requestId;
}

void PlayerActions::Finish(uint32_t requestId, const Outcome& outcome) {
	const auto it = g_Pending.find(requestId);
	if (it == g_Pending.end()) return;
	it->second.onComplete = [outcome](const PlayerActionResult&) { return outcome; };
	PlayerActionResult result;
	result.requestId = requestId;
	Complete(requestId, result, true);
}

nlohmann::json PlayerActions::GetStatus(uint32_t requestId, uint32_t requesterAccountId) {
	// Request ids are sequential, so only the account that started a request may read it
	const auto it = g_Results.find(requestId);
	if (it != g_Results.end()) {
		if (it->second.owner == 0 || it->second.owner != requesterAccountId) return { {"requestId", requestId}, {"status", "unknown"} };
		auto status = it->second.status;
		const auto data = g_ResultData.find(requestId);
		if (data != g_ResultData.end()) status["data"] = data->second.data;
		return status;
	}
	const auto pending = g_Pending.find(requestId);
	if (pending != g_Pending.end() && pending->second.owner != 0 && pending->second.owner == requesterAccountId) return { {"requestId", requestId}, {"status", "pending"} };
	return { {"requestId", requestId}, {"status", "unknown"} };
}
