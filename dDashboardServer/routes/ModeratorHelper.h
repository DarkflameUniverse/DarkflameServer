#pragma once

/**
 * The staff-side AI moderator helper: a Suggest button on player reports, chat messages, a player's recent chat,
 * pending character and pet names, and economy flags. It sends Claude the item and the context the dashboard already
 * has (the chat around it, the account's strikes and earlier moderation, the operator's rules) and shows staff a
 * drafted action, a reason for the player and an explanation citing the evidence. Staff decide; nothing is applied
 * automatically and nothing AI-written reaches players unless staff choose to use it.
 *
 * Requests run on a worker thread; results come back through PlayerActions (api.job in the browser), are stored
 * with the item (so the same case isn't sent twice) and audited. Budget limits keep the cost bounded.
 */
namespace ModeratorHelper {
	void RegisterRoutes();

	// Main loop: store and report finished requests
	void Update();

	void Shutdown();
}
