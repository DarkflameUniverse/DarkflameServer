#include "WorldView.h"
#include "PositionHistory.h"
#include "WorldScene.h"
#include "Scenery.h"
#include "ZonePaths.h"
#include "ReportRoutes.h"
#include "EconomyPlaces.h"
#include "ClientAssets.h"
#include "DashboardRoutes.h"
#include "GameLabels.h"
#include "Permissions.h"
#include "ServerState.h"
#include "Background.h"
#include "OnceCache.h"
#include "Workers.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <map>
#include <set>

#include "RouteUtils.h"
#include "CDClientDatabase.h"
#include "master/DashboardMessages.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"
#include "eHTTPMethod.h"
#include "magic_enum.hpp"
#include "ZCompression.h"

using namespace RouteUtils;

namespace {
	constexpr int64_t DAY_SECONDS = 24 * 60 * 60;
	constexpr int64_t IDLE_INTERVAL = 30;          // seconds between samples of a player standing still
	constexpr int64_t FLUSH_INTERVAL = 10;         // seconds between batched writes
	constexpr size_t MAX_BUFFERED = 50000;         // samples held while the database is busy; older ones are dropped
	constexpr int64_t MAX_REPLAY_SPAN = 7 * DAY_SECONDS;
	constexpr int64_t MAX_SAMPLES_PER_PLAYER = 3000;
	constexpr uint32_t MAX_REPLAY_ROWS = 400000;
	constexpr uint32_t MAX_HEATMAP_DAYS = 180;
	constexpr uint32_t MAX_HEATMAP_ROWS = 300000;

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	int64_t Setting(const std::string& key, int64_t fallback) {
		if (!Game::config) return fallback;
		return GeneralUtils::TryParse<int64_t>(Game::config->GetValue(key)).value_or(fallback);
	}

	// ---- Recording ----

	PositionHistory::Throttle g_Throttle;
	std::vector<IPlayerPositions::PositionSample> g_Buffer;
	int64_t g_LastFlush = 0;
	int64_t g_LastForget = 0;

	void Flush(int64_t now) {
		if (g_Buffer.empty()) return;
		if (Background::IsRunning("position_history")) {
			if (g_Buffer.size() > MAX_BUFFERED) g_Buffer.erase(g_Buffer.begin(), g_Buffer.begin() + static_cast<std::ptrdiff_t>(g_Buffer.size() - MAX_BUFFERED));
			return;
		}
		g_LastFlush = now;
		auto batch = std::move(g_Buffer);
		g_Buffer.clear();
		Background::Run("position_history", [batch = std::move(batch)](GameDatabase& db) -> nlohmann::json {
			db.InsertPositionSamples(batch);
			return batch.size();
		}, [](nlohmann::json, const std::string& error) {
			if (!error.empty()) LOG("Could not save player positions: %s", error.c_str());
		});
	}

	// ---- Zone data ----

	std::string ZoneName(uint32_t zone) {
		const auto& names = ZoneNames();
		const auto key = std::to_string(zone);
		return names.contains(key) ? names[key].get<std::string>() : "Zone " + key;
	}

	std::optional<std::string> LuzPath(uint32_t zone) {
		return ZoneLuzPath(zone);
	}

	// LOT -> index into WorldScene::KINDS for every LOT with one of those components, read at startup (Preload): the
	// zone data is built on worker threads, which never query the CDClient
	std::map<uint32_t, uint8_t> ReadLotKinds() {
		std::map<uint32_t, uint8_t> kinds;
		std::map<uint32_t, std::vector<uint32_t>> components;
		try {
			auto result = CDClientDatabase::ExecuteQuery("SELECT id, component_type FROM ComponentsRegistry;");
			for (; !result.eof(); result.nextRow()) components[static_cast<uint32_t>(result.getIntField(0, 0))].push_back(static_cast<uint32_t>(result.getIntField(1, 0)));
		} catch (const std::exception& ex) {
			LOG("Could not read the components of objects: %s", ex.what());
		}
		for (const auto& [lot, types] : components) {
			const auto kind = WorldScene::Classify(types);
			if (kind < WorldScene::KINDS.size()) kinds[lot] = static_cast<uint8_t>(kind);
		}
		return kinds;
	}

