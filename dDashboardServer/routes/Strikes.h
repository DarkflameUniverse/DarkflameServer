#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "dCommonVars.h"
#include "eStrikeSource.h"
#include "json.hpp"

struct HTTPContext;
struct HTTPReply;

/**
 * Strikes against accounts. Rejecting a name, pet name or property, or removing a leaderboard score, can carry a strike
 * when staff decide the thing deserved one ({strike: true} in the request); not every rejection does. Strikes can also be
 * given by hand on an account and revoked. With strike_expiry_days set, older strikes stay on record but stop counting.
 */
namespace Strikes {
	// Strikes given at or after this time count (0: all of them)
	int64_t CountsSince();

	uint32_t Active(uint32_t accountId);

	/**
	 * Whether the request asks for a strike ({strike: true}). When it does and the user may not give strikes, the error
	 * is already sent and this returns nullopt: stop without doing anything.
	 */
	std::optional<bool> Requested(const HTTPContext& context, const nlohmann::json& body, HTTPReply& reply);

	struct Given {
		uint32_t active{};        // the account's active strikes afterwards
		std::string step;         // what a strike threshold did on its own (empty: nothing)
		uint32_t stepRequestId{}; // the in-game part of that step (warning shown, player disconnected), 0 if none

		// Adds {strikes, strikeStep, strikeStepRequestId} to a reply; the dashboard shows the step to the moderator
		void Into(nlohmann::json& response) const;
	};

	/**
	 * Record a strike; audited (and so sent to webhooks). Then, if the account reached a strike threshold, apply its
	 * step (warn, mute or ban) the same way the account page does, unless it was already applied for that many strikes.
	 */
	Given Give(const HTTPContext& context, uint32_t accountId, LWOOBJID characterId, eStrikeSource source, const std::string& subject, const std::string& reason);
}

void RegisterStrikeRoutes();
