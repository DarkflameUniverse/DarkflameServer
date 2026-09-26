#include "EconomyPlaces.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <vector>

#include "BehaviorTemplate.h"
#include "CDClientDatabase.h"
#include "CDClientSchema.h"
#include "ClientAssets.h"
#include "DashboardRoutes.h"
#include "Database.h"
#include "EconomyScan.h"
#include "GameLabels.h"
#include "GeneralUtils.h"
#include "RouteUtils.h"
#include "StatisticID.h"
#include "eHTTPMethod.h"
#include "magic_enum.hpp"

using namespace RouteUtils;

namespace EconomyPlaces {
	const std::set<uint32_t>& PropertyZones() {
		static const std::set<uint32_t> zones = [] {
			std::set<uint32_t> found;
			try {
				// PropertyTemplate also lists test maps and a few worlds that never run as properties (the FV dragon crevice), so
				// only the ones players are sent to through a property entrance count
				auto result = CDClientDatabase::ExecuteQuery(
					"SELECT DISTINCT mapID FROM PropertyEntranceComponent WHERE mapID IN (SELECT mapID FROM PropertyTemplate) ORDER BY mapID;");
				for (; !result.eof(); result.nextRow()) {
					const auto map = result.getIntField("mapID", 0);
					if (map > 0) found.insert(static_cast<uint32_t>(map));
				}
			} catch (const std::exception&) {
				// No CDClient: nothing is a property zone
			}
			return found;
		}();
		return zones;
	}

	bool IsPropertyZone(uint32_t zone) {
		return PropertyZones().contains(zone);
	}

	std::string ZoneName(uint32_t zone) {
		const auto& names = ZoneNames();
		const auto key = std::to_string(zone);
		if (names.contains(key) && names[key].is_string()) return names[key].get<std::string>();
		return "Zone " + key;
	}

	std::optional<IEconomyLedger::PlaceFilter> Parse(std::string_view place) {
		IEconomyLedger::PlaceFilter filter;
		if (place.empty() || place == "all") return filter;
		const auto propertyZones = [&filter] {
			filter.zones.assign(PropertyZones().begin(), PropertyZones().end());
			// No property zones known: match nothing rather than everything
			if (filter.zones.empty()) filter.zones.push_back(0);
		};
		if (place == "properties") {
			propertyZones();
			return filter;
		}
		const auto colon = place.find(':');
		const auto zoneText = place.substr(0, colon);
		if (colon != std::string_view::npos) {
			const auto clone = GeneralUtils::TryParse<uint32_t>(place.substr(colon + 1));
			if (!clone) return std::nullopt;
			filter.clone = *clone;
		}
		if (zoneText == "*") {
			// One owner's properties: only meaningful for a real clone
			if (!filter.clone || *filter.clone == 0) return std::nullopt;
			propertyZones();
			return filter;
		}
		const auto zone = GeneralUtils::TryParse<uint32_t>(zoneText);
		if (!zone || *zone == 0) return std::nullopt;
		filter.zones.push_back(*zone);
		return filter;
	}

	Owners::Owners(const std::set<uint32_t>& clones) {
		std::vector<uint32_t> ids;
		for (const auto clone : clones) if (clone != 0) ids.push_back(clone);
		if (ids.empty()) return;
		const auto found = Database::Get()->GetCloneOwners(ids);
		for (const auto& owner : found.value("owners", nlohmann::json::array())) {
			m_Owners[owner.value("clone", 0u)] = { owner.value("character_id", ""), owner.value("name", "") };
		}
		for (const auto& property : found.value("properties", nlohmann::json::array())) {
			m_Properties[{ property.value("zone", 0u), property.value("clone", 0u) }] = { property.value("id", ""), property.value("name", "") };
		}
	}

	nlohmann::json Owners::Info(uint32_t zone, uint32_t clone) const {
		const bool property = IsPropertyZone(zone);
		const auto zoneName = ZoneName(zone);
		nlohmann::json info{
			{"place", std::to_string(zone) + ":" + std::to_string(clone)}, {"zone", zone}, {"zone_name", zoneName}, {"clone", clone},
			{"property", property}, {"unknown", property && clone == 0}, {"name", zoneName}
		};
		if (!property && clone == 0) return info;
		if (clone == 0) {
			info["name"] = "Unknown property (recorded before properties were told apart)";
			return info;
		}
		std::string ownerName;
		if (const auto owner = m_Owners.find(clone); owner != m_Owners.end()) {
			info["owner_id"] = owner->second.first;
			info["owner_name"] = ownerName = owner->second.second;
		}
		std::string propertyName;
		if (const auto it = m_Properties.find({ zone, clone }); it != m_Properties.end()) {
			info["property_id"] = it->second.first;
			info["property_name"] = propertyName = it->second.second;
		}
		const auto who = ownerName.empty() ? "clone " + std::to_string(clone) : ownerName;
		if (!propertyName.empty()) info["name"] = propertyName + " (" + who + ")";
		else if (info.contains("property_id")) info["name"] = who + "'s property";
		// A property instance the owner visited without claiming it
		else info["name"] = who + "'s unclaimed property";
		return info;
	}