	const std::map<uint32_t, uint8_t>& LotKinds() {
		static const auto kinds = ReadLotKinds();
		return kinds;
	}

	double Round(float value) { return std::round(static_cast<double>(value) * 100.0) / 100.0; }

	/**
	 * Everything the 3D view draws of a zone except the terrain, scenery and paths, built once per zone from the start of
	 * its .luz (the scene list) and its scene files. Objects are columns (lot, kind, flags, scene, x/y/z) to keep big zones
	 * small: flags 1 = spawner, 2 = client only, 4 = drawn with a model by the scenery layer.
	 */
	std::optional<std::string> BuildSceneJson(uint32_t zone) {
		const auto luzPath = LuzPath(zone);
		const auto luz = luzPath ? ClientAssets::ReadResFile("maps/" + *luzPath) : std::nullopt;
		std::string error;
		const auto zoneFile = luz ? ZonePaths::ReadHeader(*luz, error) : std::nullopt;
		if (!zoneFile) return std::nullopt;

		const auto& lotKinds = LotKinds();
		const auto other = static_cast<uint8_t>(WorldScene::KINDS.size());
		nlohmann::json kinds = nlohmann::json::array();
		for (size_t i = 0; i < WorldScene::KINDS.size(); i++) kinds.push_back({ {"value", i}, {"name", GameLabels::Name(WorldScene::KINDS[i])} });
		kinds.push_back({ {"value", other}, {"name", "Other"} });

		nlohmann::json lots = nlohmann::json::array(), kindOf = nlohmann::json::array(), flags = nlohmann::json::array(), sceneOf = nlohmann::json::array(), positions = nlohmann::json::array();
		nlohmann::json names = nlohmann::json::object(), lotNames = nlohmann::json::object(), scenes = nlohmann::json::object();
		std::set<uint32_t> seenLots;
		const auto folder = luzPath->substr(0, luzPath->find_last_of('/') + 1);
		size_t index = 0;
		for (const auto& scene : zoneFile->scenes) {
			// Audio scenes share their general scene's id; the general one names it
			if (scene.sceneType == eSceneType::General || !scenes.contains(std::to_string(scene.id))) scenes[std::to_string(scene.id)] = scene.name;
			const auto lvl = ClientAssets::ReadResFile("maps/" + folder + scene.filename);
			if (!lvl) continue;
			for (const auto& object : WorldScene::ReadObjects(*lvl)) {
				const auto lot = object.templateLot ? object.templateLot : object.lot;
				const auto kind = lotKinds.find(lot);
				lots.push_back(lot);
				kindOf.push_back(kind == lotKinds.end() ? other : kind->second);
				flags.push_back((object.spawner ? 1 : 0) | (object.clientOnly ? 2 : 0) | (Scenery::HasModel(object) ? 4 : 0));
				sceneOf.push_back(scene.id);
				positions.push_back(Round(object.x));
				positions.push_back(Round(object.y));
				positions.push_back(Round(object.z));
				if (!object.name.empty()) names[std::to_string(index)] = object.name;
				if (seenLots.insert(lot).second) lotNames[std::to_string(lot)] = ClientAssets::ItemName(static_cast<LOT>(lot));
				index++;
			}
		}

		nlohmann::json json{
			{"zone", zone}, {"name", ZoneName(zone)},
			{"spawn", { Round(zoneFile->spawnpoint.x), Round(zoneFile->spawnpoint.y), Round(zoneFile->spawnpoint.z) }},
			{"scenes", scenes}, {"kinds", kinds},
			{"objects", { {"lot", lots}, {"kind", kindOf}, {"flags", flags}, {"scene", sceneOf}, {"pos", positions}, {"names", names} }}, {"lotNames", lotNames},
			{"spawnPoints", ZoneSpawnPointsJson(zone).value_or(nlohmann::json::array())}
		};
		return json.dump();
	}

	OnceCache<uint32_t, std::optional<std::string>> g_Scenes;
	OnceCache<uint32_t, std::optional<std::string>> g_Paths;

	// Built on a worker thread the first time (any thread)
	const std::optional<std::string>& SceneJson(uint32_t zone) {
		return g_Scenes.Get(zone, [zone] { return BuildSceneJson(zone); });
	}

