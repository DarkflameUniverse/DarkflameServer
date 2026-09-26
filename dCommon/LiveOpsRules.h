#ifndef __LIVEOPSRULES__H__
#define __LIVEOPSRULES__H__

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "GeneralUtils.h"
#include "NiPoint3.h"

/**
 * The rules of live events and community challenges, kept free of the game and the database so both the world
 * servers and the dashboard use the same ones and they can be unit tested: where to put things, which bonus applies
 * when, how far a challenge is and who gets its rewards.
 */
namespace LiveOpsRules {
	// ---------------- Live events ----------------

	enum class eEventType : uint8_t { TREASURE_HUNT, BONUS, INVASION, CELEBRATION };

	constexpr std::string_view TypeName(eEventType type) {
		switch (type) {
		case eEventType::TREASURE_HUNT: return "treasure_hunt";
		case eEventType::BONUS: return "bonus";
		case eEventType::INVASION: return "invasion";
		case eEventType::CELEBRATION: return "celebration";
		}
		return "";
	}

	inline std::optional<eEventType> ParseType(std::string_view name) {
		for (const auto type : { eEventType::TREASURE_HUNT, eEventType::BONUS, eEventType::INVASION, eEventType::CELEBRATION }) {
			if (TypeName(type) == name) return type;
		}
		return std::nullopt;
	}

	// Events that put objects in the world need a zone to put them in
	constexpr bool NeedsZone(eEventType type) { return type != eEventType::BONUS; }

	constexpr int64_t MIN_DURATION = 60;                 // seconds
	constexpr int64_t MAX_DURATION = 7 * 24 * 60 * 60;
	constexpr uint32_t MAX_TREASURES = 50;               // per instance
	constexpr uint32_t MAX_WAVES = 20;
	constexpr uint32_t MAX_PER_WAVE = 30;
	constexpr uint32_t MAX_INVADERS_ALIVE = 60;          // per instance, whatever the waves say
	constexpr float MAX_MULTIPLIER = 10.0f;

	/**
	 * Where to put things: `count` of the candidate positions (known to be on walkable ground), in random order, at
	 * least `minSpacing` apart, and when `center` is set only those within `radius` of it. With `reuse`, candidates are
	 * used again (cycling) when there are fewer than asked for, e.g. for waves of enemies around one point; without it
	 * fewer come back. Spacing is relaxed rather than returning nothing when the candidates are close together.
	 */
	struct Placement {
		size_t count{};
		float minSpacing{};
		std::optional<NiPoint3> center;
		float radius{};
		bool reuse{};
	};

	template<typename Rng>
	std::vector<NiPoint3> ChoosePositions(std::vector<NiPoint3> candidates, const Placement& placement, Rng& rng) {
		std::vector<NiPoint3> chosen;
		if (placement.count == 0) return chosen;
		if (placement.center) {
			const auto center = *placement.center;
			const auto radiusSq = placement.radius * placement.radius;
			std::erase_if(candidates, [&](const NiPoint3& point) { return NiPoint3::DistanceSquared(point, center) > radiusSq; });
		}
		if (candidates.empty()) return chosen;
		std::shuffle(candidates.begin(), candidates.end(), rng);

		const auto spacedPick = [&](float spacing) {
			const auto spacingSq = spacing * spacing;
			for (const auto& point : candidates) {
				if (chosen.size() >= placement.count) break;
				const bool farEnough = std::ranges::none_of(chosen, [&](const NiPoint3& other) { return NiPoint3::DistanceSquared(point, other) < spacingSq; });
				if (farEnough) chosen.push_back(point);
			}
		};
		spacedPick(placement.minSpacing);
		// Too close together for the spacing asked: halve it until enough fit (or no spacing at all)
		for (auto spacing = placement.minSpacing / 2.0f; chosen.size() < std::min(placement.count, candidates.size()) && spacing > 0.5f; spacing /= 2.0f) {
			chosen.clear();
			spacedPick(spacing);
		}
		if (chosen.size() < std::min(placement.count, candidates.size())) {
			chosen.assign(candidates.begin(), candidates.begin() + std::min(placement.count, candidates.size()));
		}
		for (size_t i = 0; placement.reuse && chosen.size() < placement.count; i++) chosen.push_back(chosen[i % candidates.size()]);
		return chosen;
	}

