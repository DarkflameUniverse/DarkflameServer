#include "ReportRoutes.h"
#include "ZonePaths.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <map>
#include <set>
#include <mutex>

#include "RouteUtils.h"
#include "EconomyScan.h"
#include "TerrainMap.h"
#include "SceneColor.h"
#include "WSRoutes.h"
#include "Background.h"
#include "PlayerActions.h"
#include "CDClientDatabase.h"
#include "mongoose.h"
#include "ClientAssets.h"
#include "Database.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"
#include "eInventoryType.h"
#include "eLootSourceType.h"
#include "magic_enum.hpp"
#include "LevelObjects.h"
#include "GameLabels.h"
#include "StatisticID.h"
#include "ItemTrace.h"
#include "EconomyPlaces.h"

using namespace RouteUtils;

namespace {
	constexpr uint32_t DEFAULT_DAYS = 30;
	constexpr uint32_t MAX_DAYS = 366;

	uint32_t Today() {
		return static_cast<uint32_t>(std::time(nullptr) / (24 * 60 * 60));
	}

	std::pair<uint32_t, uint32_t> RequestedDays(const HTTPContext& context) {
		return EconomyScan::DayRange(
			GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "from")),
			GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "to")),
			Today(), DEFAULT_DAYS, MAX_DAYS);
	}

	// Staff are left out unless ?staff=1, so moderators testing things do not skew the numbers
	bool ExcludeStaff(const HTTPContext& context) {
		return QueryValue(context.queryString, "staff") != "1";
	}

	nlohmann::json SourceNames() {
		nlohmann::json names = nlohmann::json::object();
		for (const auto source : magic_enum::enum_values<eLootSourceType>()) {
			names[std::to_string(static_cast<uint32_t>(source))] = std::string(magic_enum::enum_name(source));
		}
		names[std::to_string(IEconomyLedger::DONATION_SOURCE)] = "DONATION";
		names[std::to_string(IEconomyLedger::HARDCORE_DEATH_SOURCE)] = "HARDCORE_DEATH";
		names[std::to_string(IEconomyLedger::HARDCORE_KILL_SOURCE)] = "HARDCORE_KILL";
		return names;
	}

	std::map<LWOOBJID, std::string> CharacterNames() {
		std::map<LWOOBJID, std::string> names;
		for (auto& [id, name] : Database::Get()->GetCharacterIdsAndNames()) names.emplace(id, std::move(name));
		return names;
	}

	std::string NameOf(const std::map<LWOOBJID, std::string>& names, LWOOBJID id) {
		const auto it = names.find(id);
		return it == names.end() ? "" : it->second;
	}

	// Items that are in the game for real: not set sub-items, and not sold back to a vendor
	bool CountsAsHeld(const EconomyScan::InventoryItem& item) {
		return !item.isProxy && item.inventoryType != eInventoryType::VENDOR_BUYBACK && item.inventoryType != eInventoryType::VENDOR;
	}

	std::string InventoryName(uint32_t type) {
		if (type >= NUMBER_OF_INVENTORIES) return std::to_string(type);
		return GameLabels::Name(static_cast<eInventoryType>(type)); // VAULT_ITEMS -> "Vault Items", as the other pages show it
	}

	nlohmann::json LocationJson(const EconomyScan::Location& location, const std::map<LWOOBJID, std::string>& names) {
		const bool mail = location.kind == EconomyScan::Location::eKind::MAIL;
		nlohmann::json json{
			{"where", mail ? "mail" : "inventory"},
			{"character_id", std::to_string(location.ownerId)},
			{"character_name", NameOf(names, location.ownerId)},
			{"lot", location.lot},
			{"name", ClientAssets::ItemName(location.lot)},
			{"count", location.count}
		};
		if (mail) json["mail_id"] = std::to_string(location.mailId);
		else {
			json["inventory"] = InventoryName(location.inventoryType);
			json["character_version"] = location.characterVersion;
			json["new_id_at_login"] = location.NewIdAtLogin();
		}
		return json;
	}

	// What the next logins do to an id held in several places (EconomyScan::ResolveOnLogin), for the pages
	nlohmann::json LoginFixJson(const std::vector<EconomyScan::Location>& copies, const std::map<LWOOBJID, std::string>& names) {
		const auto fix = EconomyScan::ResolveOnLogin(copies);
		nlohmann::json characters = nlohmann::json::array();
		for (const auto id : fix.characters) characters.push_back({ {"character_id", std::to_string(id)}, {"character_name", NameOf(names, id)} });
		return { {"resolves", fix.resolves}, {"characters", characters}, {"logins_needed", fix.loginsNeeded} };
	}

	// Result of the last full duplicate scan; scans read every character, so they only run when asked
	struct DuplicateScan {
		int64_t time{};
		double seconds{};
		size_t characters{};
		size_t items{};
		nlohmann::json duplicates = nlohmann::json::array();
		// Object ids shared by different LOTs: old data reused ids, so these are not dupes (see EconomyScan::Classify)
		nlohmann::json collisions = nlohmann::json::array();
		nlohmann::json heldByLot = nlohmann::json::array();
	};
	std::optional<DuplicateScan> g_LastScan;

	constexpr size_t MAX_DUPLICATES_SHOWN = 500;
	constexpr size_t MAX_LOTS_SHOWN = 200;

	nlohmann::json RawLocation(const EconomyScan::Location& location) {
		return { location.kind == EconomyScan::Location::eKind::MAIL ? 1 : 0, location.ownerId, location.mailId, location.inventoryType, location.lot, location.count, location.characterVersion };
	}

	EconomyScan::Location FromRawLocation(const nlohmann::json& raw) {
		return { raw[0].get<int>() == 1 ? EconomyScan::Location::eKind::MAIL : EconomyScan::Location::eKind::CHARACTER,
			raw[1].get<LWOOBJID>(), raw[2].get<uint64_t>(), raw[3].get<uint32_t>(), raw[4].get<LOT>(), raw[5].get<uint32_t>(), raw.size() > 6 ? raw[6].get<uint32_t>() : 0u };
	}

	// Worker thread: read every character and unclaimed mail. Only ids and counts; names are added on the main thread.
	nlohmann::json ScanDuplicatesRaw(GameDatabase& db) {
		const auto started = std::chrono::steady_clock::now();
		EconomyScan::DuplicateFinder finder;
		std::map<LOT, std::pair<uint64_t, uint32_t>> held; // lot -> (count, holders)
		size_t characters = 0;

		db.ForEachCharacterXml([&](LWOOBJID characterId, const std::string& xml) {
			characters++;
			std::set<LOT> lotsHere;
			const auto version = EconomyScan::CharacterVersion(xml);
			EconomyScan::ForEachInventoryItem(xml, [&](const EconomyScan::InventoryItem& item) {
				if (item.isProxy) return;
				finder.Add(item.id, { EconomyScan::Location::eKind::CHARACTER, characterId, 0, item.inventoryType, item.lot, item.count, version });
				if (!CountsAsHeld(item)) return;
				held[item.lot].first += item.count;
				if (lotsHere.insert(item.lot).second) held[item.lot].second++;
			});
		});
		db.ForEachMailAttachment([&](const IEconomyLedger::MailAttachment& attachment) {
			finder.Add(attachment.itemId, { EconomyScan::Location::eKind::MAIL, attachment.receiverId, attachment.mailId, 0, attachment.lot, attachment.count });
			held[attachment.lot].first += attachment.count;
		});

		const auto rawCopies = [](const std::vector<EconomyScan::Location>& locations) {
			nlohmann::json copies = nlohmann::json::array();
			for (const auto& location : locations) copies.push_back(RawLocation(location));
			return copies;
		};
		nlohmann::json duplicates = nlohmann::json::array();
		nlohmann::json collisions = nlohmann::json::array();
		for (const auto& [id, locations] : finder.Duplicates()) {
			const auto groups = EconomyScan::Classify(locations);
			for (const auto& copies : groups.duplicates) {
				if (duplicates.size() < MAX_DUPLICATES_SHOWN) duplicates.push_back({ {"id", id}, {"copies", rawCopies(copies)} });
			}
			if (groups.collision && collisions.size() < MAX_DUPLICATES_SHOWN) {
				collisions.push_back({ {"id", id}, {"copies", rawCopies(locations)}, {"duplicated", !groups.duplicates.empty()} });
			}
		}
		std::vector<std::pair<LOT, std::pair<uint64_t, uint32_t>>> byCount(held.begin(), held.end());
		std::ranges::sort(byCount, [](const auto& a, const auto& b) { return a.second.first > b.second.first; });
		nlohmann::json heldByLot = nlohmann::json::array();
		for (const auto& [lot, totals] : byCount) {
			if (heldByLot.size() >= MAX_LOTS_SHOWN) break;
			heldByLot.push_back({ lot, totals.first, totals.second });
		}
		return {
			{"characters", characters}, {"items", finder.ItemsSeen()}, {"duplicates", duplicates}, {"collisions", collisions}, {"held", heldByLot},
			{"seconds", std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count()}
		};
	}

	// Main thread: add character and item names to a raw scan
	DuplicateScan FinishDuplicateScan(const nlohmann::json& raw) {
		DuplicateScan scan;
		const auto names = CharacterNames();
		for (const auto& duplicate : raw["duplicates"]) {
			nlohmann::json copies = nlohmann::json::array();
			std::vector<EconomyScan::Location> locations;
			LOT lot = 0;
			for (const auto& copy : duplicate["copies"]) {
				const auto location = FromRawLocation(copy);
				lot = location.lot;
				locations.push_back(location);
				copies.push_back(LocationJson(location, names));
			}
			scan.duplicates.push_back({ {"item_id", std::to_string(duplicate["id"].get<LWOOBJID>())}, {"lot", lot}, {"name", ClientAssets::ItemName(lot)}, {"copies", copies},
				{"login_fix", LoginFixJson(locations, names)} });
		}
		for (const auto& collision : raw.value("collisions", nlohmann::json::array())) {
			nlohmann::json copies = nlohmann::json::array();
			std::vector<EconomyScan::Location> locations;
			for (const auto& copy : collision["copies"]) {
				locations.push_back(FromRawLocation(copy));
				copies.push_back(LocationJson(locations.back(), names));
			}
			scan.collisions.push_back({ {"item_id", std::to_string(collision["id"].get<LWOOBJID>())}, {"duplicated", collision["duplicated"]}, {"copies", copies},
				{"login_fix", LoginFixJson(locations, names)} });
		}
		for (const auto& entry : raw["held"]) {
			const auto lot = entry[0].get<LOT>();
			scan.heldByLot.push_back({ {"lot", lot}, {"name", ClientAssets::ItemName(lot)}, {"count", entry[1]}, {"holders", entry[2]} });
		}
		scan.characters = raw.value("characters", size_t{ 0 });
		scan.items = raw.value("items", size_t{ 0 });
		scan.seconds = raw.value("seconds", 0.0);
		scan.time = std::time(nullptr);
		return scan;
	}

	// Worker thread: where an object id is now
	nlohmann::json FindObjectRaw(GameDatabase& db, LWOOBJID itemId) {
		nlohmann::json locations = nlohmann::json::array();
		db.ForEachCharacterXmlContaining("id=\"" + std::to_string(itemId) + "\"", [&](LWOOBJID characterId, const std::string& xml) {
			EconomyScan::ForEachInventoryItem(xml, [&](const EconomyScan::InventoryItem& item) {
				if (item.id == itemId) locations.push_back(RawLocation({ EconomyScan::Location::eKind::CHARACTER, characterId, 0, item.inventoryType, item.lot, item.count, EconomyScan::CharacterVersion(xml) }));
			});
		});
		db.ForEachMailAttachment([&](const IEconomyLedger::MailAttachment& attachment) {
			if (attachment.itemId == itemId) locations.push_back(RawLocation({ EconomyScan::Location::eKind::MAIL, attachment.receiverId, attachment.mailId, 0, attachment.lot, attachment.count }));
		});
		return locations;
	}

	// Worker thread: where each of several object ids is now: {"<id>": [raw locations]}. A few ids are looked up one
	// by one (the database narrows it down); more are found in one pass over every character.
	nlohmann::json FindObjectsRaw(GameDatabase& db, const std::vector<LWOOBJID>& itemIds) {
		std::map<LWOOBJID, nlohmann::json> found;
		for (const auto id : itemIds) found[id] = nlohmann::json::array();
		const auto visitCharacter = [&](LWOOBJID characterId, const std::string& xml) {
			EconomyScan::ForEachInventoryItem(xml, [&](const EconomyScan::InventoryItem& item) {
				const auto it = found.find(item.id);
				if (it != found.end()) it->second.push_back(RawLocation({ EconomyScan::Location::eKind::CHARACTER, characterId, 0, item.inventoryType, item.lot, item.count, EconomyScan::CharacterVersion(xml) }));
			});
		};
		if (itemIds.size() <= 4) {
			for (const auto id : itemIds) {
				db.ForEachCharacterXmlContaining("id=\"" + std::to_string(id) + "\"", [&](LWOOBJID characterId, const std::string& xml) {
					EconomyScan::ForEachInventoryItem(xml, [&](const EconomyScan::InventoryItem& item) {
						if (item.id == id) found[id].push_back(RawLocation({ EconomyScan::Location::eKind::CHARACTER, characterId, 0, item.inventoryType, item.lot, item.count, EconomyScan::CharacterVersion(xml) }));
					});
				});
			}
		} else {
			std::vector<std::string> needles;
			for (const auto id : itemIds) needles.push_back("id=\"" + std::to_string(id) + "\"");
			db.ForEachCharacterXml([&](LWOOBJID characterId, const std::string& xml) {
				if (std::ranges::any_of(needles, [&](const std::string& needle) { return xml.find(needle) != std::string::npos; })) visitCharacter(characterId, xml);
			});
		}
		db.ForEachMailAttachment([&](const IEconomyLedger::MailAttachment& attachment) {
			const auto it = found.find(attachment.itemId);
			if (it != found.end()) it->second.push_back(RawLocation({ EconomyScan::Location::eKind::MAIL, attachment.receiverId, attachment.mailId, 0, attachment.lot, attachment.count }));
		});
		nlohmann::json result = nlohmann::json::object();
		for (auto& [id, locations] : found) result[std::to_string(id)] = std::move(locations);
		return result;
	}

	constexpr size_t TRACE_MAX_HOPS = 200;
	constexpr size_t TRACE_MAX_LOOKUPS = 100; // ids looked up in inventories and mail, the latest first

	// Worker thread: an item's chain of transfers (ItemTrace) and where each of its ids is now
	nlohmann::json TraceRaw(GameDatabase& db, LWOOBJID itemId, bool followMerges) {
		std::map<int64_t, nlohmann::json> rowsById;
		const auto chain = ItemTrace::Build(itemId, [&](const std::vector<LWOOBJID>& ids) {
			std::vector<ItemTrace::Hop> hops;
			for (auto& row : db.GetTransfersForItems(ids)) {
				hops.push_back(ItemTrace::FromRow(row));
				rowsById.emplace(hops.back().row, std::move(row));
			}
			return hops;
		}, TRACE_MAX_HOPS, followMerges);

		nlohmann::json hops = nlohmann::json::array();
		for (const auto& traced : chain.hops) {
			auto row = rowsById.at(traced.hop.row);
			row["role"] = std::string(magic_enum::enum_name(traced.role));
			row["merge"] = traced.merge;
			row["merge_proven"] = traced.mergeProven;
			row["gap"] = traced.gap ? nlohmann::json(std::string(magic_enum::enum_name(*traced.gap))) : nlohmann::json();
			hops.push_back(std::move(row));
		}
		// Look up the latest ids first; the rest may still hold part of a split stack
		std::vector<LWOOBJID> lookups(chain.latest.begin(), chain.latest.end());
		for (const auto id : chain.ids) {
			if (lookups.size() >= TRACE_MAX_LOOKUPS) break;
			if (std::ranges::find(lookups, id) == lookups.end()) lookups.push_back(id);
		}
		nlohmann::json ids = nlohmann::json::array();
		for (const auto id : chain.ids) ids.push_back(std::to_string(id));
		nlohmann::json latest = nlohmann::json::array();
		for (const auto id : chain.latest) latest.push_back(std::to_string(id));
		return {
			{"hops", hops}, {"ids", ids}, {"latest", latest}, {"first", std::to_string(chain.first)}, {"truncated", chain.truncated},
			{"checked", lookups.size()}, {"locations", FindObjectsRaw(db, lookups)}
		};
	}

	// Worker thread: who holds a LOT, by character, with counts per inventory
	nlohmann::json FindHoldersRaw(GameDatabase& db, LOT lot) {
		std::map<LWOOBJID, std::map<std::string, uint64_t>> holders;
		db.ForEachCharacterXmlContaining("l=\"" + std::to_string(lot) + "\"", [&](LWOOBJID characterId, const std::string& xml) {
			EconomyScan::ForEachInventoryItem(xml, [&](const EconomyScan::InventoryItem& item) {
				if (item.lot == lot && CountsAsHeld(item)) holders[characterId][InventoryName(item.inventoryType)] += item.count;
			});
		});
		db.ForEachMailAttachment([&](const IEconomyLedger::MailAttachment& attachment) {
			if (attachment.lot == lot) holders[attachment.receiverId]["MAIL"] += attachment.count;
		});
		nlohmann::json rows = nlohmann::json::array();
		for (const auto& [characterId, places] : holders) rows.push_back({ {"id", characterId}, {"places", places} });
		return rows;
	}

	nlohmann::json ScanJson(const DuplicateScan& scan) {
		return {
			{"time", scan.time}, {"seconds", scan.seconds}, {"characters", scan.characters}, {"items", scan.items},
			{"duplicateCount", scan.duplicates.size()}, {"duplicates", scan.duplicates},
			{"collisionCount", scan.collisions.size()}, {"collisions", scan.collisions}, {"heldByLot", scan.heldByLot}
		};
	}

	// ?kind= is an eMapEvent value
	std::optional<IEconomyLedger::eMapEvent> MapKind(const std::string& kind) {
		const auto value = GeneralUtils::TryParse<uint8_t>(kind);
		return value ? magic_enum::enum_cast<IEconomyLedger::eMapEvent>(*value) : std::nullopt;
	}

	// How the pages show a kind of map event: the heading for its LOT column (empty when the LOT means nothing) and
	// what its quantity counts (empty when it is just the number of events)
	std::pair<std::string, std::string> MapKindColumns(IEconomyLedger::eMapEvent kind) {
		using enum IEconomyLedger::eMapEvent;
		switch (kind) {
		case ENEMY_KILLS: return { "Enemy", "" };
		case ITEM_DROPS: return { "Item", "Items" };
		case COIN_DROPS: return { "", "Coins" };
		case PLAYER_DEATHS: return { "Killed by", "" };
		case PLAYER_COIN_DROPS: return { "", "Coins" };
		case SMASHABLES_SMASHED: return { "Object", "" };
		case QUICKBUILDS_COMPLETED: return { "Quickbuild", "" };
		case POWERUP_DROPS: return { "Powerup", "" };
		case POWERUP_PICKUPS: return { "Powerup", "" };
		}
		return { "Object", "" };
	}

	// [{value, name, lots, quantity}] for every kind of map event
	nlohmann::json MapKindsJson() {
		nlohmann::json kinds = nlohmann::json::array();
		for (const auto kind : magic_enum::enum_values<IEconomyLedger::eMapEvent>()) {
			const auto [lots, quantity] = MapKindColumns(kind);
			kinds.push_back({ {"value", static_cast<int>(kind)}, {"name", GameLabels::Name(kind)}, {"lots", lots}, {"quantity", quantity} });
		}
		return kinds;
	}

	// [{value, name}] for every player statistic
	nlohmann::json StatNamesJson() {
		nlohmann::json stats = nlohmann::json::array();
		for (const auto stat : magic_enum::enum_values<StatisticID>()) stats.push_back({ {"value", static_cast<int>(stat)}, {"name", GameLabels::Name(stat)} });
		return stats;
	}

	struct ZoneInfo {
		std::string name;
		std::string luzPath; // relative to res/maps
	};

	std::optional<ZoneInfo> GetZoneInfo(uint32_t zoneId) {
		static std::map<uint32_t, std::optional<ZoneInfo>> cache;
		if (const auto it = cache.find(zoneId); it != cache.end()) return it->second;
		auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT zoneName, DisplayDescription FROM ZoneTable WHERE zoneID = ?;");
		stmt.bind(1, static_cast<int>(zoneId));
		auto result = stmt.execQuery();
		std::optional<ZoneInfo> info;
		if (!result.eof()) {
			const std::string description = result.getStringField("DisplayDescription", "");
			info = ZoneInfo{ description.empty() ? "Zone " + std::to_string(zoneId) : description, result.getStringField("zoneName", "") };
		}
		return cache[zoneId] = info;
	}

	/**
	 * A zone's terrain file read whole (Raw, dCommon): the one its .luz names, in the .luz's folder, else the .luz's
	 * name with .raw. Only the start of the .luz is read.
	 */
	std::optional<Raw::Raw> ReadZoneRaw(const ZoneInfo& zone) {
		if (!zone.luzPath.ends_with(".luz")) return std::nullopt;
		const auto folder = zone.luzPath.substr(0, zone.luzPath.find_last_of('/') + 1);
		std::string rawPath = zone.luzPath.substr(0, zone.luzPath.size() - 4) + ".raw";
		if (const auto luz = ClientAssets::ReadResFile("maps/" + zone.luzPath)) {
			std::string error;
			const auto header = ZonePaths::ReadHeader(*luz, error);
			if (header && header->zoneRawPath.ends_with(".raw")) rawPath = folder + header->zoneRawPath;
		}
		auto data = ClientAssets::ReadResFile("maps/" + rawPath);
		if (!data) return std::nullopt;
		auto raw = TerrainMap::Read(std::move(*data));
		if (!raw) LOG("Could not read the terrain of %s", rawPath.c_str());
		return raw;
	}

	// The luz file name without folder or extension, which also names the zone's minimap folder
	std::string ZoneBaseName(const ZoneInfo& zone) {
		auto name = zone.luzPath.substr(zone.luzPath.find_last_of('/') + 1);
		if (name.ends_with(".luz")) name.resize(name.size() - 4);
		return name;
	}

	struct Minimap {
		std::string folder;
		uint32_t tiles{}; // per side
	};

	/**
	 * The game's minimap for a zone: square grids of 256px tiles (image_0001.dds, row by row) covering the terrain's
	 * bounds, drawn rotated 180 degrees (x and z both decrease to the right and down). Checked in Avant Gardens and
	 * Nimbus Station against object positions. Uses the most detailed zoom level with at most 6x6 tiles (1536 pixels).
	 */
	std::optional<Minimap> FindMinimap(const ZoneInfo& zone) {
		std::optional<Minimap> best;
		for (int zoom = 0; zoom < 3; zoom++) {
			const auto folder = "maps/minimaps/" + ZoneBaseName(zone) + "/zoom_" + std::to_string(zoom);
			const auto listing = ClientAssets::ListDirectory(folder);
			if (!listing) break;
			uint32_t images = 0;
			for (const auto& entry : (*listing)["entries"]) {
				const auto name = entry.value("name", "");
				if (name.size() == 14 && (name.starts_with("image_") || name.starts_with("Image_")) && name.ends_with(".dds")) images++;
			}
			const auto side = static_cast<uint32_t>(std::lround(std::sqrt(images)));
			if (side == 0 || side * side != images || side > 6) break;
			best = Minimap{ folder, side };
		}
		return best;
	}

	// Terrain for a zone as JSON with base64 16-bit heights, built once per zone
	std::optional<std::string> TerrainJson(uint32_t zoneId) {
		static std::map<uint32_t, std::optional<std::string>> cache;
		if (const auto it = cache.find(zoneId); it != cache.end()) return it->second;

		std::optional<std::string> json;
		const auto zone = GetZoneInfo(zoneId);
		if (zone) {
			const auto raw = ReadZoneRaw(*zone);
			// Full resolution for every live zone (Avant Gardens is 704 samples a side)
			const auto grid = raw ? TerrainMap::Parse(*raw, 1024) : std::nullopt;
			if (grid) {
				const auto packed = TerrainMap::Quantize(*grid);
				std::string encoded(packed.size() * 4 / 3 + 8, '\0');
				encoded.resize(mg_base64_encode(reinterpret_cast<const unsigned char*>(packed.data()), packed.size(), encoded.data(), encoded.size()));
				nlohmann::json terrain{
					{"zone", zoneId}, {"minX", grid->minX}, {"minZ", grid->minZ}, {"step", grid->step},
					{"width", grid->width}, {"height", grid->height}, {"minY", grid->minY}, {"maxY", grid->maxY}, {"heights", encoded}
				};
				if (const auto minimap = FindMinimap(*zone)) terrain["minimapTiles"] = minimap->tiles;
				json = terrain.dump();
			}
		}
		return cache[zoneId] = json;
	}

	std::string DayText(uint32_t day) {
		const auto time = static_cast<std::time_t>(day) * 24 * 60 * 60;
		std::tm tm{};
#ifdef _WIN32
		gmtime_s(&tm, &time);
#else
		gmtime_r(&time, &tm);
#endif
		char buffer[16];
		std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
		return buffer;
	}

	// Flow rows with readable dates and source names, for CSV
	nlohmann::json ReadableFlows(nlohmann::json rows) {
		const auto sources = SourceNames();
		for (auto& row : rows) {
			row["date"] = DayText(row.value("day", 0u));
			row["source_name"] = sources.value(std::to_string(row.value("source", 0)), std::to_string(row.value("source", 0)));
		}
		return rows;
	}

	std::string RangeSuffix(uint32_t from, uint32_t to) {
		return DayText(from) + "_to_" + DayText(to) + ".csv";
	}

	void WithItemNames(nlohmann::json& rows) {
		for (auto& row : rows) {
			if (row.contains("lot") && row["lot"].get<LOT>() > 0) row["name"] = ClientAssets::ItemName(row["lot"].get<LOT>());
		}
	}
}

