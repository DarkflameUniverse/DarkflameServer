#ifndef __OUTDATEDINSTANCES__H__
#define __OUTDATEDINSTANCES__H__

#include <chrono>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

#include "master/InstanceMigration.h"

/**
 * World instances on an old version: started before a live update (old binary) or on zone files that changed since
 * (docs/LiveUpdate.md, docs/WorldHotReload.md). Master marks them outdated; nobody new is sent to one (AcceptsNewPlayers)
 * and a request for its zone goes to a new instance instead.
 *
 * Properties (clone instances) are never replaced or moved: players may have building in progress that isn't saved.
 * An outdated property keeps running until everyone left, reminding its players now and then that an update is
 * waiting, then stops. A new instance of the same property only starts once the old one is gone, so two instances of
 * one property never both save its models; whoever asks for it meanwhile waits for the new one.
 *
 * Header only, no master state, so it is unit tested.
 */
namespace OutdatedInstances {
	using Clock = std::chrono::steady_clock;

	// Between two reminders to players on an outdated property
	constexpr auto NOTICE_INTERVAL = std::chrono::minutes(10);

	inline constexpr const char* NOTICE_TITLE = "Server update";
	inline constexpr const char* NOTICE_MESSAGE =
		"A server update is available. This property keeps running on the old version until everyone has left it: "
		"leave and come back to get the update. Nothing you built is lost.";

	// Property instances hold building in progress that lives only in that world
	inline bool IsProperty(const InstanceMigration::InstanceView& view) {
		return view.cloneId != 0;
	}

	/**
	 * Whether players on this instance are due a reminder: an outdated property with players, once at first and then
	 * every interval while they stay. lastNotice is when the last one went out (none yet: nullopt).
	 */
	inline bool NoticeDue(const InstanceMigration::InstanceView& view, std::optional<Clock::time_point> lastNotice, Clock::time_point now,
		Clock::duration interval = NOTICE_INTERVAL) {
		if (!view.outdated || !IsProperty(view) || view.players <= 0 || !view.ready || view.shuttingDown) return false;
		return !lastNotice || now - *lastNotice >= interval;
	}

	/**
	 * Whether an outdated instance nobody is in or on the way to should stop now. Instances a live update or reload
	 * handles itself are left to it: ones being emptied (draining), character selection, and public instances of zones
	 * that always keep one (they get their new instance first).
	 * hasPending: players on their way (affirmations or requests waiting for it)
	 */
	inline bool ShouldStop(const InstanceMigration::InstanceView& view, bool keepZone, bool hasPending) {
		if (!view.outdated || view.shuttingDown || view.draining || !view.ready) return false;
		if (view.Load() > 0 || hasPending) return false;
		if (view.zoneId == 0) return false;
		if (!IsProperty(view) && !view.isPrivate && keepZone) return false;
		return true;
	}

	/**
	 * Whether a new instance of property zone/clone must wait before it starts: an instance of the same property is still
	 * running (outdated, possibly still saving on its way out). Starting the new one then would have two worlds save the
	 * same property.
	 */
	inline bool MustWaitForOld(const std::vector<InstanceMigration::InstanceView>& running, uint32_t zone, uint32_t clone) {
		if (clone == 0) return false;
		for (const auto& view : running) {
			if (view.zoneId == zone && view.cloneId == clone && view.outdated) return true;
		}
		return false;
	}
}

#endif  //!__OUTDATEDINSTANCES__H__