	// A zone's paths, which need the whole .luz (they are most of it): loaded when a path layer is first shown
	std::optional<std::string> BuildPathsJson(uint32_t zone) {
		const auto luzPath = LuzPath(zone);
		const auto luz = luzPath ? ClientAssets::ReadResFile("maps/" + *luzPath) : std::nullopt;
		std::string error;
		const auto zoneFile = luz ? ZonePaths::Read(*luz, error) : std::nullopt;
		if (!zoneFile) return std::nullopt;

		nlohmann::json pathTypes = nlohmann::json::array();
		for (const auto type : magic_enum::enum_values<PathType>()) pathTypes.push_back({ {"value", static_cast<int>(type)}, {"name", GameLabels::Name(type)} });
		nlohmann::json paths = nlohmann::json::array();
		for (const auto& path : zoneFile->paths) {
			nlohmann::json points = nlohmann::json::array();
			for (const auto& waypoint : path.pathWaypoints) {
				points.push_back(Round(waypoint.position.x));
				points.push_back(Round(waypoint.position.y));
				points.push_back(Round(waypoint.position.z));
			}
			if (points.empty()) continue;
			paths.push_back({ {"name", path.pathName}, {"type", static_cast<int>(path.pathType)}, {"loop", path.pathBehavior == PathBehavior::Loop}, {"points", points} });
		}
		return nlohmann::json{ {"zone", zone}, {"pathTypes", pathTypes}, {"paths", paths} }.dump();
	}

	const std::optional<std::string>& PathsJson(uint32_t zone) {
		return g_Paths.Get(zone, [zone] { return BuildPathsJson(zone); });
	}

	std::string CharacterName(LWOOBJID id) {
		static std::map<LWOOBJID, std::string> names;
		if (const auto it = names.find(id); it != names.end()) return it->second;
		const auto info = Database::Get()->GetCharacterInfo(id);
		return names[id] = info ? info->name : std::to_string(id);
	}

	void RawJson(HTTPReply& reply, const std::string& json, const char* cache) {
		reply.status = eHTTPStatusCode::OK;
		reply.contentType = eContentType::APPLICATION_JSON;
		reply.message = json;
		reply.headers.push_back(std::string("Cache-Control: ") + cache);
	}

	using Deflated = OnceCache<uint32_t, std::optional<std::string>>;

	bool TakesDeflate(const HTTPContext& context) {
		return context.GetHeader("Accept-Encoding").find("deflate") != std::string::npos;
	}

	/**
	 * A big cached JSON body, deflated once per zone when the browser takes it: a zone's terrain chunks are mostly
	 * base64 maps that shrink about tenfold (Avant Gardens: 19 MB to 1.4 MB). Any thread.
	 */
	void DeflatedJson(HTTPReply& reply, bool deflate, Deflated& deflated, uint32_t zone, const std::string& json, const char* cache) {
		if (!deflate || json.size() > UINT32_MAX / 2) return RawJson(reply, json, cache);
		const auto& out = deflated.Get(zone, [&json]() -> std::optional<std::string> {
			std::string out(ZCompression::GetMaxCompressedLength(static_cast<uint32_t>(json.size())), '\0');
			const auto size = ZCompression::Compress(reinterpret_cast<const uint8_t*>(json.data()), static_cast<uint32_t>(json.size()),
				reinterpret_cast<uint8_t*>(out.data()), static_cast<uint32_t>(out.size()));
			if (size <= 0) return std::nullopt;
			out.resize(static_cast<size_t>(size));
			return out;
		});
		if (!out) return RawJson(reply, json, cache);
		RawJson(reply, *out, cache);
		reply.headers.push_back("Content-Encoding: deflate");
		reply.headers.push_back("Vary: Accept-Encoding");
	}

	int64_t QueryInt(const HTTPContext& context, const char* name, int64_t fallback) {
		return GeneralUtils::TryParse<int64_t>(QueryValue(context.queryString, name)).value_or(fallback);
	}
}

namespace WorldView {
	void Preload() {
		LotKinds();
	}