bool StartDuplicateScan(std::function<void(const nlohmann::json& scan, const std::string& error)> done) {
	return Background::Run("duplicate_scan", ScanDuplicatesRaw, [done = std::move(done)](nlohmann::json raw, const std::string& error) {
		if (!error.empty()) return done(nullptr, error);
		g_LastScan = FinishDuplicateScan(raw);
		BroadcastTableChanged("duplicate_scan");
		done(ScanJson(*g_LastScan), "");
	});
}

nlohmann::json EconomySourceNames() {
	return SourceNames();
}

std::optional<std::string> ZoneTerrainJson(uint32_t zoneId) {
	return TerrainJson(zoneId);
}

namespace {
	std::string Base64(std::string_view bytes) {
		std::string encoded(bytes.size() * 4 / 3 + 8, '\0');
		encoded.resize(mg_base64_encode(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), encoded.data(), encoded.size()));
		return encoded;
	}
}

std::optional<Raw::Raw> ZoneRaw(uint32_t zoneId) {
	const auto zone = GetZoneInfo(zoneId);
	return zone ? ReadZoneRaw(*zone) : std::nullopt;
}

namespace {
	std::string_view BytesOf(const std::vector<uint8_t>& bytes) {
		return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
	}
}

std::optional<std::string> ZoneTerrainChunksJson(uint32_t zoneId) {
	static std::map<uint32_t, std::optional<std::string>> cache;
	if (const auto it = cache.find(zoneId); it != cache.end()) return it->second;
	std::optional<std::string> json;
	if (const auto raw = ZoneRaw(zoneId)) {
		nlohmann::json out{ {"zone", zoneId}, {"chunks", nlohmann::json::array()} };
		std::set<uint32_t> textures;
		for (const auto& chunk : raw->chunks) {
			const std::string_view heights(reinterpret_cast<const char*>(chunk.heightMap.data()), chunk.heightMap.size() * sizeof(float));
			std::array<uint32_t, 4> ids{};
			std::copy_n(chunk.textureIds.begin(), std::min<size_t>(4, chunk.textureIds.size()), ids.begin());
			// The maps are BGRA, as the file stores them; older files' color maps aren't square to the resolution
			const bool colorFits = chunk.colorMap.size() == static_cast<size_t>(chunk.colorMapResolution) * chunk.colorMapResolution * 4;
			out["chunks"].push_back({ {"x", chunk.offsetX}, {"z", chunk.offsetZ}, {"width", chunk.width}, {"height", chunk.height}, {"scale", chunk.scaleFactor},
				{"textures", ids},
				{"heights", Base64(heights)}, // float32, little-endian; heights[width * i + j] is at x = x + i * scale, z = z + j * scale
				{"colorSize", colorFits ? chunk.colorMapResolution : 0}, {"color", colorFits ? Base64(BytesOf(chunk.colorMap)) : std::string{}},
				{"blendSize", chunk.textureMapResolution}, {"blend", Base64(BytesOf(chunk.textureMap))} });
			for (const auto texture : ids) textures.insert(texture);
		}
		out["textures"] = textures;
		json = out.dump();
	}
	return cache[zoneId] = json;
}

