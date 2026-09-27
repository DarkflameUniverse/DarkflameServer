#pragma once

#include <optional>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

#include "json.hpp"
#include "master/PlayerAction.h"

/**
 * Sends player actions (kick, refresh, rescue) through master to every world server and reports the outcome.
 *
 * Results arrive asynchronously, so HTTP handlers return a request id immediately. When the result comes in,
 * the completion callback runs on the main thread, the outcome is pushed on the `action_result` WebSocket
 * topic to the owner's connections only, and it can be polled with GET /api/actions/:id.
 */
namespace PlayerActions {
	struct Outcome {
		bool success{true};
		std::string message{};
		// Only returned by GetStatus to the account that started the work, never broadcast
		nlohmann::json data = nullptr;
		// For work started with Begin: how many players or characters it changed. Left out of the result when unset,
		// rather than claiming 0 (player actions report what the worlds answered)
		std::optional<uint32_t> affected{};
	};

	// Called with the aggregated result; returns the message shown to the moderator
	using Completion = std::function<Outcome(const PlayerActionResult& result)>;

	// Send an action to all worlds. Returns the request id. Only ownerAccountId (who asked for it) gets the outcome.
	uint32_t Request(PlayerActionRequest request, uint32_t ownerAccountId, Completion onComplete);

	// Feed a PLAYER_ACTION_RESULT packet from master
	void HandleResult(const PlayerActionResult& result);

	// Expire requests master never answered. Call once per tick.
	void Update();

	// Track some other asynchronous work (e.g. sending an email) with the same result reporting.
	// Begin returns a request id; Finish publishes the outcome like a player action result.
	uint32_t Begin(uint32_t ownerAccountId, std::chrono::seconds timeout = std::chrono::minutes(2));
	void Finish(uint32_t requestId, const Outcome& outcome);

	// Result of a finished request, or {"status": "pending"} / {"status": "unknown"}. Only the owner sees it; anyone else gets "unknown".
	nlohmann::json GetStatus(uint32_t requestId, uint32_t requesterAccountId);
}