	void RecordPositions(const PlayerPositions& positions) {
		if (Setting("position_history", 1) == 0) {
			g_Buffer.clear();
			return;
		}
		const auto now = Now();
		const auto interval = std::clamp<int64_t>(Setting("position_history_seconds", 5), 1, 600);
		for (const auto& player : positions.players) {
			if (!std::isfinite(player.x) || !std::isfinite(player.y) || !std::isfinite(player.z)) continue;
			if (!g_Throttle.Keep(player.characterId, positions.zoneId, positions.instanceId, player.x, player.y, player.z, now, interval, IDLE_INTERVAL)) continue;
			g_Buffer.push_back({ now, player.characterId, positions.zoneId, positions.instanceId, positions.cloneId, player.x, player.y, player.z });
		}
		if (now - g_LastForget > 300) {
			g_LastForget = now;
			g_Throttle.Forget(now - 300);
		}
		if (now - g_LastFlush >= FLUSH_INTERVAL) Flush(now);
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/world3d", Perm("players_view"), "Live 3D world view: a zone's terrain and objects with players moving on it, replays and heat map timelapses",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "world3d.jinja2", "world3d"); });

		Route(eHTTPMethod::GET, "/api/world3d/meta", Perm("players_view"),
			"For the 3D world view: every zone's name, the running worlds, map event kinds, and how position history is kept",
			[](HTTPReply& reply, const HTTPContext&) {
				nlohmann::json worlds = nlohmann::json::array();
				{
					std::lock_guard lock(ServerState::g_StatusMutex);
					for (const auto& w : ServerState::g_WorldInstances) {
						worlds.push_back({ {"zone", w.mapID}, {"instance", w.instanceID}, {"clone", w.cloneID}, {"players", w.players} });
					}
				}
				nlohmann::json mapKinds = nlohmann::json::array();
				for (const auto kind : magic_enum::enum_values<IEconomyLedger::eMapEvent>()) mapKinds.push_back({ {"value", static_cast<int>(kind)}, {"name", GameLabels::Name(kind)} });
				JsonReply(reply, eHTTPStatusCode::OK, {
					{"zones", ZoneNames()}, {"worlds", worlds}, {"mapKinds", mapKinds}, {"cellSize", IEconomyLedger::MAP_CELL_SIZE},
					{"propertyZones", EconomyPlaces::PropertyZones()},
					{"today", Now() / DAY_SECONDS}, {"now", Now()},
					{"history", { {"enabled", Setting("position_history", 1) != 0}, {"days", Setting("position_history_days", 3)},
						{"seconds", std::clamp<int64_t>(Setting("position_history_seconds", 5), 1, 600)}, {"idleSeconds", IDLE_INTERVAL} }}
				});
			});

		Route(eHTTPMethod::GET, "/api/world3d/:zone/scene", Perm("players_view"),
			"A zone's scenes, scene objects (LOT, kind, scene, position) and spawn points from its client files, for the 3D world view (needs client_location)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = PathId<uint32_t>(context.path, 2);
				if (!zone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone");
				Workers::Reply(reply, context, g_Scenes.Ready(*zone), [zone = *zone](HTTPReply& out) {
					const auto& json = SceneJson(zone);
					if (!json) return JsonError(out, eHTTPStatusCode::NOT_FOUND, "No zone file for this zone (is client_location set?)");
					RawJson(out, *json, "private, max-age=3600");
				});
			});

		Route(eHTTPMethod::GET, "/api/world3d/:zone/paths", Perm("players_view"),
			"A zone's paths (name, type, waypoints) from its .luz, for the 3D world view's path layers (needs client_location)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = PathId<uint32_t>(context.path, 2);
				if (!zone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone");
				Workers::Reply(reply, context, g_Paths.Ready(*zone), [zone = *zone](HTTPReply& out) {
					const auto& json = PathsJson(zone);
					if (!json) return JsonError(out, eHTTPStatusCode::NOT_FOUND, "No zone file for this zone (is client_location set?)");
					RawJson(out, *json, "private, max-age=3600");
				});
			});

		Route(eHTTPMethod::GET, "/api/world3d/:zone/terrain_layers", Perm("players_view"),
			"A zone's terrain scene map (the scene of each terrain cell, per chunk of /terrain_chunks) and its scenes with names and colors, for the 3D world view (needs client_location)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = PathId<uint32_t>(context.path, 2);
				if (!zone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone");
				static Deflated deflated;
				const auto deflate = TakesDeflate(context);
				Workers::Reply(reply, context, ZoneTerrainLayersReady(*zone) && (!deflate || deflated.Ready(*zone)), [zone = *zone, deflate](HTTPReply& out) {
					const auto layers = ZoneTerrainLayersJson(zone);
					if (!layers) return JsonError(out, eHTTPStatusCode::NOT_FOUND, "No terrain for this zone");
					DeflatedJson(out, deflate, deflated, zone, *layers, "private, max-age=86400");
				});
			});

		Route(eHTTPMethod::GET, "/api/world3d/:zone/flairs", Perm("players_view"),
			"A zone's flairs (the grass, flowers and small rocks its terrain file strews over it) in the scenery manifest's form, for the 3D world view (needs client_location)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = PathId<uint32_t>(context.path, 2);
				if (!zone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone");
				Workers::Reply(reply, context, Scenery::FlairsReady(*zone), [zone = *zone](HTTPReply& out) {
					const auto flairs = Scenery::FlairsJson(zone);
					if (!flairs) return JsonError(out, eHTTPStatusCode::NOT_FOUND, "No terrain for this zone");
					RawJson(out, *flairs, "private, max-age=3600");
				});
			});

		Route(eHTTPMethod::GET, "/api/world3d/:zone/terrain_chunks", Perm("players_view"),
			"A zone's terrain as the game draws it, for the 3D world view (needs client_location). Textures: /api/terrain_textures/:id",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = PathId<uint32_t>(context.path, 2);
				if (!zone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone");
				static Deflated deflated;
				const auto deflate = TakesDeflate(context);
				Workers::Reply(reply, context, ZoneTerrainChunksReady(*zone) && (!deflate || deflated.Ready(*zone)), [zone = *zone, deflate](HTTPReply& out) {
					const auto terrain = ZoneTerrainChunksJson(zone);
					if (!terrain) return JsonError(out, eHTTPStatusCode::NOT_FOUND, "No terrain for this zone");
					DeflatedJson(out, deflate, deflated, zone, *terrain, "private, max-age=86400");
				});
			});

		Route(eHTTPMethod::GET, "/api/world3d/:zone/scenery", Perm("players_view"),
			"A zone's scenery as the game draws it: every scene object's model and the sky, for the 3D world view (needs client_location). Models: /api/scenery/:zone/mesh/:asset",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = PathId<uint32_t>(context.path, 2);
				if (!zone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone");
				Workers::Reply(reply, context, Scenery::ZoneReady(*zone), [zone = *zone](HTTPReply& out) {
					const auto scenery = Scenery::ZoneJson(zone);
					if (!scenery) return JsonError(out, eHTTPStatusCode::NOT_FOUND, "No zone file for this zone (is client_location set?)");
					RawJson(out, *scenery, "private, max-age=3600");
				});
			});

		Route(eHTTPMethod::GET, "/api/world3d/history/instances", Perm("players_history"),
			"World instances with recorded player positions. Query: ?zone= (0 or none: every zone)&from=&to= (Unix seconds; default the whole history)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = static_cast<uint32_t>(std::max<int64_t>(0, QueryInt(context, "zone", 0)));
				const auto to = QueryInt(context, "to", Now());
				const auto from = QueryInt(context, "from", 0);
				nlohmann::json instances = nlohmann::json::array();
				for (const auto& i : Database::Get()->GetPositionInstances(zone, from, to)) {
					instances.push_back({ {"zone", i.zoneId}, {"zoneName", ZoneName(i.zoneId)}, {"instance", i.instanceId}, {"clone", i.cloneId},
						{"first", i.first}, {"last", i.last}, {"players", i.players} });
				}
				JsonReply(reply, eHTTPStatusCode::OK, { {"instances", instances} });
			});