	bool IsPowerupKind(IEconomyLedger::eMapEvent kind) {
		return kind == IEconomyLedger::eMapEvent::POWERUP_DROPS || kind == IEconomyLedger::eMapEvent::POWERUP_PICKUPS;
	}

	namespace {
		// What each restoring behavior gives back: the stat its parameter names ("health", "imagination", "armor")
		std::optional<std::string> Restores(BehaviorTemplate behavior) {
			switch (behavior) {
			case BehaviorTemplate::HEAL: return "Health";
			case BehaviorTemplate::IMAGINATION: return "Imagination";
			case BehaviorTemplate::REPAIR_ARMOR: return "Armor";
			default: return std::nullopt;
			}
		}

		std::string ClassifyPowerup(LOT lot) {
			constexpr size_t MAX_NODES = 64;
			std::vector<int64_t> queue;
			std::set<int64_t> seen;
			auto skills = CDClientDatabase::CreatePreppedStmt(
				"SELECT sb.behaviorID FROM ObjectSkills os JOIN SkillBehavior sb ON sb.skillID = os.skillID WHERE os.objectTemplate = ? ORDER BY os.skillID;");
			skills.bind(1, static_cast<int>(lot));
			for (auto result = skills.execQuery(); !result.eof(); result.nextRow()) {
				const auto id = result.getInt64Field("behaviorID", 0);
				if (id > 0 && seen.insert(id).second) queue.push_back(id);
			}

			// Breadth first through the behavior tree, following the parameters the server reads as child behaviors
			std::vector<std::string> restores;
			std::optional<BehaviorTemplate> firstEffect;
			for (size_t i = 0; i < queue.size() && i < MAX_NODES; i++) {
				auto templateStmt = CDClientDatabase::CreatePreppedStmt("SELECT templateID FROM BehaviorTemplate WHERE behaviorID = ?;");
				templateStmt.bind(1, static_cast<sqlite_int64>(queue[i]));
				auto templateRow = templateStmt.execQuery();
				if (templateRow.eof()) continue;
				const auto behavior = magic_enum::enum_cast<BehaviorTemplate>(static_cast<unsigned int>(templateRow.getIntField("templateID", 0)));

				bool hasChildren = false;
				auto params = CDClientDatabase::CreatePreppedStmt("SELECT parameterID, value FROM BehaviorParameter WHERE behaviorID = ? ORDER BY parameterID;");
				params.bind(1, static_cast<sqlite_int64>(queue[i]));
				for (auto param = params.execQuery(); !param.eof(); param.nextRow()) {
					const std::string name = param.getStringField("parameterID", "");
					const auto value = param.getFloatField("value", 0);
					if (!CDClientSchema::IsChildBehaviorParameter(name) || value <= 0 || std::floor(value) != value) continue;
					hasChildren = true;
					const auto child = static_cast<int64_t>(value);
					if (seen.insert(child).second) queue.push_back(child);
				}
				if (!behavior) continue;
				if (const auto stat = Restores(*behavior)) {
					if (std::ranges::find(restores, *stat) == restores.end()) restores.push_back(*stat);
				} else if (!hasChildren && !firstEffect && *behavior != BehaviorTemplate::PLAY_EFFECT && *behavior != BehaviorTemplate::EMPTY) {
					firstEffect = *behavior;
				}
			}
			if (!restores.empty()) {
				std::string type;
				for (const auto& stat : restores) type += (type.empty() ? "" : " + ") + stat;
				return type;
			}
			return firstEffect ? GameLabels::Name(*firstEffect) : "Other";
		}
	}

	std::string PowerupType(LOT lot) {
		static std::map<LOT, std::string> cache;
		if (const auto it = cache.find(lot); it != cache.end()) return it->second;
		std::string type = "Other";
		try {
			type = ClassifyPowerup(lot);
		} catch (const std::exception&) {
			// No CDClient
		}
		return cache[lot] = type;
	}

	namespace {
		uint32_t Today() {
			return static_cast<uint32_t>(std::time(nullptr) / (24 * 60 * 60));
		}

