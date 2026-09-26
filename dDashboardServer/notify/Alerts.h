#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "WebhookFormat.h"

/**
 * Alerts sent to outgoing webhooks (Discord, Slack or any JSON receiver), configured on the Webhooks page.
 * Delivery happens on a worker thread with retries; results are written back from the main loop.
 */
namespace Alerts {
	void Initialize();
	void Shutdown();

	// Main loop: record finished deliveries
	void Update();

	/**
	 * Send an alert to every enabled webhook subscribed to the event.
	 * @param link dashboard path such as "/bug_reports/5"; made absolute with dashboard_url when that is set
	 */
	void Emit(const std::string& event, const std::string& title, const std::string& description,
		std::vector<WebhookFormat::Field> fields = {}, const std::string& link = "");

	// Send a test message to one webhook; done receives an error or nothing on success
	void SendTest(uint32_t webhookId, std::function<void(std::optional<std::string>)> done);

	// ---- Sources ----

	// New bug reports since the last check (called when the game reports one, and by the periodic check)
	void CheckNewBugReports();

	// A character asked for a name that needs approval
	void PendingName(LWOOBJID characterId);

	// Moderation and security actions taken on the dashboard, from the audit log
	void FromAudit(const std::string& actor, const std::string& action, const std::string& description);

	// Bans and mutes done in game with slash commands
	void InGameAccountAction(const std::string& action, uint32_t accountId);

	// Auth/chat availability changes (first call only records the state)
	void ServerStatus(bool authOnline, bool chatOnline);
}