std::optional<std::string> ZoneTerrainLayersJson(uint32_t zoneId) {
	static std::map<uint32_t, std::optional<std::string>> cache;
	if (const auto it = cache.find(zoneId); it != cache.end()) return it->second;
	std::optional<std::string> json;
	const auto zone = GetZoneInfo(zoneId);
	const auto raw = zone ? ReadZoneRaw(*zone) : std::nullopt;
	if (raw) {
		// Scene names from the .luz: the terrain's scene map holds the ids of its general scenes (audio scenes share them)
		std::map<uint32_t, std::string> names;
		const auto luz = ClientAssets::ReadResFile("maps/" + zone->luzPath);
		std::string error;
		if (const auto header = luz ? ZonePaths::ReadHeader(*luz, error) : std::nullopt) {
			for (const auto& scene : header->scenes) if (scene.sceneType == 0) names.try_emplace(scene.id, scene.name);
		}
		std::map<uint8_t, uint64_t> cells;
		nlohmann::json chunks = nlohmann::json::array();
		for (const auto& chunk : raw->chunks) {
			const bool fits = chunk.colorMapResolution > 0 && chunk.sceneMap.size() == static_cast<size_t>(chunk.colorMapResolution) * chunk.colorMapResolution;
			if (fits) for (const auto scene : chunk.sceneMap) cells[scene]++;
			// scenes[size * i + j] is the scene of the cell at x = x + i / size * (width - 1) * scale, z likewise with j
			chunks.push_back({ {"sceneSize", fits ? chunk.colorMapResolution : 0}, {"scenes", fits ? Base64(BytesOf(chunk.sceneMap)) : std::string{}} });
		}
		uint64_t total = 0;
		for (const auto& [scene, count] : cells) total += count;
		nlohmann::json scenes = nlohmann::json::array();
		for (const auto& [scene, count] : cells) {
			// The game's own scene colors (its level editor's template colors, by scene id)
			const auto& color = SceneColor::Get(scene);
			const auto name = names.find(scene);
			scenes.push_back({ {"id", scene}, {"name", name == names.end() ? "Scene " + std::to_string(scene) : name->second},
				{"color", { std::lround(color.m_Red * 255), std::lround(color.m_Green * 255), std::lround(color.m_Blue * 255) }},
				{"share", total ? std::round(static_cast<double>(count) / static_cast<double>(total) * 10000.0) / 10000.0 : 0.0} });
		}
		size_t flairs = 0;
		for (const auto& chunk : raw->chunks) flairs += chunk.flairs.size();
		json = nlohmann::json{ {"zone", zoneId}, {"version", raw->version}, {"scenes", scenes}, {"chunks", chunks}, {"flairs", flairs} }.dump();
	}
	return cache[zoneId] = json;
}

