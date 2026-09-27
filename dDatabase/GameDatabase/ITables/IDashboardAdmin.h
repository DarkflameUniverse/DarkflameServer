#ifndef __IDASHBOARDADMIN__H__
#define __IDASHBOARDADMIN__H__

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "json.hpp"

/**
 * Dashboard administration data: outgoing webhooks, economy anomaly flags, small persistent state, two-factor
 * login, and the queries the dashboard's daily jobs need (anomaly checks, ledger compaction).
 */
class IDashboardAdmin {
public:
	// ---- Webhooks ----

	struct Webhook {
		uint32_t id{};
		std::string name;
		std::string url;
		std::string format;  // "discord", "slack" or "json"
		std::string events;  // comma separated event names, or "*"
		std::string secret;  // signs "json" deliveries (X-DLU-Signature), empty for none
		bool enabled{ true };
		int64_t createdAt{};
		int64_t lastSentAt{};
		int32_t lastStatus{};
		std::string lastError;
	};

	virtual std::vector<Webhook> GetWebhooks() = 0;
	virtual std::optional<Webhook> GetWebhook(uint32_t id) = 0;
	virtual void InsertWebhook(const Webhook& webhook) = 0;
	virtual void UpdateWebhook(const Webhook& webhook) = 0; // name, url, format, events, secret, enabled
	virtual void DeleteWebhook(uint32_t id) = 0;
	virtual void RecordWebhookResult(uint32_t id, int64_t time, int32_t status, const std::string& error) = 0;

	// ---- Small persistent key/value state (job bookkeeping, scheduled restarts) ----

	virtual std::optional<std::string> GetDashboardState(const std::string& name) = 0;
	virtual void SetDashboardState(const std::string& name, const std::string& value) = 0;
	virtual void DeleteDashboardState(const std::string& name) = 0;

	// ---- Per-account view choices (a JSON object the pages own) ----

	virtual std::string GetDashboardPreferences(uint32_t accountId) = 0; // "{}" when none are saved
	virtual void SetDashboardPreferences(uint32_t accountId, const std::string& prefs) = 0;

	// ---- Economy anomaly flags ----

	enum class eFlagKind : uint8_t {
		COIN_INCOME = 1,   // a character earned far more coins in a day than players usually do
		ITEM_SPIKE = 2,    // far more of an item was created in a day than usual
		DUPLICATE = 3,     // an object id exists in more than one place (day 0, so each item is flagged once)
		// different items share an object id and the login migration will not separate them: the characters holding
		// them already migrated, or a copy is in mail (day 0)
		ID_COLLISION = 4,
		// a character has an item on the contraband list (value: how many, baseline: 1 when it was removed). Found when
		// the character loaded (day 0, one flag per item) or when the item was added (that day, item id 0)
		CONTRABAND = 5
	};

	enum class eFlagStatus : uint8_t { OPEN = 0, DISMISSED = 1, ACTIONED = 2 };

	struct EconomyFlag {
		uint32_t day{};
		eFlagKind kind{};
		LWOOBJID characterId{};
		LOT lot{};
		LWOOBJID itemId{};
		int64_t value{};
		int64_t baseline{};
		std::string details;
	};

	// Adds the flag unless the same one exists already. Returns whether it was new.
	virtual bool InsertEconomyFlag(const EconomyFlag& flag) = 0;

	// Flags newest first, optionally only one status (status < 0 for all); DataTables format
	virtual nlohmann::json GetEconomyFlagsTable(uint32_t start, uint32_t length, int32_t status) = 0;
	virtual void ReviewEconomyFlag(uint64_t id, eFlagStatus status, uint32_t reviewerAccountId, const std::string& note) = 0;
	virtual uint32_t GetOpenEconomyFlagCount() = 0;

	// Coins each character earned on a day, not counting trades and mail and not counting staff
	virtual std::vector<std::pair<LWOOBJID, int64_t>> GetDailyIncome(uint32_t day) = 0;

	// Items created per LOT over a range of days, not counting staff
	virtual std::map<LOT, int64_t> GetItemCreationTotals(uint32_t fromDay, uint32_t toDay) = 0;

	// ---- Ledger size ----

	/**
	 * Merge daily ledger rows from before cutoffDay into one row per month (on the month's first day), and map
	 * events from before mapCutoffDay likewise. Returns how many daily rows were merged away.
	 */
	virtual uint32_t CompactEconomy(uint32_t cutoffDay, uint32_t mapCutoffDay) = 0;
	virtual uint32_t PruneTransfers(int64_t beforeTime) = 0;

	enum class eLog : uint8_t { ACTIVITY, COMMAND, AUDIT, CHEAT_DETECTION, CHAT, LOGIN_ADDRESS };
	// Delete log rows older than a unix time; rows without a time (0) are kept. Returns rows deleted.
	virtual uint32_t PruneLog(eLog log, int64_t beforeTime) = 0;

	// ---- Two-factor login ----

	struct Totp {
		std::string encryptedSecret; // empty when not set up
		int64_t enabledAt{};         // 0 while off
		int64_t lastStep{};
	};

	virtual Totp GetTotp(uint32_t accountId) = 0;
	virtual void SetTotp(uint32_t accountId, const std::string& encryptedSecret, int64_t enabledAt) = 0;
	// Accept a code's time step only once: records it and returns true if it is newer than the last one used
	virtual bool UseTotpStep(uint32_t accountId, int64_t step) = 0;

	// Recovery codes are stored hashed; each works once
	virtual void ReplaceRecoveryCodes(uint32_t accountId, const std::vector<std::string>& codeHashes) = 0;
	virtual bool UseRecoveryCode(uint32_t accountId, const std::string& codeHash) = 0;
	virtual uint32_t GetRecoveryCodesLeft(uint32_t accountId) = 0;

	// ---- Misc ----

	// Bug reports with an id above afterId, oldest first: {id, body, client_version, other_player_id, selection, reporter_id, reporter_name}
	virtual nlohmann::json GetBugReportsAfter(uint32_t afterId, uint32_t limit) = 0;
	virtual uint32_t GetMaxBugReportId() = 0;

	// Trades and mail (not inventory moves) involving any of these characters, newest first; DataTables format
	virtual nlohmann::json GetTransfersForCharacters(const std::vector<LWOOBJID>& characterIds, uint32_t start, uint32_t length) = 0;

	// Days since the Unix epoch of the first day of the month `day` falls in
	static uint32_t MonthStartDay(uint32_t day) {
		// Civil-from-days (Howard Hinnant's algorithm), then back to the first of that month
		const int64_t z = static_cast<int64_t>(day) + 719468;
		const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
		const int64_t doe = z - era * 146097;
		const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		const int64_t mp = (5 * doy + 2) / 153;
		const int64_t dayOfMonth = doy - (153 * mp + 2) / 5 + 1;
		return static_cast<uint32_t>(static_cast<int64_t>(day) - (dayOfMonth - 1));
	}
};

#endif  //!__IDASHBOARDADMIN__H__