	// What a bonus event multiplies. 1 is no change.
	struct Multipliers {
		float coins{ 1.0f };
		float uscore{ 1.0f };
		float lootChance{ 1.0f };

		bool Any() const { return coins != 1.0f || uscore != 1.0f || lootChance != 1.0f; }
		bool operator==(const Multipliers&) const = default;
	};

	constexpr float ClampMultiplier(float value) {
		if (!(value >= 1.0f)) return 1.0f; // also NaN
		return std::min(value, MAX_MULTIPLIER);
	}

	struct BonusWindow {
		int64_t startsAt{};
		int64_t endsAt{};
		Multipliers multipliers;
	};

	constexpr bool InWindow(int64_t startsAt, int64_t endsAt, int64_t now) { return now >= startsAt && now < endsAt; }

	// The bonus right now: each multiplier is the largest of the running windows (they don't stack)
	inline Multipliers ActiveMultipliers(const std::vector<BonusWindow>& windows, int64_t now) {
		Multipliers active;
		for (const auto& window : windows) {
			if (!InWindow(window.startsAt, window.endsAt, now)) continue;
			active.coins = std::max(active.coins, ClampMultiplier(window.multipliers.coins));
			active.uscore = std::max(active.uscore, ClampMultiplier(window.multipliers.uscore));
			active.lootChance = std::max(active.lootChance, ClampMultiplier(window.multipliers.lootChance));
		}
		return active;
	}

	// An amount times a multiplier, rounded, without overflowing
	template<typename T>
	T Scale(T amount, float multiplier) {
		if (multiplier == 1.0f || amount <= 0) return amount;
		const auto scaled = std::round(static_cast<double>(amount) * multiplier);
		if (scaled >= static_cast<double>(std::numeric_limits<T>::max())) return std::numeric_limits<T>::max();
		return static_cast<T>(scaled);
	}

	// A drop chance (0-1) times a multiplier, never above certain
	constexpr float ScaleChance(float chance, float multiplier) {
		return multiplier == 1.0f ? chance : std::min(1.0f, chance * multiplier);
	}

	// How many invasion waves should have been sent by now: the first at the start, then one every interval
	constexpr uint32_t WavesDue(int64_t startedAt, int64_t interval, uint32_t waves, int64_t now) {
		if (now < startedAt || waves == 0) return 0;
		if (interval <= 0) return waves;
		const auto due = static_cast<uint64_t>((now - startedAt) / interval) + 1;
		return static_cast<uint32_t>(std::min<uint64_t>(due, waves));
	}

	// ---------------- Challenges ----------------

	enum class ePhase : uint8_t { SCHEDULED, ACTIVE, COMPLETED, EXPIRED, CANCELLED };

	constexpr std::string_view PhaseName(ePhase phase) {
		switch (phase) {
		case ePhase::SCHEDULED: return "scheduled";
		case ePhase::ACTIVE: return "active";
		case ePhase::COMPLETED: return "completed";
		case ePhase::EXPIRED: return "expired";
		case ePhase::CANCELLED: return "cancelled";
		}
		return "";
	}

	// state: the stored ILiveOps::eChallengeState (0 open, 1 completed, 2 expired, 3 cancelled)
	constexpr ePhase Phase(uint8_t state, int64_t startsAt, int64_t endsAt, int64_t now) {
		if (state == 1) return ePhase::COMPLETED;
		if (state == 2) return ePhase::EXPIRED;
		if (state == 3) return ePhase::CANCELLED;
		if (now < startsAt) return ePhase::SCHEDULED;
		if (now >= endsAt) return ePhase::EXPIRED;
		return ePhase::ACTIVE;
	}