std::optional<std::string> TerrainTextureFile(uint32_t textureId) {
	static std::map<uint32_t, std::string> paths; // id -> path under res/, found once
	if (!paths.contains(textureId)) {
		auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT texturepath FROM mapTextureResource WHERE id = ? LIMIT 1;");
		stmt.bind(1, static_cast<int>(textureId));
		auto result = stmt.execQuery();
		if (result.eof()) return std::nullopt;
		std::string file = result.getStringField(0);
		std::transform(file.begin(), file.end(), file.begin(), ::tolower);
		std::string found;
		if (ClientAssets::ReadResFile("textures/env/" + file)) found = "textures/env/" + file;
		else if (const auto listing = ClientAssets::FindResFile("textures", file)) found = *listing;
		paths[textureId] = found;
	}
	if (paths[textureId].empty()) return std::nullopt;
	// Tiled 4 times per chunk: 512 pixels is plenty and keeps the page light
	return ClientAssets::TextureAsPng(paths[textureId], 512, true);
}

std::optional<nlohmann::json> ZonePropertyAreasJson(uint32_t zoneId) {
	static std::map<uint32_t, std::optional<nlohmann::json>> cache;
	if (const auto it = cache.find(zoneId); it != cache.end()) return it->second;
	std::optional<nlohmann::json> json;
	const auto zone = GetZoneInfo(zoneId);
	const auto luz = zone ? ClientAssets::ReadResFile("maps/" + zone->luzPath) : std::nullopt;
	if (luz) {
		std::string error;
		if (const auto areas = ZonePaths::ReadPropertyAreas(*luz, error)) {
			json = nlohmann::json::array();
			for (const auto& area : *areas) {
				nlohmann::json outline = nlohmann::json::array();
				for (const auto& point : area.outline) outline.push_back({ point.x, point.y, point.z });
				json->push_back({ {"name", area.name}, {"displayName", area.displayName}, {"type", area.areaType}, {"maxBuildHeight", area.maxBuildHeight}, {"outline", outline} });
			}
		} else {
			LOG("Couldn't read the property areas of zone %u: %s", zoneId, error.c_str());
		}
	}
	return cache[zoneId] = json;
}

std::optional<nlohmann::json> ZoneSpawnPointsJson(uint32_t zoneId) {
	static std::mutex mutex;
	static std::map<uint32_t, std::optional<nlohmann::json>> cache;
	std::lock_guard lock(mutex);
	if (const auto it = cache.find(zoneId); it != cache.end()) return it->second;
	const auto zone = GetZoneInfo(zoneId);
	const auto luz = zone ? ClientAssets::ReadResFile("maps/" + zone->luzPath) : std::nullopt;
	if (!luz) return cache[zoneId] = std::nullopt;
	// Scene files are named relative to the .luz's folder
	const auto folder = zone->luzPath.substr(0, zone->luzPath.find_last_of('/') + 1);
	nlohmann::json points = nlohmann::json::array();
	std::set<std::string> seen; // the world keeps the last one of a name; the list names each once
	for (const auto& scene : ZonePaths::ReadSceneFiles(*luz)) {
		const auto lvl = ClientAssets::ReadResFile("maps/" + folder + scene);
		if (!lvl) continue;
		for (const auto& point : LevelObjects::ReadSpawnPoints(*lvl)) {
			if (!seen.insert(point.name).second) continue;
			points.push_back({ {"name", point.name}, {"lot", point.lot}, {"x", point.x}, {"y", point.y}, {"z", point.z} });
		}
	}
	std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) { return a["name"].template get<std::string>() < b["name"].template get<std::string>(); });
	return cache[zoneId] = points;
}

