#ifndef __IACCOUNTSTRIKES__H__
#define __IACCOUNTSTRIKES__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"

/**
 * Strikes against accounts: kept when staff reject something a player made (a name, a pet name, a property) or
 * remove a score and decide it deserves one, or given by hand. Not every rejection is a strike; staff choose.
 * A revoked strike stays on record but no longer counts.
 */
class IAccountStrikes {
public:
	struct Strike {
		uint64_t id{};
		uint32_t accountId{};
		LWOOBJID characterId{};    // the character it was about, 0 for the account as a whole
		std::string source;        // eStrikeSource's name: MANUAL, NAME, PET_NAME, PROPERTY, LEADERBOARD, PLAYER_REPORT
		std::string subject;       // what was rejected: the name, the property's name, the leaderboard
		std::string reason;
		uint32_t givenById{};
		std::string givenBy;
		int64_t createdAt{};
		int64_t revokedAt{};       // 0 while it counts
		std::string revokedBy;
		std::string revokeReason;
	};

	virtual uint64_t InsertStrike(const Strike& strike) = 0;
	virtual std::vector<Strike> GetStrikes(uint32_t accountId) = 0; // newest first
	virtual std::optional<Strike> GetStrike(uint64_t id) = 0;
	virtual void RevokeStrike(uint64_t id, const std::string& revokedBy, const std::string& reason, int64_t time) = 0;
	// Strikes that still count: not revoked and given at or after `since` (0: ever). accountId 0: every account
	virtual uint32_t CountActiveStrikes(uint32_t accountId, int64_t since) = 0;
};

#endif  //!__IACCOUNTSTRIKES__H__
