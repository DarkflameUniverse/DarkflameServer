#ifndef __INSTANCELIMITS__H__
#define __INSTANCELIMITS__H__

#include <cstdint>

#include "eTransferFailure.h"

namespace InstanceLimits {
	// The most world instances that run at once when the config does not say
	constexpr uint32_t DefaultMaxInstances = 256;

	enum class eNewInstance : uint8_t {
		Allowed,
		UnknownZone,
		TooManyInstances
	};

	/**
	 * Whether a new world instance may be started for a zone.
	 * @param mapID the zone asked for, 0 is the character select and has no row in the ZoneTable
	 * @param isInZoneTable whether the ZoneTable has a row for the zone
	 * @param runningInstances the instances that run already
	 * @param maxInstances the most instances that may run at once, 0 for no limit
	 */
	constexpr eNewInstance CheckNewInstance(const uint32_t mapID, const bool isInZoneTable, const uint64_t runningInstances, const uint32_t maxInstances) {
		// Players always have to be able to reach the character select
		if (mapID == 0) return eNewInstance::Allowed;
		if (!isInZoneTable) return eNewInstance::UnknownZone;
		if (maxInstances != 0 && runningInstances >= maxInstances) return eNewInstance::TooManyInstances;
		return eNewInstance::Allowed;
	}

	/**
	 * What the player who asked for the zone is told when no instance was started for it.
	 */
	constexpr eTransferFailure TransferFailure(const eNewInstance newInstance) {
		switch (newInstance) {
		case eNewInstance::UnknownZone: return eTransferFailure::MapLoad;
		case eNewInstance::TooManyInstances: return eTransferFailure::NoServer;
		default: return eTransferFailure::Other;
		}
	}
}

#endif  //!__INSTANCELIMITS__H__