		Route(eHTTPMethod::GET, "/api/world3d/history", Perm("players_history"),
			"Where players went in a zone between two times, for replays: per character its samples as [seconds after from, x, y, z, ...]. "
			"Query: ?zone=&instance= (0: all)&from=&to= (Unix seconds, at most 7 days apart). Long ranges are thinned to about 3000 samples per player. Audited",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = static_cast<uint32_t>(std::max<int64_t>(0, QueryInt(context, "zone", 0)));
				const auto instance = static_cast<uint32_t>(std::max<int64_t>(0, QueryInt(context, "instance", 0)));
				const auto to = QueryInt(context, "to", Now());
				const auto from = QueryInt(context, "from", to - 3600);
				if (!zone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick a zone");
				if (from >= to || to - from > MAX_REPLAY_SPAN) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick a range of at most 7 days");
				const auto interval = std::clamp<int64_t>(Setting("position_history_seconds", 5), 1, 600);
				const auto bucket = PositionHistory::BucketFor(to - from, interval, MAX_SAMPLES_PER_PLAYER);
				const auto samples = Database::Get()->GetPositionSamples(zone, instance, from, to, bucket, MAX_REPLAY_ROWS);

				nlohmann::json players = nlohmann::json::array();
				for (size_t i = 0; i < samples.size();) {
					const auto& first = samples[i];
					nlohmann::json points = nlohmann::json::array();
					std::set<uint32_t> instances;
					for (; i < samples.size() && samples[i].characterId == first.characterId; i++) {
						const auto& s = samples[i];
						points.push_back(s.time - from);
						points.push_back(Round(s.x));
						points.push_back(Round(s.y));
						points.push_back(Round(s.z));
						instances.insert(s.instanceId);
					}
					players.push_back({ {"id", std::to_string(first.characterId)}, {"name", CharacterName(first.characterId)}, {"instances", instances}, {"samples", points} });
				}
				Audit(context, "view_position_history", ZoneName(zone) + (instance ? " instance " + std::to_string(instance) : "") + ", " +
					std::to_string((to - from + 59) / 60) + " minute(s) ending " + std::to_string(to));
				JsonReply(reply, eHTTPStatusCode::OK, {
					{"zone", zone}, {"instance", instance}, {"from", from}, {"to", to}, {"bucket", bucket}, {"interval", interval}, {"idleSeconds", IDLE_INTERVAL},
					{"truncated", samples.size() >= MAX_REPLAY_ROWS}, {"players", players}
				});
			});

