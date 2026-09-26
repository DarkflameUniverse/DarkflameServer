#pragma once

/**
 * Moderation tools:
 * - Player reports: what players report from the game's Report Abuse window (another player, a model, a property),
 *   with acting on them (optionally with a strike) or dismissing them.
 * - Linked accounts: other accounts sharing a play key, email address or login address with an account.
 * - Chat filter: words staff allow or block on top of the filter's files, applied to running worlds, and which recent
 *   chat a word would have changed.
 */
void RegisterModerationToolRoutes();
