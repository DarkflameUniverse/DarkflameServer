#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "dCommonVars.h"
#include "IEconomyLedger.h"
#include "json.hpp"

/**
 * Where economy map events and player statistics happened, as the reports group them.
 *
 * Property worlds run one instance per property on the property's zone, told apart by the clone id the world records
 * (the owner's property clone). All properties of a zone mixed together would be meaningless (different builds on the
 * same ground), so the reports show every property together ("All properties"), every property of one zone, or one
 * property. Rows written before the clone was recorded have clone 0 and show as an unknown property.
 *
 * A place is named by a short text the pages pass around as ?place=:
 *   ""             everywhere
 *   "properties"   every property zone, every clone
 *   "<zone>"       one zone, every instance (on a property zone: all its properties)
 *   "<zone>:<n>"   one zone's clone n (a property; 0 on a property zone: unknown property)
 *   "*:<n>"        clone n on every property zone (all of one owner's properties)
 */
namespace EconomyPlaces {
	// Zones the game runs properties on: the CDClient's PropertyTemplate maps that a PropertyEntranceComponent sends players to
	const std::set<uint32_t>& PropertyZones();
	bool IsPropertyZone(uint32_t zone);

	// A zone's name as the dashboard shows it, or "Zone <id>"
	std::string ZoneName(uint32_t zone);

	// The filter for a place text; nullopt when it isn't one
	std::optional<IEconomyLedger::PlaceFilter> Parse(std::string_view place);

	// Who the clones belong to, read once for a batch of rows
	class Owners {
	public:
		// Reads the owners of every non-zero clone given
		explicit Owners(const std::set<uint32_t>& clones);

		/**
		 * One (zone, clone) as the pages show it: {place, zone, zone_name, clone, property (a property zone), unknown
		 * (a property zone's clone 0), name, owner_id, owner_name, property_id, property_name}
		 */
		nlohmann::json Info(uint32_t zone, uint32_t clone) const;

	private:
		std::map<uint32_t, std::pair<std::string, std::string>> m_Owners; // clone -> (character id, name)
		std::map<std::pair<uint32_t, uint32_t>, std::pair<std::string, std::string>> m_Properties; // (zone, clone) -> (property id, name)
	};

	// Map event kinds whose LOT is a powerup
	bool IsPowerupKind(IEconomyLedger::eMapEvent kind);

	/**
	 * What a powerup restores, from its skills' behaviors in the CDClient: "Health", "Imagination" or "Armor" (the
	 * heal, imagination and armor-repair behaviors anywhere in its behavior tree), otherwise the name of its first
	 * effect behavior (e.g. "Speed"), or "Other"
	 */
	std::string PowerupType(LOT lot);

	// /api/reports/places and /api/reports/property
	void RegisterRoutes();
}