		Route(eHTTPMethod::GET, "/api/world3d/:zone/heatmap", Perm("reports_view"),
			"Map events of one kind per day and 4x4 cell in a zone, for the heat map timelapse. Query: ?kind= (a map event kind)&from=&to= (days since epoch, at most 180 apart)"
			"&clone= (on a property zone, one property; 0: rows from before properties were told apart; left out: every property together)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = PathId<uint32_t>(context.path, 2);
				if (!zone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone");
				const auto kind = magic_enum::enum_cast<IEconomyLedger::eMapEvent>(static_cast<uint8_t>(QueryInt(context, "kind", 0)));
				if (!kind || QueryInt(context, "kind", 0) > 255) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "kind must be a map event kind");
				const auto today = static_cast<uint32_t>(Now() / DAY_SECONDS);
				const auto to = static_cast<uint32_t>(std::clamp<int64_t>(QueryInt(context, "to", today), 0, today));
				const auto from = static_cast<uint32_t>(std::clamp<int64_t>(QueryInt(context, "from", to > 29 ? to - 29 : 0), 0, to));
				if (to - from >= MAX_HEATMAP_DAYS) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick at most 180 days");
				const auto cloneText = QueryValue(context.queryString, "clone");
				const auto clone = cloneText.empty() ? std::nullopt : GeneralUtils::TryParse<uint32_t>(cloneText);
				if (!cloneText.empty() && !clone) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid clone");
				const auto cells = Database::Get()->GetMapCellsPerDay(*zone, clone, static_cast<uint8_t>(*kind), from, to, MAX_HEATMAP_ROWS);
				JsonReply(reply, eHTTPStatusCode::OK, {
					{"zone", *zone}, {"clone", clone ? nlohmann::json(*clone) : nlohmann::json()}, {"kind", static_cast<int>(*kind)}, {"from", from}, {"to", to}, {"cellSize", IEconomyLedger::MAP_CELL_SIZE},
					{"truncated", cells.size() >= MAX_HEATMAP_ROWS}, {"cells", cells}
				});
			});
	}
}
