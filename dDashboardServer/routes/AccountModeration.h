#pragma once

#include <cstdint>
#include <string>

#include "eServerDisconnectIdentifiers.h"

struct HTTPContext;

/**
 * Bans, mutes, warnings and kicks on an account, as the account page's buttons do them and as strike thresholds apply
 * them: each is written to the account's moderation history and the audit log (and so to moderation webhooks).
 * Permission and rank checks are the caller's.
 */
namespace AccountModeration {
	// An entry in the account's moderation history
	void Note(const HTTPContext& context, uint32_t accountId, const std::string& kind, const std::string& text);

	// Disconnect every online session of the account. Returns the player action's request id.
	uint32_t KickAccount(const HTTPContext& context, uint32_t accountId, eServerDisconnectIdentifiers reason);

	// Ban for `days` (0: permanently) with a reason the player sees, and disconnect them. Returns the kick's request id.
	uint32_t Ban(const HTTPContext& context, uint32_t accountId, int64_t days, const std::string& reason);

	// Mute until a unix time (0 unmutes)
	void Mute(const HTTPContext& context, uint32_t accountId, uint64_t muteUntil, const std::string& reason);

	// A warning on the account's record; with tellPlayer also shown to them in game. Returns the request id (0: not sent).
	uint32_t Warn(const HTTPContext& context, uint32_t accountId, const std::string& text, bool tellPlayer);
}