void RegisterReportRoutes() {
	EconomyPlaces::RegisterRoutes();

	Route(eHTTPMethod::GET, "/api/reports/meta", Perm("reports_view"), "Report metadata: today's day number, the names of economy sources, map event kinds and player statistics",
		[](HTTPReply& reply, const HTTPContext&) {
			JsonReply(reply, eHTTPStatusCode::OK, { {"today", Today()}, {"sources", SourceNames()}, {"defaultDays", DEFAULT_DAYS}, {"maxDays", MAX_DAYS},
				{"mapKinds", MapKindsJson()}, {"stats", StatNamesJson()},
				{"powerupKinds", { {"drops", static_cast<int>(IEconomyLedger::eMapEvent::POWERUP_DROPS)}, {"pickups", static_cast<int>(IEconomyLedger::eMapEvent::POWERUP_PICKUPS)} }} });
		});

	// Days in these routes count from the Unix epoch (UTC); ranges default to the last 30 days and span at most a year
	Route(eHTTPMethod::GET, "/api/reports/currency", Perm("reports_view"), "Coins gained and spent per day and source. Query: ?from=&to= (days since epoch), &staff=1 to include staff",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto [from, to] = RequestedDays(context);
			JsonReply(reply, eHTTPStatusCode::OK, { {"from", from}, {"to", to}, {"rows", Database::Get()->GetCurrencyFlows(from, to, ExcludeStaff(context))} });
		});

	Route(eHTTPMethod::GET, "/api/reports/uscore", Perm("reports_view"), "U-score gained and lost per day and source. Query: ?from=&to=&staff=1",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto [from, to] = RequestedDays(context);
			JsonReply(reply, eHTTPStatusCode::OK, { {"from", from}, {"to", to}, {"rows", Database::Get()->GetUScoreFlows(from, to, ExcludeStaff(context))} });
		});

	Route(eHTTPMethod::GET, "/api/reports/items", Perm("reports_view"), "Items created and destroyed per day and source, for all items or one. Query: ?from=&to=&lot=&staff=1",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto [from, to] = RequestedDays(context);
			const LOT lot = GeneralUtils::TryParse<LOT>(QueryValue(context.queryString, "lot")).value_or(0);
			nlohmann::json response{ {"from", from}, {"to", to}, {"lot", lot}, {"rows", Database::Get()->GetItemFlows(from, to, lot, ExcludeStaff(context))} };
			if (lot > 0) response["name"] = ClientAssets::ItemName(lot);
			JsonReply(reply, eHTTPStatusCode::OK, response);
		});

	Route(eHTTPMethod::GET, "/api/reports/top_earners", Perm("reports_view"), "Characters with the most coin income (trades and mail excluded). Query: ?from=&to=&limit=&staff=1",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto [from, to] = RequestedDays(context);
			const auto limit = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "limit")).value_or(25), 1, 200);
			JsonReply(reply, eHTTPStatusCode::OK, { {"from", from}, {"to", to}, {"rows", Database::Get()->GetTopEarners(from, to, limit, ExcludeStaff(context))} });
		});

	Route(eHTTPMethod::GET, "/api/reports/top_items", Perm("reports_view"), "Items created the most. Query: ?from=&to=&limit=&staff=1",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto [from, to] = RequestedDays(context);
			const auto limit = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "limit")).value_or(25), 1, 200);
			auto rows = Database::Get()->GetTopItems(from, to, limit, ExcludeStaff(context));
			WithItemNames(rows);
			JsonReply(reply, eHTTPStatusCode::OK, { {"from", from}, {"to", to}, {"rows", rows} });
		});

	Route(eHTTPMethod::POST, "/api/reports/transfers", Perm("reports_view"), "Trades and mail between players, newest first (DataTables). Body adds {character (name or ID), lot} filters",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto request = ParseDataTablesRequest(context.body);
			const auto body = ParseBody(context);
			if (!request || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			LWOOBJID characterId = 0;
			const auto& character = body->contains("character") ? (*body)["character"] : body->contains("character_id") ? (*body)["character_id"] : nlohmann::json("");
			const auto text = character.is_string() ? character.get<std::string>() : character.is_number_integer() ? character.dump() : "";
			if (text.find_first_not_of(" \t") != std::string::npos) {
				const auto resolved = ResolveCharacter(text);
				if (!resolved) return JsonReply(reply, eHTTPStatusCode::OK, { {"draw", request->draw}, {"recordsTotal", 0}, {"recordsFiltered", 0},
					{"data", nlohmann::json::array()}, {"error", "No character called " + text} });
				characterId = *resolved;
			}
			const LOT lot = body->value("lot", 0);
			auto response = Database::Get()->GetTransfers(request->start, std::min<uint32_t>(request->length, 500), characterId, lot);
			WithItemNames(response["data"]);
			response["draw"] = request->draw;
			JsonReply(reply, eHTTPStatusCode::OK, response);
		});

	Route(eHTTPMethod::POST, "/api/account/transfers", Perm("own_history"), "Trades and mail involving your own characters, newest first (DataTables)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto request = ParseDataTablesRequest(context.body);
			if (!request) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto characters = Database::Get()->GetAccountCharacterIds(context.accountId);
			auto response = Database::Get()->GetTransfersForCharacters(characters, request->start, std::min<uint32_t>(request->length, 100));
			WithItemNames(response["data"]);
			response["draw"] = request->draw;
			JsonReply(reply, eHTTPStatusCode::OK, response);
		});

	Route(eHTTPMethod::POST, "/api/reports/objects/:id/restore", Perm("items_restore"),
		"Mail a traced item back to a character. Body: {to (character name or ID), lot, count, note}. If the object ID is not held anywhere any more "
		"the restored item keeps it; otherwise it gets a new one (so nothing is duplicated). Runs in the background: returns {requestId}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto itemId = PathId<LWOOBJID>(context.path, 3);
			const auto body = ParseBody(context);
			if (!itemId || *itemId == 0 || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid request");
			const auto& to = (*body)["to"];
			const auto recipient = ResolveCharacter(to.is_string() ? to.get<std::string>() : to.is_number_integer() ? to.dump() : "");
			const auto info = recipient ? Database::Get()->GetCharacterInfo(*recipient) : std::nullopt;
			if (!info) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "No such character to give it to");
			// Like any item given out: only to accounts the actor may manage, their own characters only with self_items
			if (!AuthorizeAccountAction(context, info->accountId, reply, eAccountAction::ITEMS)) return;
			const LOT lot = body->value("lot", 0);
			const int64_t count = body->value("count", 1);
			if (lot <= 0 || ClientAssets::ItemName(lot).empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown item");
			if (count < 1 || count > 999) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Count must be 1 to 999");
			std::string note = body->value("note", "");
			if (note.size() > 300) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "The note is too long");

			const auto requestId = PlayerActions::Begin(context.accountId, std::chrono::minutes(10));
			const auto id = *itemId;
			const auto actor = context;
			const auto target = *info;
			Background::Run("restore:" + std::to_string(requestId), [id](GameDatabase& db) { return FindObjectRaw(db, id); },
				[=](nlohmann::json raw, const std::string& error) {
					if (!error.empty()) return PlayerActions::Finish(requestId, { false, "Checking where the item is failed: " + error });
					// Still held somewhere: send a replacement with a new id, never a second copy of the same object
					const bool stillExists = !raw.empty();
					MailInfo mail;
					mail.senderId = LWOOBJID_EMPTY;
					mail.senderUsername = "[GM] " + actor.authenticatedUser;
					mail.receiverId = target.id;
					mail.recipient = target.name;
					mail.subject = "An item returned to you";
					mail.body = note.empty() ? "A moderator sent this item back to you." : note;
					mail.timeSent = static_cast<uint64_t>(std::time(nullptr));
					mail.itemID = stillExists ? LWOOBJID_EMPTY : id;
					mail.itemSubkey = LWOOBJID_EMPTY;
					mail.itemLOT = lot;
					mail.itemCount = static_cast<int16_t>(count);
					Database::Get()->InsertNewMail(mail);
					const auto what = std::to_string(count) + "x " + ClientAssets::ItemName(lot) + " (object " + std::to_string(id) + ")";
					Audit(actor, "restore_item", "Mailed " + what + " to " + target.name + (stillExists ? " as a replacement with a new ID; the original still exists" : " with its original ID"), AuditTarget::Character(target.id));
					BroadcastTableChanged("mail", std::to_string(target.id));
					PlayerActions::Finish(requestId, { true, "Mailed " + what + " to " + target.name +
						(stillExists ? ". The original still exists, so this is a replacement with a new ID." : " with its original ID.") });
				});
			JsonSuccess(reply, { {"requestId", requestId} });
		});

	Route(eHTTPMethod::GET, "/api/reports/objects/:id", Perm("reports_view"),
		"Trace an item from any of its object IDs: every trade, mail and inventory move it went through (an item gets a new ID at each), "
		"followed back to its first recorded ID and forward to the latest, and where each ID is now. ?merges=1 also follows the history of "
		"stacks it merged into. Runs in the background: returns {requestId}; the result's data (GET /api/actions/:id) has {item_id, lot, name, "
		"first_id, latest_ids, history (each hop with role LINE/BRANCH/MERGE_IN, merge, gap), ids [{id, latest, checked, locations}], locations "
		"(under any ID, each with its lot, name, character_version and new_id_at_login), duplicated (the same LOT twice under one ID), collision (different "
		"LOTs share an ID: old data, not a dupe), resolves_on_login (every shared ID stops being shared once the characters holding it log in; each "
		"shared entry in ids has login_fix {resolves, characters, logins_needed}), truncated, gaps}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto itemId = PathId<LWOOBJID>(context.path, 3);
			if (!itemId || *itemId == 0) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid object ID");
			const bool followMerges = QueryValue(context.queryString, "merges") == "1";

			const auto requestId = PlayerActions::Begin(context.accountId, std::chrono::minutes(10));
			const auto id = *itemId;
			Background::Run("trace:" + std::to_string(requestId), [id, followMerges](GameDatabase& db) { return TraceRaw(db, id, followMerges); },
				[requestId, id, followMerges](nlohmann::json raw, const std::string& error) {
					if (!error.empty()) return PlayerActions::Finish(requestId, { false, "The search failed: " + error });
					auto history = raw["hops"];
					WithItemNames(history);
					for (auto& hop : history) {
						const auto zone = GetZoneInfo(hop.value("zone", 0u));
						hop["zone_name"] = zone ? zone->name : "";
					}
					const auto names = CharacterNames();
					std::set<std::string> latest;
					for (const auto& value : raw["latest"]) latest.insert(value.get<std::string>());
					nlohmann::json locations = nlohmann::json::array();
					nlohmann::json ids = nlohmann::json::array();
					// The traded LOT is what was traced; without a recorded hop, the first copy found
					LOT lot = history.empty() ? 0 : history.back().value("lot", 0);
					bool duplicated = false;
					bool collision = false;
					// Whether every id shared by several copies stops being shared once the characters holding them log in
					bool resolvesOnLogin = true;
					for (const auto& value : raw["ids"]) {
						const auto idText = value.get<std::string>();
						const bool checked = raw["locations"].contains(idText);
						nlohmann::json here = nlohmann::json::array();
						std::vector<EconomyScan::Location> found;
						if (checked) {
							for (const auto& entry : raw["locations"][idText]) {
								const auto location = FromRawLocation(entry);
								found.push_back(location);
								if (lot == 0) lot = location.lot;
								auto json = LocationJson(location, names);
								json["item_id"] = idText;
								here.push_back(json);
								locations.push_back(std::move(json));
							}
						}
						// Two copies of the same LOT are a dupe; different LOTs under one id are an old-data id collision
						const auto groups = EconomyScan::Classify(found);
						duplicated = duplicated || !groups.duplicates.empty();
						collision = collision || groups.collision;
						nlohmann::json entry{ {"id", idText}, {"latest", latest.contains(idText)}, {"checked", checked}, {"locations", here} };
						if (found.size() > 1) {
							entry["login_fix"] = LoginFixJson(found, names);
							resolvesOnLogin = resolvesOnLogin && entry["login_fix"]["resolves"].get<bool>();
						}
						ids.push_back(std::move(entry));
					}
					size_t gaps = 0;
					for (const auto& hop : history) gaps += hop["gap"].is_null() ? 0 : 1;
					nlohmann::json latestIds = nlohmann::json::array();
					for (const auto& value : raw["latest"]) latestIds.push_back(value);
					PlayerActions::Finish(requestId, { true, "", {
						{"item_id", std::to_string(id)}, {"lot", lot}, {"name", lot > 0 ? ClientAssets::ItemName(lot) : ""},
						{"first_id", raw["first"]}, {"latest_ids", latestIds}, {"history", history}, {"ids", ids}, {"locations", locations},
						{"duplicated", duplicated}, {"collision", collision}, {"resolves_on_login", (duplicated || collision) && resolvesOnLogin}, {"truncated", raw["truncated"]}, {"max_hops", TRACE_MAX_HOPS}, {"gaps", gaps}, {"merges_followed", followMerges}
					} });
				});
			JsonSuccess(reply, { {"requestId", requestId} });
		});

	Route(eHTTPMethod::GET, "/api/reports/holders/:lot", Perm("reports_view"),
		"Who holds an item right now, from saved characters and unclaimed mail. Runs in the background: returns {requestId}; "
		"the result's data has {lot, name, total, holderCount, holders: [{character_id, character_name, count, places}]}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto lot = PathId<LOT>(context.path, 3);
			if (!lot || *lot <= 0) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid LOT");

			const auto requestId = PlayerActions::Begin(context.accountId, std::chrono::minutes(10));
			const auto target = *lot;
			Background::Run("holders:" + std::to_string(requestId), [target](GameDatabase& db) { return FindHoldersRaw(db, target); },
				[requestId, target](nlohmann::json raw, const std::string& error) {
					if (!error.empty()) return PlayerActions::Finish(requestId, { false, "The search failed: " + error });
					const auto names = CharacterNames();
					std::vector<nlohmann::json> holders;
					uint64_t total = 0;
					for (const auto& entry : raw) {
						uint64_t count = 0;
						for (const auto& [place, amount] : entry["places"].items()) count += amount.get<uint64_t>();
						total += count;
						const auto characterId = entry["id"].get<LWOOBJID>();
						holders.push_back({ {"character_id", std::to_string(characterId)}, {"character_name", NameOf(names, characterId)}, {"count", count}, {"places", entry["places"]} });
					}
					std::ranges::sort(holders, [](const auto& a, const auto& b) { return a["count"].template get<uint64_t>() > b["count"].template get<uint64_t>(); });
					const auto holderCount = holders.size();
					if (holders.size() > 5000) holders.resize(5000);
					PlayerActions::Finish(requestId, { true, "", {
						{"lot", target}, {"name", ClientAssets::ItemName(target)}, {"total", total}, {"holderCount", holderCount}, {"holders", holders}
					} });
				});
			JsonSuccess(reply, { {"requestId", requestId} });
		});

	Route(eHTTPMethod::GET, "/api/reports/map/zones", Perm("reports_view"),
		"Zones with map events. Property zones have one instance per property, so they list each property (clone) under the zone, with clone 0 "
		"for rows recorded before properties were told apart. Query: ?kind= (a map event kind from /api/reports/meta)&from=&to=",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto kind = MapKind(QueryValue(context.queryString, "kind"));
			if (!kind) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "kind must be a map event kind (see /api/reports/meta)");
			const auto [from, to] = RequestedDays(context);
			const auto rows = Database::Get()->GetMapZones(*kind, from, to);
			std::set<uint32_t> clones;
			for (const auto& row : rows) clones.insert(row.value("clone", 0u));
			const EconomyPlaces::Owners owners(clones);

			// One entry per zone; a property zone's entry sums its properties and lists them
			std::map<uint32_t, nlohmann::json> byZone;
			nlohmann::json allProperties{ {"events", 0}, {"quantity", 0}, {"properties", 0} };
			for (const auto& row : rows) {
				const auto zoneId = row.value("zone", 0u);
				const auto events = row.value("events", int64_t{ 0 }), quantity = row.value("quantity", int64_t{ 0 });
				auto& zone = byZone[zoneId];
				if (zone.is_null()) {
					const auto info = GetZoneInfo(zoneId);
					const bool property = EconomyPlaces::IsPropertyZone(zoneId);
					zone = { {"zone", zoneId}, {"name", info ? info->name : "Zone " + std::to_string(zoneId)}, {"events", 0}, {"quantity", 0}, {"property", property} };
					if (property) zone["properties"] = nlohmann::json::array();
				}
				zone["events"] = zone["events"].get<int64_t>() + events;
				zone["quantity"] = zone["quantity"].get<int64_t>() + quantity;
				if (!zone["property"].get<bool>()) continue;
				auto property = owners.Info(zoneId, row.value("clone", 0u));
				property["events"] = events;
				property["quantity"] = quantity;
				zone["properties"].push_back(std::move(property));
				allProperties["events"] = allProperties["events"].get<int64_t>() + events;
				allProperties["quantity"] = allProperties["quantity"].get<int64_t>() + quantity;
				allProperties["properties"] = allProperties["properties"].get<int64_t>() + 1;
			}
			std::vector<nlohmann::json> zones;
			for (auto& [id, zone] : byZone) zones.push_back(std::move(zone));
			std::ranges::stable_sort(zones, [](const nlohmann::json& a, const nlohmann::json& b) { return a["events"].get<int64_t>() > b["events"].get<int64_t>(); });
			JsonReply(reply, eHTTPStatusCode::OK, { {"from", from}, {"to", to}, {"zones", zones}, {"allProperties", allProperties} });
		});

	Route(eHTTPMethod::GET, "/api/reports/map/:zone", Perm("reports_view"),
		"Map events for a zone per 4x4 unit cell, plus the top LOTs. On a property zone, &clone= picks one property (0: rows from before "
		"properties were told apart; left out: every property together), and the reply adds the build area and, for one property, its owner and placed models. "
		"Query: ?kind=&lot=&from=&to=&clone=",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto zone = PathId<uint32_t>(context.path, 3);
			const auto kind = MapKind(QueryValue(context.queryString, "kind"));
			if (!zone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone");
			if (!kind) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "kind must be a map event kind (see /api/reports/meta)");
			const auto [from, to] = RequestedDays(context);
			const LOT lot = GeneralUtils::TryParse<LOT>(QueryValue(context.queryString, "lot")).value_or(0);
			const auto cloneText = QueryValue(context.queryString, "clone");
			const auto clone = cloneText.empty() ? std::nullopt : GeneralUtils::TryParse<uint32_t>(cloneText);
			if (!cloneText.empty() && !clone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid clone");
			auto lots = Database::Get()->GetMapLots(*zone, clone, *kind, from, to, 50);
			WithItemNames(lots);
			if (EconomyPlaces::IsPowerupKind(*kind)) {
				for (auto& row : lots) row["type"] = EconomyPlaces::PowerupType(row.value("lot", 0));
			}
			const auto info = GetZoneInfo(*zone);
			nlohmann::json response{
				{"zone", *zone}, {"name", info ? info->name : ""}, {"from", from}, {"to", to}, {"lot", lot}, {"clone", clone ? nlohmann::json(*clone) : nlohmann::json()},
				{"cellSize", static_cast<int>(IEconomyLedger::MAP_CELL_SIZE)}, {"lots", lots}, {"cells", Database::Get()->GetMapCells(*zone, clone, *kind, from, to, lot)}
			};
			if (EconomyPlaces::IsPropertyZone(*zone)) {
				// Where owners may build, so events line up with the property
				nlohmann::json property{ {"areas", ZonePropertyAreasJson(*zone).value_or(nlohmann::json())} };
				if (clone) {
					const EconomyPlaces::Owners owners({ *clone });
					property["info"] = owners.Info(*zone, *clone);
					const auto propertyId = GeneralUtils::TryParse<LWOOBJID>(property["info"].value("property_id", ""));
					nlohmann::json models = nlohmann::json::array();
					if (propertyId) {
						for (const auto& model : Database::Get()->GetPropertyModels(*propertyId)) {
							models.push_back({ {"lot", model.lot}, {"name", model.lot == 14 ? "Player-built model" : ClientAssets::ItemName(model.lot)},
								{"x", model.position.x}, {"z", model.position.z} });
						}
					}
					property["models"] = models;
				}
				response["property"] = property;
			}
			JsonReply(reply, eHTTPStatusCode::OK, response);
		});

	Route(eHTTPMethod::GET, "/api/reports/activity", Perm("reports_view"),
		"What players did per day: map events of every kind and player statistics (per day and per place), and powerups by type. "
		"Query: ?from=&to=&staff=1 (staff only applies to statistics)&place= (see /api/reports/places; empty: everywhere)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto [from, to] = RequestedDays(context);
			const bool excludeStaff = ExcludeStaff(context);
			const auto placeText = QueryValue(context.queryString, "place");
			const auto place = EconomyPlaces::Parse(placeText);
			if (!place) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "place must be empty, properties, <zone>, <zone>:<clone> or *:<clone>");
			// Whether a (zone, clone) row is in the place, for the per-place breakdowns
			const auto inPlace = [&place](uint32_t zone, uint32_t clone) {
				if (!place->zones.empty() && std::ranges::find(place->zones, zone) == place->zones.end()) return false;
				return !place->clone || *place->clone == clone;
			};

			auto statRows = Database::Get()->GetPlayerStatsPerZone(from, to, excludeStaff);
			nlohmann::json mapRows = nlohmann::json::array();
			// One pass over the range for every kind (a query per kind read it once each)
			for (auto& row : Database::Get()->GetMapZonesAllKinds(from, to)) {
				if (magic_enum::enum_cast<IEconomyLedger::eMapEvent>(row.value("kind", 0))) mapRows.push_back(std::move(row));
			}
			std::set<uint32_t> clones;
			for (const auto* rows : { &statRows, &mapRows }) for (const auto& row : *rows) clones.insert(row.value("clone", 0u));
			const EconomyPlaces::Owners owners(clones);
			// Each row named for its place: the world, or the property (with its zone) on property zones
			const auto describe = [&owners](nlohmann::json& row) {
				const auto zoneId = row.value("zone", 0u);
				const auto zone = GetZoneInfo(zoneId);
				row["name"] = zone ? zone->name : "Zone " + std::to_string(zoneId);
				if (!EconomyPlaces::IsPropertyZone(zoneId)) return;
				const auto info = owners.Info(zoneId, row.value("clone", 0u));
				row["property"] = true;
				row["place"] = info["place"];
				row["property_name"] = info["name"];
				row["unknown"] = info["unknown"];
				if (info.contains("property_id")) row["property_id"] = info["property_id"];
				if (info.contains("owner_id")) { row["owner_id"] = info["owner_id"]; row["owner_name"] = info["owner_name"]; }
			};
			nlohmann::json statZones = nlohmann::json::array(), mapZones = nlohmann::json::array();
			for (auto& row : statRows) if (inPlace(row.value("zone", 0u), row.value("clone", 0u))) { describe(row); statZones.push_back(std::move(row)); }
			for (auto& row : mapRows) if (inPlace(row.value("zone", 0u), row.value("clone", 0u))) { describe(row); mapZones.push_back(std::move(row)); }

			// Powerups per day and type, as the CDClient's behaviors say what each restores
			nlohmann::json powerupDays = nlohmann::json::array();
			for (const auto kind : { IEconomyLedger::eMapEvent::POWERUP_DROPS, IEconomyLedger::eMapEvent::POWERUP_PICKUPS }) {
				std::map<std::pair<uint32_t, std::string>, int64_t> byDay;
				for (const auto& row : Database::Get()->GetMapEventsByLot(kind, from, to, *place, 50000)) {
					byDay[{ row.value("day", 0u), EconomyPlaces::PowerupType(row.value("lot", 0)) }] += row.value("events", int64_t{ 0 });
				}
				for (const auto& [key, events] : byDay) powerupDays.push_back({ {"day", key.first}, {"kind", static_cast<int>(kind)}, {"type", key.second}, {"events", events} });
			}
			JsonReply(reply, eHTTPStatusCode::OK, {
				{"from", from}, {"to", to}, {"place", placeText},
				{"mapDays", Database::Get()->GetMapEventsPerDay(from, to, *place)}, {"mapZones", mapZones},
				{"statDays", Database::Get()->GetPlayerStatsPerDay(from, to, excludeStaff, *place)}, {"statZones", statZones},
				{"powerupDays", powerupDays}
			});
		});

	Route(eHTTPMethod::GET, "/api/reports/map/:zone/terrain", Perm("reports_view"), "A zone's terrain heights for drawing maps (needs client_location)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto zone = PathId<uint32_t>(context.path, 3);
			if (!zone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone");
			const auto terrain = TerrainJson(*zone);
			if (!terrain) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No terrain for this zone (is client_location set?)");
			reply.status = eHTTPStatusCode::OK;
			reply.contentType = eContentType::APPLICATION_JSON;
			reply.message = *terrain;
			reply.headers.push_back("Cache-Control: private, max-age=86400");
		});

	Route(eHTTPMethod::GET, "/api/reports/map/:zone/minimap/:tile", Perm("reports_view"), "One tile (1-based, row by row) of a zone's in-game minimap as PNG",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto zone = PathId<uint32_t>(context.path, 3);
			const auto tile = PathId<uint32_t>(context.path, 5);
			const auto info = zone ? GetZoneInfo(*zone) : std::nullopt;
			const auto minimap = info ? FindMinimap(*info) : std::nullopt;
			if (!minimap || !tile || *tile < 1 || *tile > minimap->tiles * minimap->tiles) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such minimap tile");
			char name[32];
			std::snprintf(name, sizeof(name), "/image_%04u.dds", *tile);
			const auto png = ClientAssets::TextureAsPng(minimap->folder + name, 256);
			if (!png) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Could not convert the minimap (is ImageMagick installed?)");
			reply.status = eHTTPStatusCode::OK;
			reply.contentType = eContentType::IMAGE_PNG;
			reply.message = *png;
			reply.headers.push_back("Cache-Control: private, max-age=86400");
		});

	// ---- CSV downloads (same filters as the JSON routes) ----

	Route(eHTTPMethod::GET, "/api/reports/currency/csv", Perm("reports_view"), "Coins per day and source as CSV. Query: ?from=&to=&staff=1",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto [from, to] = RequestedDays(context);
			CsvReply(reply, "coins_" + RangeSuffix(from, to), ToCsv(ReadableFlows(Database::Get()->GetCurrencyFlows(from, to, ExcludeStaff(context))),
				{ {"date", "Date"}, {"source_name", "Source"}, {"gained", "Gained"}, {"spent", "Spent"} }));
		});

	Route(eHTTPMethod::GET, "/api/reports/uscore/csv", Perm("reports_view"), "U-score per day and source as CSV. Query: ?from=&to=&staff=1",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto [from, to] = RequestedDays(context);
			CsvReply(reply, "uscore_" + RangeSuffix(from, to), ToCsv(ReadableFlows(Database::Get()->GetUScoreFlows(from, to, ExcludeStaff(context))),
				{ {"date", "Date"}, {"source_name", "Source"}, {"gained", "Gained"}, {"lost", "Lost"} }));
		});

	Route(eHTTPMethod::GET, "/api/reports/items/csv", Perm("reports_view"), "Items created and destroyed per day and source as CSV. Query: ?from=&to=&lot=&staff=1",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto [from, to] = RequestedDays(context);
			const LOT lot = GeneralUtils::TryParse<LOT>(QueryValue(context.queryString, "lot")).value_or(0);
			CsvReply(reply, "items_" + (lot > 0 ? std::to_string(lot) + "_" : std::string()) + RangeSuffix(from, to),
				ToCsv(ReadableFlows(Database::Get()->GetItemFlows(from, to, lot, ExcludeStaff(context))),
					{ {"date", "Date"}, {"source_name", "Source"}, {"created", "Created"}, {"destroyed", "Destroyed"} }));
		});

	Route(eHTTPMethod::GET, "/api/reports/top_earners/csv", Perm("reports_view"), "Top earners as CSV. Query: ?from=&to=&limit= (up to 1000)&staff=1",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto [from, to] = RequestedDays(context);
			const auto limit = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "limit")).value_or(100), 1, 1000);
			CsvReply(reply, "top_earners_" + RangeSuffix(from, to), ToCsv(Database::Get()->GetTopEarners(from, to, limit, ExcludeStaff(context)),
				{ {"character_id", "Character ID"}, {"name", "Character"}, {"gained", "Earned"}, {"spent", "Spent"} }));
		});

	Route(eHTTPMethod::GET, "/api/reports/top_items/csv", Perm("reports_view"), "Most created items as CSV. Query: ?from=&to=&limit= (up to 1000)&staff=1",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto [from, to] = RequestedDays(context);
			const auto limit = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "limit")).value_or(100), 1, 1000);
			auto rows = Database::Get()->GetTopItems(from, to, limit, ExcludeStaff(context));
			WithItemNames(rows);
			CsvReply(reply, "top_items_" + RangeSuffix(from, to), ToCsv(rows, { {"lot", "LOT"}, {"name", "Item"}, {"created", "Created"}, {"destroyed", "Destroyed"} }));
		});

	Route(eHTTPMethod::GET, "/api/reports/transfers/csv", Perm("reports_view"), "Trades and mail as CSV, newest first (up to 50,000). Query: ?character= (name or ID)&lot=",
		[](HTTPReply& reply, const HTTPContext& context) {
			auto characterText = QueryValue(context.queryString, "character");
			if (characterText.empty()) characterText = QueryValue(context.queryString, "character_id");
			const auto characterId = characterText.empty() ? 0 : ResolveCharacter(characterText).value_or(-1);
			const auto lot = GeneralUtils::TryParse<LOT>(QueryValue(context.queryString, "lot")).value_or(0);
			auto data = Database::Get()->GetTransfers(0, 50000, characterId, lot)["data"];
			static const char* METHODS[] = { "", "Trade", "Mail sent", "Mail claimed" };
			for (auto& row : data) {
				const auto method = row.value("method", 0);
				row["how"] = method >= 1 && method <= 3 ? METHODS[method] : "";
				row["when"] = row.value("time", int64_t{ 0 });
			}
			WithItemNames(data);
			CsvReply(reply, "transfers.csv", ToCsv(data, {
				{"when", "Unix time"}, {"how", "How"}, {"lot", "LOT"}, {"name", "Item"}, {"count", "Count"}, {"coins", "Coins"},
				{"from_character", "From ID"}, {"from_name", "From"}, {"to_character", "To ID"}, {"to_name", "To"},
				{"item_id", "Object ID"}, {"new_item_id", "Received as"}, {"zone", "Zone"} }));
		});

	Route(eHTTPMethod::GET, "/api/reports/duplicates/csv", Perm("reports_view"), "The last duplicate scan's duplicated items and object ID collisions (different items sharing an ID, from old data) as CSV, one row per copy",
		[](HTTPReply& reply, const HTTPContext&) {
			nlohmann::json rows = nlohmann::json::array();
			const auto addRows = [&rows](const nlohmann::json& groups, const std::string& kind) {
				for (const auto& group : groups) {
					for (const auto& copy : group["copies"]) {
						rows.push_back({
							{"kind", kind}, {"item_id", group["item_id"]}, {"lot", copy["lot"]}, {"name", copy.value("name", "")},
							{"character_id", copy["character_id"]}, {"character_name", copy["character_name"]},
							{"where", copy.value("where", "") == "mail" ? "mail " + copy.value("mail_id", "") : copy.value("inventory", "")}, {"count", copy["count"]},
							{"version", copy.contains("character_version") ? nlohmann::json(copy["character_version"]) : nlohmann::json("")},
							{"new_id", copy.value("new_id_at_login", false) ? "yes" : "no"},
							{"resolves", group.contains("login_fix") && group["login_fix"].value("resolves", false) ? "yes" : "no"}
						});
					}
				}
			};
			if (g_LastScan) {
				addRows(g_LastScan->duplicates, "Duplicate");
				addRows(g_LastScan->collisions, "ID collision");
			}
			CsvReply(reply, "duplicates.csv", ToCsv(rows, {
				{"kind", "Kind"}, {"item_id", "Object ID"}, {"lot", "LOT"}, {"name", "Item"}, {"character_id", "Character ID"}, {"character_name", "Character"}, {"where", "Where"}, {"count", "Count"},
				{"version", "Character version"}, {"new_id", "New ID at next login"}, {"resolves", "Resolves on login"} }));
		});

	Route(eHTTPMethod::GET, "/api/reports/duplicates", Perm("reports_view"), "Result of the last duplicate item scan: duplicates (the same LOT under one object ID in more than one place) and collisions "
		"(different LOTs sharing an object ID, from old data; not dupes). Each copy has character_version (the save's <lvl cv>) and new_id_at_login; "
		"each group has login_fix {resolves, characters, logins_needed}: whether the login migration that gives old saves new item IDs clears it",
		[](HTTPReply& reply, const HTTPContext&) {
			if (!g_LastScan) return JsonReply(reply, eHTTPStatusCode::OK, { {"time", 0} });
			JsonReply(reply, eHTTPStatusCode::OK, ScanJson(*g_LastScan));
		});

	Route(eHTTPMethod::POST, "/api/reports/duplicates/scan", Perm("reports_view"),
		"Scan every character and unclaimed mail for duplicated items and totals held per item. Runs in the background: returns {requestId}",
		[](HTTPReply& reply, const HTTPContext& context) {
			// Reads every character; a minute between scans keeps it from being run in a loop
			if (g_LastScan && std::time(nullptr) - g_LastScan->time < 60) {
				return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "A scan ran less than a minute ago");
			}
			const auto requestId = PlayerActions::Begin(context.accountId, std::chrono::minutes(30));
			const auto actor = context;
			const bool started = StartDuplicateScan([requestId, actor](const nlohmann::json& scan, const std::string& error) {
				if (!error.empty()) return PlayerActions::Finish(requestId, { false, "The scan failed: " + error });
				const auto count = scan["duplicateCount"].get<size_t>();
				Audit(actor, "economy_duplicate_scan", std::to_string(count) + " duplicated item(s)");
				PlayerActions::Finish(requestId, { true, "Scan finished: " + std::to_string(count) + " duplicated item(s)", scan });
			});
			if (!started) {
				PlayerActions::Finish(requestId, { false, "A scan is already running" });
				return JsonError(reply, eHTTPStatusCode::CONFLICT, "A scan is already running");
			}
			JsonSuccess(reply, { {"requestId", requestId}, {"message", "Scan started"} });
		});
}