		std::pair<uint32_t, uint32_t> RequestedDays(const HTTPContext& context) {
			return EconomyScan::DayRange(
				GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "from")),
				GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "to")),
				Today(), 30, 366);
		}

		struct PlaceTotals {
			int64_t events{};
			int64_t stats{};
		};

		// Every (zone, clone) with map events or statistics in the range
		std::map<std::pair<uint32_t, uint32_t>, PlaceTotals> PlacesWithData(uint32_t from, uint32_t to) {
			std::map<std::pair<uint32_t, uint32_t>, PlaceTotals> places;
			for (const auto& row : Database::Get()->GetMapZonesAllKinds(from, to)) {
				if (!magic_enum::enum_cast<IEconomyLedger::eMapEvent>(row.value("kind", 0))) continue;
				places[{ row.value("zone", 0u), row.value("clone", 0u) }].events += row.value("events", int64_t{ 0 });
			}
			for (const auto& row : Database::Get()->GetPlayerStatsPerZone(from, to, false)) {
				places[{ row.value("zone", 0u), row.value("clone", 0u) }].stats += row.value("amount", int64_t{ 0 });
			}
			return places;
		}
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/api/reports/places", Perm("reports_view"),
			"Places with map events or player statistics in a range, as the reports group them: everywhere, all properties, each property zone's "
			"properties, each property and each other world. Query: ?from=&to=&zone= (only that zone's places)&place= (a *:<clone> place to name too). "
			"Returns {places: [{place, name, group, events, stats, ...property info}], propertyZones}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto [from, to] = RequestedDays(context);
				const auto onlyZone = GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "zone")).value_or(0);
				const auto data = PlacesWithData(from, to);
				std::set<uint32_t> clones;
				for (const auto& [key, totals] : data) clones.insert(key.second);
				const Owners owners(clones);

				PlaceTotals allProperties;
				std::map<uint32_t, PlaceTotals> propertyZoneTotals;
				std::map<uint32_t, PlaceTotals> worlds;
				std::vector<nlohmann::json> properties;
				for (const auto& [key, totals] : data) {
					const auto [zone, clone] = key;
					if (onlyZone && zone != onlyZone) continue;
					if (!IsPropertyZone(zone)) {
						worlds[zone].events += totals.events;
						worlds[zone].stats += totals.stats;
						continue;
					}
					allProperties.events += totals.events;
					allProperties.stats += totals.stats;
					propertyZoneTotals[zone].events += totals.events;
					propertyZoneTotals[zone].stats += totals.stats;
					auto info = owners.Info(zone, clone);
					info["group"] = ZoneName(zone) + " properties";
					info["events"] = totals.events;
					info["stats"] = totals.stats;
					properties.push_back(std::move(info));
				}
				std::ranges::sort(properties, [](const nlohmann::json& a, const nlohmann::json& b) {
					if (a["zone"] != b["zone"]) return a["zone"].get<uint32_t>() < b["zone"].get<uint32_t>();
					return a["events"].get<int64_t>() + a["stats"].get<int64_t>() > b["events"].get<int64_t>() + b["stats"].get<int64_t>();
				});

				nlohmann::json places = nlohmann::json::array();
				if (!onlyZone) {
					places.push_back({ {"place", ""}, {"name", "Everywhere"}, {"group", ""} });
					if (!propertyZoneTotals.empty()) {
						places.push_back({ {"place", "properties"}, {"name", "All properties"}, {"group", "Properties"}, {"events", allProperties.events}, {"stats", allProperties.stats} });
					}
					// All of one owner's properties (?place=*:<clone>, from a character page)
					const auto asked = QueryValue(context.queryString, "place");
					const auto askedFilter = asked.starts_with("*:") ? Parse(asked) : std::nullopt;
					if (askedFilter && askedFilter->clone) {
						const Owners owner({ *askedFilter->clone });
						const auto info = owner.Info(0, *askedFilter->clone);
						nlohmann::json entry{ {"place", asked}, {"group", "Properties"}, {"property", true}, {"clone", *askedFilter->clone},
							{"name", (info.contains("owner_name") ? info["owner_name"].get<std::string>() : "Clone " + std::to_string(*askedFilter->clone)) + "'s properties"} };
						if (info.contains("owner_id")) { entry["owner_id"] = info["owner_id"]; entry["owner_name"] = info["owner_name"]; }
						places.push_back(std::move(entry));
					}
				}
				for (const auto& [zone, totals] : propertyZoneTotals) {
					places.push_back({ {"place", std::to_string(zone)}, {"zone", zone}, {"name", ZoneName(zone) + " properties"}, {"group", "Properties"},
						{"property", true}, {"events", totals.events}, {"stats", totals.stats} });
				}
				for (auto& property : properties) places.push_back(std::move(property));
				for (const auto& [zone, totals] : worlds) {
					places.push_back({ {"place", std::to_string(zone)}, {"zone", zone}, {"name", ZoneName(zone)}, {"group", "Worlds"}, {"property", false},
						{"events", totals.events}, {"stats", totals.stats} });
				}
				nlohmann::json zones = nlohmann::json::array();
				for (const auto zone : PropertyZones()) zones.push_back({ {"zone", zone}, {"name", ZoneName(zone)} });
				JsonReply(reply, eHTTPStatusCode::OK, { {"from", from}, {"to", to}, {"places", places}, {"propertyZones", zones} });
			});

		Route(eHTTPMethod::GET, "/api/reports/place", Perm("reports_view"),
			"Totals for one place (see /api/reports/places) in a range: map events per kind, player statistics, powerups by type and, for one "
			"property, who it belongs to, its placed models and (with players_history) who was seen there. Query: ?place=&from=&to=&staff=1",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto placeText = QueryValue(context.queryString, "place");
				const auto place = Parse(placeText);
				if (!place) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "place must be empty, properties, <zone>, <zone>:<clone> or *:<clone>");
				const auto [from, to] = RequestedDays(context);
				const bool excludeStaff = QueryValue(context.queryString, "staff") != "1";

				std::map<int, std::pair<int64_t, int64_t>> kinds;
				for (const auto& row : Database::Get()->GetMapEventsPerDay(from, to, *place)) {
					auto& [events, quantity] = kinds[row.value("kind", 0)];
					events += row.value("events", int64_t{ 0 });
					quantity += row.value("quantity", int64_t{ 0 });
				}
				nlohmann::json events = nlohmann::json::array();
				for (const auto& [kind, totals] : kinds) {
					const auto value = magic_enum::enum_cast<IEconomyLedger::eMapEvent>(static_cast<uint8_t>(kind));
					events.push_back({ {"kind", kind}, {"name", value ? GameLabels::Name(*value) : std::to_string(kind)}, {"events", totals.first}, {"quantity", totals.second} });
				}
				std::map<int, int64_t> statTotals;
				for (const auto& row : Database::Get()->GetPlayerStatsPerDay(from, to, excludeStaff, *place)) statTotals[row.value("stat", 0)] += row.value("amount", int64_t{ 0 });
				nlohmann::json stats = nlohmann::json::array();
				for (const auto& [stat, amount] : statTotals) {
					const auto value = magic_enum::enum_cast<StatisticID>(static_cast<uint32_t>(stat));
					stats.push_back({ {"stat", stat}, {"name", value ? GameLabels::Name(*value) : std::to_string(stat)}, {"amount", amount} });
				}
				nlohmann::json powerups = nlohmann::json::array();
				for (const auto kind : { IEconomyLedger::eMapEvent::POWERUP_DROPS, IEconomyLedger::eMapEvent::POWERUP_PICKUPS }) {
					std::map<std::string, int64_t> byType;
					for (const auto& row : Database::Get()->GetMapEventsByLot(kind, from, to, *place, 20000)) byType[PowerupType(row.value("lot", 0))] += row.value("events", int64_t{ 0 });
					for (const auto& [type, count] : byType) powerups.push_back({ {"kind", static_cast<int>(kind)}, {"type", type}, {"events", count} });
				}

				nlohmann::json response{ {"place", placeText}, {"from", from}, {"to", to}, {"events", events}, {"stats", stats}, {"powerups", powerups} };
				// One zone and clone: who it belongs to, what is built there and who went there
				if (place->zones.size() == 1 && place->clone && placeText.find('*') == std::string::npos) {
					const auto zone = place->zones[0];
					const auto clone = *place->clone;
					const Owners owners({ clone });
					auto info = owners.Info(zone, clone);
					if (info.contains("property_id")) {
						const auto propertyId = GeneralUtils::TryParse<LWOOBJID>(info["property_id"].get<std::string>());
						nlohmann::json models = nlohmann::json::array();
						if (propertyId) {
							for (const auto& model : Database::Get()->GetPropertyModels(*propertyId)) {
								models.push_back({ {"lot", model.lot}, {"name", model.lot == 14 ? "Player-built model" : ClientAssets::ItemName(model.lot)},
									{"x", model.position.x}, {"z", model.position.z} });
							}
						}
						response["models"] = models;
					}
					response["info"] = info;
					if (info.value("property", false) && clone != 0) {
						if (Can(context, "players_history")) {
							const int64_t fromTime = static_cast<int64_t>(from) * 24 * 60 * 60, toTime = (static_cast<int64_t>(to) + 1) * 24 * 60 * 60 - 1;
							response["visitors"] = Database::Get()->GetCloneVisitors(zone, clone, fromTime, toTime, 200);
						} else {
							response["visitorsHidden"] = true;
						}
					}
				} else if (place->clone && *place->clone != 0) {
					// All of one owner's properties
					const Owners owners({ *place->clone });
					auto info = owners.Info(place->zones.empty() ? 0 : place->zones[0], *place->clone);
					response["owner"] = { {"owner_id", info.value("owner_id", "")}, {"owner_name", info.value("owner_name", "")} };
				}
				JsonReply(reply, eHTTPStatusCode::OK, response);
			});
	}
}
