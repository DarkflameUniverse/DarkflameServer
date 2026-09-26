#ifndef __ILIVEOPS__H__
#define __ILIVEOPS__H__

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "dCommonVars.h"

/**
 * Live operations run from the dashboard: live events (treasure hunts, bonus multipliers, invasions, celebrations)
 * that world servers run for a while in chosen zones, and server-wide community challenges counted from what the
 * game records per character. The dashboard writes the definitions; world servers write progress.
 */
class ILiveOps {
public:
	enum class eLiveEventState : uint8_t { ACTIVE = 0, ENDED = 1, CANCELLED = 2 };

	struct LiveEvent {
		uint64_t id{};
		std::string type;            // LiveOpsRules::eEventType name, e.g. "treasure_hunt"
		std::string title;
		std::string message;         // announced when it starts
		std::vector<uint32_t> zones; // empty: every world (bonus events only)
		int32_t instanceId{ -1 };    // -1: every instance of the zones
		std::string config;          // JSON, per type
		int64_t startsAt{};
		int64_t endsAt{};
		eLiveEventState state{ eLiveEventState::ACTIVE };
		int64_t endedAt{};
		std::string endReason;
		std::string createdBy;
		std::string endedBy;
	};

	// Newest first. activeOnly: only events still running (state ACTIVE, whatever the time)
	virtual std::vector<LiveEvent> GetLiveEvents(bool activeOnly, uint32_t limit) = 0;
	virtual std::optional<LiveEvent> GetLiveEvent(uint64_t id) = 0;
	virtual uint64_t InsertLiveEvent(const LiveEvent& event) = 0;
	// Ends a running event; false when it had already ended
	virtual bool EndLiveEvent(uint64_t id, eLiveEventState state, int64_t endedAt, const std::string& endedBy, const std::string& reason) = 0;

	struct LiveEventInstance {
		uint64_t eventId{};
		uint32_t zoneId{};
		uint32_t instanceId{};
		std::string status; // JSON the world reported
		int64_t updatedAt{};
	};

	virtual std::vector<LiveEventInstance> GetLiveEventInstances(const std::vector<uint64_t>& eventIds) = 0;
	// Insert or replace
	virtual void SetLiveEventInstance(const LiveEventInstance& instance) = 0;

	struct LiveOpsScore {
		uint64_t id{}; // the event or challenge
		LWOOBJID characterId{};
		std::string name;
		int64_t amount{};
	};

	// Adds to each character's score
	virtual void AddLiveEventScores(uint64_t eventId, const std::vector<std::pair<LWOOBJID, int64_t>>& scores, int64_t time) = 0;
	// Highest first, with character names
	virtual std::vector<LiveOpsScore> GetLiveEventScores(uint64_t eventId, uint32_t limit) = 0;

	enum class eChallengeState : uint8_t { OPEN = 0, COMPLETED = 1, EXPIRED = 2, CANCELLED = 3 };
	enum class eChallengeMetric : uint8_t { STATISTIC = 0, MAP_EVENT = 1 };

	struct Challenge {
		uint64_t id{};
		std::string title;
		std::string description;
		eChallengeMetric metricKind{ eChallengeMetric::STATISTIC };
		uint32_t metric{};           // StatisticID or IEconomyLedger::eMapEvent
		LOT lot{};                   // map events: only this LOT (0: any)
		std::vector<uint32_t> zones; // empty: every zone
		int64_t target{};
		int64_t startsAt{};
		int64_t endsAt{};
		bool includeStaff{};
		bool isPublic{ true };
		int64_t rewardCoins{};
		std::string rewardItems{ "[]" }; // JSON [{lot, count}]
		int64_t rewardMin{ 1 };
		eChallengeState state{ eChallengeState::OPEN };
		uint8_t milestone{};             // last percentage announced
		int64_t completedAt{};
		int64_t rewardedAt{};
		uint32_t rewardedCount{};
		int64_t endAnnouncedAt{};        // when players were told in game it was complete or over; 0: still to do
		int64_t createdAt{};
		std::string createdBy;
		int64_t updatedAt{};
		std::string updatedBy;
	};

	// Newest first
	virtual std::vector<Challenge> GetChallenges(bool openOnly) = 0;
	virtual std::optional<Challenge> GetChallenge(uint64_t id) = 0;
	virtual uint64_t InsertChallenge(const Challenge& challenge) = 0;
	virtual void UpdateChallenge(const Challenge& challenge) = 0;
	// With its contributions and rewards
	virtual void DeleteChallenge(uint64_t id) = 0;

	struct Contribution {
		uint64_t challengeId{};
		LWOOBJID characterId{};
		int64_t amount{};
	};

	// Adds to each character's contribution
	virtual void AddChallengeContributions(const std::vector<Contribution>& contributions, int64_t time) = 0;

	struct ChallengeTotal {
		int64_t total{};
		uint32_t contributors{};
	};

	virtual std::map<uint64_t, ChallengeTotal> GetChallengeTotals(const std::vector<uint64_t>& challengeIds) = 0;
	// Highest first, with character names; limit 0: all
	virtual std::vector<LiveOpsScore> GetChallengeContributions(uint64_t challengeId, uint32_t limit) = 0;
	// challenge -> amount, for one character
	virtual std::map<uint64_t, int64_t> GetCharacterContributions(LWOOBJID characterId) = 0;

	struct Reward {
		uint64_t challengeId{};
		LWOOBJID characterId{};
		int64_t amount{};  // what the character contributed
		int64_t coins{};
		int64_t rewardedAt{};
		int64_t claimedAt{}; // 0: coins still waiting
	};

	virtual std::vector<Reward> GetChallengeRewards(uint64_t challengeId) = 0;
	virtual void InsertChallengeReward(const Reward& reward) = 0;
	// Rewards with coins the character hasn't been given yet
	virtual std::vector<Reward> GetUnclaimedChallengeCoins(LWOOBJID characterId) = 0;
	// Marks the coins given; false when they already were (so they are given once)
	virtual bool ClaimChallengeCoins(uint64_t challengeId, LWOOBJID characterId, int64_t time) = 0;
};

#endif  //!__ILIVEOPS__H__
