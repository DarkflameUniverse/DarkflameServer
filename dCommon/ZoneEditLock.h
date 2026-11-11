#pragma once

#include <cstdint>
#include <optional>

/**
 * Who may edit a zone's files (docs/ZonePublish.md). A zone has up to MAX_EDITORS assigned editors and at most one
 * lock holder at a time; a set can only be sent, restored or cleared by the account holding the zone's lock. A lock
 * never runs out by itself: its holder gives it up, or an admin takes it away.
 *
 * Pure rules, the same whichever way a request arrives; the caller looks up the account's GM level (now, never from
 * anything the requester says), the zone's holder and whether the account is assigned.
 */
namespace ZoneEditLock {
	constexpr uint32_t MAX_EDITORS = 10;
	constexpr uint8_t DEFAULT_PUBLISH_LEVEL = 9;
	constexpr uint8_t DEFAULT_ADMIN_LEVEL = 9;

	// The numbers are part of what requesters are told; never change them
	enum class eResult : uint8_t {
		OK = 0,
		LEVEL_TOO_LOW = 1,    // the account's level is below the level editing needs
		NOT_ASSIGNED = 2,     // not one of the zone's editors (and not an admin)
		LOCKED_BY_OTHER = 3,  // another account holds the zone's lock
		NO_CHANGE = 4,        // nothing to do: already the holder, or nothing was locked
	};

	struct Levels {
		uint8_t publish = DEFAULT_PUBLISH_LEVEL; // zone_publish_gm_level
		uint8_t admin = DEFAULT_ADMIN_LEVEL;     // zone_lock_admin_gm_level
	};

	struct Actor {
		uint32_t accountId{};
		uint8_t gmLevel{};
		bool assigned{};      // one of the zone's editors
	};

	inline bool IsAdmin(const Actor& actor, const Levels& levels) {
		return actor.gmLevel >= levels.admin;
	}

	// Taking the zone's lock; holder: the account holding it now
	inline eResult Take(const Actor& actor, std::optional<uint32_t> holder, const Levels& levels) {
		if (actor.accountId == 0 || actor.gmLevel < levels.publish) return eResult::LEVEL_TOO_LOW;
		if (!actor.assigned && !IsAdmin(actor, levels)) return eResult::NOT_ASSIGNED;
		if (holder && *holder == actor.accountId) return eResult::NO_CHANGE;
		if (holder) return eResult::LOCKED_BY_OTHER;
		return eResult::OK;
	}

	// Giving the lock up: its holder, or an admin taking it from whoever holds it
	inline eResult Release(const Actor& actor, std::optional<uint32_t> holder, const Levels& levels) {
		if (actor.accountId == 0 || actor.gmLevel < levels.publish) return eResult::LEVEL_TOO_LOW;
		if (!holder) return eResult::NO_CHANGE;
		if (*holder == actor.accountId || IsAdmin(actor, levels)) return eResult::OK;
		return eResult::LOCKED_BY_OTHER;
	}

	/**
	 * Changing the zone's files (a new set, a restore, a clear): only the lock's holder. OK, LEVEL_TOO_LOW,
	 * LOCKED_BY_OTHER, or NOT_ASSIGNED when nobody holds the lock (it has to be taken first).
	 */
	inline eResult Change(const Actor& actor, std::optional<uint32_t> holder, const Levels& levels) {
		if (actor.accountId == 0 || actor.gmLevel < levels.publish) return eResult::LEVEL_TOO_LOW;
		if (!holder) return eResult::NOT_ASSIGNED;
		if (*holder != actor.accountId) return eResult::LOCKED_BY_OTHER;
		return eResult::OK;
	}

	// What happens to the zone's running instances after its files changed
	enum class eReload : uint8_t {
		SKIPPED,  // not asked for: the files are read when an instance next starts
		STARTED,  // the zone's instances are being replaced now
		HELD,     // players are there and they were not to be moved: waits until someone says to
	};

	inline eReload PlanReload(bool reload, uint32_t players, bool movePlayers) {
		if (!reload) return eReload::SKIPPED;
		return players > 0 && !movePlayers ? eReload::HELD : eReload::STARTED;
	}

	inline const char* ReloadName(eReload reload) {
		switch (reload) {
		case eReload::SKIPPED: return "skipped";
		case eReload::STARTED: return "started";
		case eReload::HELD: return "held";
		}
		return "skipped";
	}
}