	// Whole percent reached, 0-100; 100 only once the target is reached
	constexpr uint8_t Percent(int64_t total, int64_t target) {
		if (target <= 0) return 100;
		if (total >= target) return 100;
		if (total <= 0) return 0;
		return static_cast<uint8_t>(std::min<int64_t>(99, total * 100 / target));
	}

	inline const std::vector<uint8_t>& DefaultMilestones() {
		static const std::vector<uint8_t> milestones{ 25, 50, 75, 100 };
		return milestones;
	}

	// "25,50,75,100" -> sorted, without repeats, each 1-100; anything unreadable is skipped. Empty: the defaults.
	inline std::vector<uint8_t> ParseMilestones(std::string_view text) {
		std::vector<uint8_t> milestones;
		for (const auto& part : GeneralUtils::SplitString(std::string(text), ',')) {
			auto trimmed = part;
			trimmed.erase(0, trimmed.find_first_not_of(" \t"));
			trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
			const auto value = GeneralUtils::TryParse<int32_t>(trimmed);
			if (value && *value >= 1 && *value <= 100) milestones.push_back(static_cast<uint8_t>(*value));
		}
		std::ranges::sort(milestones);
		milestones.erase(std::unique(milestones.begin(), milestones.end()), milestones.end());
		return milestones.empty() ? DefaultMilestones() : milestones;
	}

	// The highest milestone reached that is above the last one announced, if any (several passed at once: one message)
	inline std::optional<uint8_t> NextMilestone(const std::vector<uint8_t>& milestones, uint8_t lastAnnounced, int64_t total, int64_t target) {
		const auto percent = Percent(total, target);
		std::optional<uint8_t> next;
		for (const auto milestone : milestones) {
			if (milestone > lastAnnounced && milestone <= percent) next = milestone;
		}
		return next;
	}

	// Whether a character's contribution earns the rewards: at least `minimum`, and always more than nothing
	constexpr bool Eligible(int64_t contribution, int64_t minimum) {
		return contribution > 0 && contribution >= std::max<int64_t>(1, minimum);
	}

	// Who gets a completed challenge's rewards: characters that contributed enough and weren't rewarded yet (a restart in
	// the middle of handing them out carries on without giving anyone twice), highest contribution first
	template<typename Id>
	std::vector<std::pair<Id, int64_t>> RewardRecipients(std::vector<std::pair<Id, int64_t>> contributions, int64_t minimum, const std::vector<Id>& alreadyRewarded) {
		std::erase_if(contributions, [&](const auto& entry) {
			return !Eligible(entry.second, minimum) || std::ranges::find(alreadyRewarded, entry.first) != alreadyRewarded.end();
		});
		std::stable_sort(contributions.begin(), contributions.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
		return contributions;
	}

	// How long after a challenge ends its end is still worth announcing in game, when the announcement couldn't go out
	// at the time (the dashboard was restarting and not yet connected to the worlds)
	constexpr int64_t END_ANNOUNCEMENT_WINDOW = 60 * 60;

	/**
	 * Whether a challenge's end still has to be announced in game: it completed (state 1) or expired (state 2), it wasn't
	 * announced yet, and it ended recently enough (`endedAt`, within END_ANNOUNCEMENT_WINDOW) that the news is still news.
	 */
	constexpr bool EndAnnouncementDue(uint8_t state, int64_t endAnnouncedAt, int64_t endedAt, int64_t now) {
		if (state != 1 && state != 2) return false;
		if (endAnnouncedAt != 0 || endedAt <= 0) return false;
		return now - endedAt <= END_ANNOUNCEMENT_WINDOW;
	}

	// Whether something recorded in a zone counts for a challenge limited to `zones` (empty: every zone)
	inline bool CountsInZone(const std::vector<uint32_t>& zones, uint32_t zone) {
		return zones.empty() || std::ranges::find(zones, zone) != zones.end();
	}
}

#endif  //!__LIVEOPSRULES__H__
