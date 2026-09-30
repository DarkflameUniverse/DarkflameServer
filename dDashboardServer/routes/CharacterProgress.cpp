#include "CharacterProgress.h"
#include "GameText.h"

#include <chrono>
#include <ctime>
#include <map>
#include <set>

#include "RouteUtils.h"
#include "MissionTools.h"
#include "MissionXml.h"
#include "Background.h"
#include "DashboardRoutes.h"
#include "CDClientDatabase.h"
#include "Database.h"
#include "tinyxml2.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	// How long the server's averages are kept before they are worked out again (when someone next looks)
	constexpr int64_t AVERAGE_SECONDS = 6 * 60 * 60;

	struct Category {
		std::string type;
		std::string subtype;
		uint32_t missions{};     // released missions in it
		uint32_t achievements{}; // released achievements in it
	};

	struct Zone {
		std::optional<size_t> category; // the group its summary missions are in
		std::vector<uint32_t> summary;   // missions its summary tracks (collectibles, pets, ...), from ZoneSummary
		bool released{};
	};

	struct Catalog {
		std::vector<Category> categories;
		std::map<uint32_t, size_t> categoryOf; // released mission -> category
		std::map<uint32_t, Zone> zones;
	};

	const Catalog& GetCatalog() {
		static const auto catalog = [] {
			Catalog c;
			std::map<std::pair<std::string, std::string>, size_t> index;
			for (const auto& [id, info] : MissionCatalog::All()) {
				if (!info.released) continue;
				auto [it, added] = index.try_emplace({ info.type, info.subtype }, c.categories.size());
				if (added) c.categories.push_back({ info.type, info.subtype });
				(info.definition.isMission ? c.categories[it->second].missions : c.categories[it->second].achievements)++;
				c.categoryOf[id] = it->second;
			}
			auto rows = CDClientDatabase::ExecuteQuery("SELECT zoneID, value FROM ZoneSummary ORDER BY zoneID, type;");
			while (!rows.eof()) {
				const auto mission = static_cast<uint32_t>(rows.getIntField("value", 0));
				auto& zone = c.zones[static_cast<uint32_t>(rows.getIntField("zoneID"))];
				if (MissionCatalog::Find(mission)) zone.summary.push_back(mission);
				rows.nextRow();
			}
			auto zones = CDClientDatabase::ExecuteQuery("SELECT zoneID, locStatus FROM ZoneTable;");
			while (!zones.eof()) {
				const auto it = c.zones.find(static_cast<uint32_t>(zones.getIntField("zoneID")));
				if (it != c.zones.end()) it->second.released = zones.getIntField("locStatus", 0) == 2;
				zones.nextRow();
			}
			// A zone's group is the one most of its summary missions are in
			for (auto& [zoneId, zone] : c.zones) {
				std::map<size_t, uint32_t> votes;
				for (const auto mission : zone.summary) {
					const auto it = c.categoryOf.find(mission);
					if (it != c.categoryOf.end()) votes[it->second]++;
				}
				const auto best = std::ranges::max_element(votes, {}, [](const auto& vote) { return vote.second; });
				if (best != votes.end()) zone.category = best->first;
			}
			return c;
		}();
		return catalog;
	}

	// Every character's count of done missions and achievements (sorted), and per category the totals
	struct Averages {
		int64_t updated{};
		std::vector<uint32_t> missions;
		std::vector<uint32_t> achievements;
		std::vector<uint64_t> missionsIn; // per category
		std::vector<uint64_t> achievementsIn;
	};
	std::optional<Averages> g_Averages;

	constexpr auto AVERAGES_TASK = "progress_averages";

	// Start working out the averages if they are missing or old; answers whether they are being worked out
	bool RefreshAverages() {
		if (g_Averages && std::time(nullptr) - g_Averages->updated < AVERAGE_SECONDS) return false;
		if (Background::IsRunning(AVERAGES_TASK)) return true;
		// The worker may not use game data, so it gets what it needs to know about each mission now
		const auto& catalog = GetCatalog();
		std::map<uint32_t, std::pair<size_t, bool>> released; // mission -> (category, is a mission)
		for (const auto& [id, category] : catalog.categoryOf) released[id] = { category, MissionCatalog::Find(id)->definition.isMission };
		const auto categories = catalog.categories.size();
		Background::Run(AVERAGES_TASK, [released, categories](GameDatabase& db) -> nlohmann::json {
			std::vector<uint32_t> missions, achievements;
			std::vector<uint64_t> missionsIn(categories), achievementsIn(categories);
			const auto noDefinitions = [](uint32_t) -> const MissionXml::Definition* { return nullptr; };
			db.ForEachCharacterXml([&](LWOOBJID, const std::string& xml) {
				uint32_t doneMissions = 0, doneAchievements = 0;
				for (const auto& [id, entry] : MissionXml::Read(xml, noDefinitions)) {
					const auto it = released.find(id);
					if (it == released.end() || !entry.Done()) continue;
					if (it->second.second) { doneMissions++; missionsIn[it->second.first]++; }
					else { doneAchievements++; achievementsIn[it->second.first]++; }
				}
				// Characters that never finished anything (made and never played) would only drag the average down
				if (doneMissions + doneAchievements == 0) return;
				missions.push_back(doneMissions);
				achievements.push_back(doneAchievements);
			});
			std::ranges::sort(missions);
			std::ranges::sort(achievements);
			return { {"missions", missions}, {"achievements", achievements}, {"missions_in", missionsIn}, {"achievements_in", achievementsIn} };
		}, [](nlohmann::json result, const std::string& error) {
			if (!error.empty()) return;
			g_Averages = Averages{ static_cast<int64_t>(std::time(nullptr)), result["missions"].get<std::vector<uint32_t>>(), result["achievements"].get<std::vector<uint32_t>>(),
				result["missions_in"].get<std::vector<uint64_t>>(), result["achievements_in"].get<std::vector<uint64_t>>() };
		});
		return true;
	}

	nlohmann::json Count(uint32_t done, uint32_t available) {
		return { {"done", done}, {"available", available} };
	}

	nlohmann::json StandingJson(const std::vector<uint32_t>& sorted, uint32_t value) {
		const auto standing = CharacterProgress::Compare(sorted, value);
		return { {"average", standing.average}, {"percentile", standing.percentile} };
	}

	// The zone's statistics the game keeps in <char><zs><s map ac (achievements) bc (bricks) cc (coins) es (enemies) qbc (quickbuilds)/>
	// and the zones visited in <char><vl><l id cid/>
	void ReadZones(const std::string& xml, std::map<uint32_t, nlohmann::json>& stats, std::set<uint32_t>& visited) {
		tinyxml2::XMLDocument doc;
		if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS || !doc.FirstChildElement("obj")) return;
		const auto* character = doc.FirstChildElement("obj")->FirstChildElement("char");
		if (!character) return;
		const auto* vl = character->FirstChildElement("vl");
		for (const auto* l = vl ? vl->FirstChildElement("l") : nullptr; l; l = l->NextSiblingElement("l")) visited.insert(l->UnsignedAttribute("id"));
		const auto* zs = character->FirstChildElement("zs");
		for (const auto* s = zs ? zs->FirstChildElement("s") : nullptr; s; s = s->NextSiblingElement("s")) {
			stats[s->UnsignedAttribute("map")] = { {"achievements", s->Unsigned64Attribute("ac")}, {"bricks", s->Int64Attribute("bc")},
				{"coins", s->Unsigned64Attribute("cc")}, {"enemies", s->Unsigned64Attribute("es")}, {"quickbuilds", s->Unsigned64Attribute("qbc")} };
		}
	}

	nlohmann::json Progress(const std::string& xml) {
		const auto& catalog = GetCatalog();
		const auto entries = MissionXml::Read(xml, MissionCatalog::Definition);
		std::vector<uint32_t> missionsIn(catalog.categories.size()), achievementsIn(catalog.categories.size());
		uint32_t missions = 0, achievements = 0, missionsAvailable = 0, achievementsAvailable = 0;
		for (const auto& category : catalog.categories) { missionsAvailable += category.missions; achievementsAvailable += category.achievements; }
		for (const auto& [id, entry] : entries) {
			const auto it = catalog.categoryOf.find(id);
			if (it == catalog.categoryOf.end() || !entry.Done()) continue;
			if (MissionCatalog::Find(id)->definition.isMission) { missions++; missionsIn[it->second]++; }
			else { achievements++; achievementsIn[it->second]++; }
		}

		const bool haveAverages = g_Averages && !g_Averages->missions.empty();
		const auto perCharacter = [&](const std::vector<uint64_t>& totals, size_t i) {
			return haveAverages && i < totals.size() ? static_cast<double>(totals[i]) / static_cast<double>(g_Averages->missions.size()) : 0.0;
		};
		nlohmann::json categories = nlohmann::json::array();
		for (size_t i = 0; i < catalog.categories.size(); i++) {
			const auto& category = catalog.categories[i];
			nlohmann::json row{ {"index", i}, {"type", category.type}, {"subtype", category.subtype},
				{"missions", Count(missionsIn[i], category.missions)}, {"achievements", Count(achievementsIn[i], category.achievements)} };
			if (haveAverages) row["average"] = { {"missions", perCharacter(g_Averages->missionsIn, i)}, {"achievements", perCharacter(g_Averages->achievementsIn, i)} };
			categories.push_back(std::move(row));
		}

		std::map<uint32_t, nlohmann::json> stats;
		std::set<uint32_t> visited;
		ReadZones(xml, stats, visited);
		std::set<uint32_t> zoneIds(visited);
		for (const auto& [zoneId, s] : stats) zoneIds.insert(zoneId);
		for (const auto& [zoneId, zone] : catalog.zones) if (zone.released) zoneIds.insert(zoneId);
		zoneIds.erase(0); // character select
		nlohmann::json zones = nlohmann::json::array();
		for (const auto zoneId : zoneIds) {
			const auto zone = catalog.zones.find(zoneId);
			nlohmann::json summary = nlohmann::json::array();
			if (zone != catalog.zones.end()) {
				for (const auto missionId : zone->second.summary) {
					const auto* info = MissionCatalog::Find(missionId);
					const auto entry = entries.find(missionId);
					const bool done = entry != entries.end() && entry->second.Done();
					uint32_t total = 0, found = 0;
					for (size_t i = 0; i < info->tasks.size(); i++) {
						total += info->tasks[i].targetValue;
						if (!done && entry != entries.end() && i < entry->second.tasks.size()) found += std::min(entry->second.tasks[i].progress, info->tasks[i].targetValue);
					}
					summary.push_back({ {"id", missionId}, {"name", MissionCatalog::Name(missionId)}, {"done", done}, {"found", done ? total : found}, {"total", total} });
				}
			}
			const auto category = zone != catalog.zones.end() ? zone->second.category : std::nullopt;
			zones.push_back({ {"id", zoneId}, {"name", GameText::ZoneName(zoneId)}, {"visited", visited.contains(zoneId)},
				{"category", category ? nlohmann::json(*category) : nlohmann::json(nullptr)}, {"stats", stats.contains(zoneId) ? stats[zoneId] : nlohmann::json(nullptr)},
				{"summary", summary} });
		}

		nlohmann::json progress{
			{"missions", Count(missions, missionsAvailable)}, {"achievements", Count(achievements, achievementsAvailable)},
			{"categories", categories}, {"zones", zones}, {"average", nullptr}
		};
		if (haveAverages) {
			progress["average"] = { {"characters", g_Averages->missions.size()}, {"updated", g_Averages->updated},
				{"missions", StandingJson(g_Averages->missions, missions)}, {"achievements", StandingJson(g_Averages->achievements, achievements)} };
		}
		return progress;
	}
}

void RegisterCharacterProgressRoutes() {
	Route(eHTTPMethod::GET, "/api/characters/:id/progress", 0,
		"A character's completion: missions and achievements done out of those in the game, per group and per zone (with the zone's statistics "
		"and collectibles), and the server's averages ({average} is null and {average_pending} true while they are worked out)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto charId = PathId<LWOOBJID>(context.path, 2);
			const auto info = charId ? Database::Get()->GetCharacterInfo(*charId) : std::nullopt;
			if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
			if (!CanViewCharacter(context, info->accountId)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may only view your own characters");
			const auto xml = Database::Get()->GetCharacterXml(*charId);
			if (xml.empty()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "This character has no saved data yet");
			const bool pending = RefreshAverages();
			JsonSuccess(reply, { {"progress", Progress(xml)}, {"average_pending", pending && !g_Averages} });
		});
}
