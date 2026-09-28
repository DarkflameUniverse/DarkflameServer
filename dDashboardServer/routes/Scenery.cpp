#include "Scenery.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "ClientAssets.h"
#include "NifFile.h"
#include "ReportRoutes.h"
#include "RouteUtils.h"
#include "OnceCache.h"
#include "TtlCache.h"
#include "Web.h"
#include "WorkerPool.h"
#include "Workers.h"
#include "WorldScene.h"
#include "ZonePaths.h"
#include "ZoneScenes.h"

#include "CDClientDatabase.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "dConfig.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	constexpr size_t MESH_CACHE_BYTES = 64 * 1024 * 1024;
	constexpr uint32_t MAX_LOD = 3;
	constexpr uint32_t RENDER_COMPONENT = 2;

	std::string Lower(std::string text) {
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	/**
	 * `relative` (backslashes or slashes, may climb with "..") inside res/ folder `folder`, lowercase with slashes.
	 * Empty when it climbs out of res/.
	 */
	std::string JoinPath(const std::string& folder, const std::string& relative) {
		std::vector<std::string> parts;
		auto text = Lower(folder.empty() ? relative : folder + "/" + relative);
		std::replace(text.begin(), text.end(), '\\', '/');
		for (const auto& part : GeneralUtils::SplitString(text, '/')) {
			if (part.empty() || part == ".") continue;
			if (part == "..") {
				if (parts.empty()) return {};
				parts.pop_back();
			} else {
				parts.push_back(part);
			}
		}
		std::string out;
		for (const auto& part : parts) out += (out.empty() ? "" : "/") + part;
		if (out.starts_with("res/")) out = out.substr(4);
		return out;
	}

	std::string FolderOf(const std::string& path) {
		const auto slash = path.find_last_of('/');
		return slash == std::string::npos ? std::string{} : path.substr(0, slash);
	}

	// Every file under res/mesh, res/textures and res/animations (lowercase, relative to res/), and by file name
	struct FileIndex {
		std::unordered_set<std::string> paths;
		std::unordered_map<std::string, std::vector<std::string>> byName;
	};

	FileIndex IndexFiles() {
		FileIndex index;
		const auto res = ClientAssets::ResFolder();
		if (res.empty()) return index;
		std::error_code ec;
		for (const char* folder : { "mesh", "textures", "animations" }) {
			// Unpacked clients keep their original case, so the top folder is matched ignoring case too
			for (const auto& top : std::filesystem::directory_iterator(res, ec)) {
				if (Lower(top.path().filename().string()) != folder || !top.is_directory(ec)) continue;
				for (auto it = std::filesystem::recursive_directory_iterator(top.path(), ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
					if (!it->is_regular_file(ec)) continue;
					const auto relative = Lower(std::filesystem::relative(it->path(), res, ec).generic_string());
					index.paths.insert(relative);
					index.byName[Lower(it->path().filename().string())].push_back(relative);
				}
			}
		}
		return index;
	}

	// Built at startup (Scenery::Preload); read only afterwards
	const FileIndex& Files() {
		static const FileIndex index = IndexFiles();
		return index;
	}

	// A texture a model names: next to the model if it's there (the usual case), else the file of that name sharing
	// the longest folder prefix with the model. Only DDS files, which the browser decodes.
	std::string FindTexture(const std::string& modelFolder, const std::string& stored) {
		const auto path = JoinPath(modelFolder, stored);
		if (path.empty() || !path.ends_with(".dds")) return {};
		const auto& files = Files();
		if (files.paths.contains(path)) return path;
		const auto name = path.substr(path.find_last_of('/') + 1);
		const auto it = files.byName.find(name);
		if (it == files.byName.end()) return {};
		std::string best;
		size_t bestShared = 0;
		for (const auto& candidate : it->second) {
			const auto shared = static_cast<size_t>(std::mismatch(candidate.begin(), candidate.end(), modelFolder.begin(), modelFolder.end()).first - candidate.begin());
			if (best.empty() || shared > bestShared) {
				best = candidate;
				bestShared = shared;
			}
		}
		return best;
	}

	// A render asset as stored (a .nif, or a .kfm naming one) as a res path of a .nif that exists; empty otherwise
	std::string ResolveModel(const std::string& stored) {
		auto path = JoinPath("", stored);
		if (path.ends_with(".kfm") && Files().paths.contains(path)) {
			const auto kfm = ClientAssets::ReadResFile(path);
			const auto named = kfm ? NifFile::KfmModelPath(*kfm) : std::nullopt;
			path = named ? JoinPath(FolderOf(path), *named) : std::string{};
		}
		return path.ends_with(".nif") && Files().paths.contains(path) ? path : std::string{};
	}

	struct RenderInfo {
		std::string asset; // RenderComponent.render_asset as stored
		std::string type;  // Objects.type
		int32_t shader{ -1 }; // mapShaders.gameValue of RenderComponent.shader_id (-1 fixed function or unknown)
	};

	// mapShaders: id (what RenderComponent.shader_id and multishader tags name) -> gameValue (the shader drawn)
	std::map<int32_t, int32_t> g_ShaderValues;

	// Every LOT with a render component, read once (Objects has no index on id, so it is read whole too)
	std::unordered_map<uint32_t, RenderInfo> ReadRenderInfos() {
		std::unordered_map<uint32_t, RenderInfo> infos;
		try {
			auto row = CDClientDatabase::ExecuteQuery(
				"SELECT cr.id, rc.render_asset, m.gameValue FROM ComponentsRegistry cr JOIN RenderComponent rc ON rc.id = cr.component_id "
				"LEFT JOIN mapShaders m ON m.id = rc.shader_id WHERE cr.component_type = " + std::to_string(RENDER_COMPONENT) + ";");
			for (; !row.eof(); row.nextRow()) {
				infos.try_emplace(static_cast<uint32_t>(row.getIntField(0)), RenderInfo{ row.getStringField(1, ""), "", row.fieldIsNull(2) ? -1 : row.getIntField(2) });
			}
			auto shaders = CDClientDatabase::ExecuteQuery("SELECT id, gameValue FROM mapShaders;");
			for (; !shaders.eof(); shaders.nextRow()) g_ShaderValues.try_emplace(shaders.getIntField(0), shaders.getIntField(1));
			auto types = CDClientDatabase::ExecuteQuery("SELECT id, type FROM Objects;");
			for (; !types.eof(); types.nextRow()) {
				const auto it = infos.find(static_cast<uint32_t>(types.getIntField(0)));
				if (it != infos.end() && it->second.type.empty()) it->second.type = types.getStringField(1, "");
			}
		} catch (const std::exception& ex) {
			LOG("Could not read the render components of objects: %s", ex.what());
		}
		return infos;
	}

	const std::unordered_map<uint32_t, RenderInfo>& RenderInfos() {
		static const auto infos = ReadRenderInfos();
		return infos;
	}

	struct Model {
		std::string path;  // res path of the .nif, empty for none
		bool hidden{};     // the client doesn't draw it (WorldScene::ClientDraws)
		int32_t shader{ -1 }; // RenderInfo::shader
	};

	/**
	 * The .nif of a scene object (its render component's, or nif_name), and whether the client draws it
	 * (WorldScene::ClientDraws). Primitive models (built from parts at run time) aren't drawn here.
	 */
	Model ModelFor(const WorldScene::Object& object) {
		const auto lot = object.spawner ? object.templateLot : object.lot;
		const auto& infos = RenderInfos();
		const auto info = infos.find(lot);
		if (info == infos.end()) return {};
		const auto draw = WorldScene::ClientDraws(object, info->second.type);
		if (draw == WorldScene::eClientDraw::NO_MODEL) return {};
		static std::mutex mutex;
		static std::unordered_map<std::string, std::string> resolved;
		const auto& stored = object.nifName.empty() ? info->second.asset : object.nifName;
		{
			std::lock_guard lock(mutex);
			if (const auto it = resolved.find(stored); it != resolved.end()) return { it->second, draw == WorldScene::eClientDraw::HIDDEN, info->second.shader };
		}
		auto path = ResolveModel(stored);
		std::lock_guard lock(mutex);
		resolved.try_emplace(stored, path);
		return { std::move(path), draw == WorldScene::eClientDraw::HIDDEN, info->second.shader };
	}

	/**
	 * How the client draws each model, for the viewers' game shaders (static/js/game-shaders.js): "shaders" gives each
	 * asset's shader (the first object drawing it wins; -1 fixed function or not an object), "shaderTags" a multishader
	 * part's tag -> shader, and "techniques" every shader's technique (NifFile::TechniquesJson, the one table of them).
	 */
	void AddShaders(nlohmann::json& manifest, const std::vector<int32_t>& assetShaders) {
		RenderInfos(); // reads g_ShaderValues
		manifest["shaders"] = assetShaders;
		nlohmann::json tags = nlohmann::json::object();
		std::set<int32_t> values{ -1, NifFile::LEGO_SHADER };
		for (const auto& [id, value] : g_ShaderValues) {
			tags[std::to_string(id)] = value;
			values.insert(value);
		}
		manifest["shaderTags"] = std::move(tags);
		manifest["techniques"] = nlohmann::json::parse(NifFile::TechniquesJson({ values.begin(), values.end() }));
		manifest["multishader"] = NifFile::MULTISHADER;
		manifest["defaultShader"] = NifFile::LEGO_SHADER;
	}

	std::optional<std::string> LuzPath(uint32_t zone) {
		return ZoneLuzPath(zone);
	}

	double Round(float value, double scale) { return std::round(static_cast<double>(value) * scale) / scale; }

	/**
	 * Bump when NifFile's output changes: converted models kept on disk are made again, and the manifests' "format"
	 * goes into the viewers' model and texture URLs so browsers don't keep drawing the old ones (they're cached for
	 * a week). 2: meshes carry their multishader tag; conversions without it drew glom parts with the LEGO shader.
	 * 3: dark textures and the UV set each texture names. 4: the game's shaders draw the models (manifest
	 * "techniques"), vertex colors go to them as stored.
	 */
	constexpr uint32_t FORMAT_VERSION = 4;

	// A zone's lighting (WorldScene::Lighting) for the viewers' shaders
	nlohmann::json LightingJson(const WorldScene::Lighting& lighting) {
		const auto triple = [](const std::array<float, 3>& value) { return nlohmann::json{ Round(value[0], 1000.0), Round(value[1], 1000.0), Round(value[2], 1000.0) }; };
		return {
			{"ambient", triple(lighting.ambient)}, {"light", triple(lighting.light)}, {"lightVec", triple(lighting.lightVec)}, {"specular", triple(lighting.specular)},
			{"upperHemi", triple(lighting.upperHemi)}, {"fogColor", triple(lighting.fogColor)},
			{"fogNear", Round(lighting.fogNear, 10.0)}, {"fogFar", Round(lighting.fogFar, 10.0)}
		};
	}

	/**
	 * A zone's models and scenery manifest, built once (g_Zones). Once shared, assets, index, flairModels and the
	 * warmed flags are guarded by g_ZoneMutex: the flairs' models join assets when their manifest is built.
	 */
	struct ZoneScenery {
		std::vector<std::string> assets;      // res paths of models, indexed by the manifests
		std::map<std::string, size_t> index;  // res path -> its index in assets
		std::string json;
		nlohmann::json lighting;              // LightingJson of the zone's lighting, null when its scenes have none
		std::unordered_set<std::string> flairModels; // res paths of the flairs' models, converted ahead of others
		bool warmedScenery{};                 // WarmUp queued the scenery's models
		bool warmedFlairs{};                  // and the flairs'

		size_t IndexOf(const std::string& path) {
			const auto [it, added] = index.try_emplace(path, assets.size());
			if (added) assets.push_back(path);
			return it->second;
		}
	};

	std::mutex g_ZoneMutex;

	/**
	 * The zone's scenes for the viewers' "scenes like the game": each general scene's id, name, the scenes its
	 * transitions connect it to (ZoneScenes::SceneGraph) and its lighting (null when its file has none).
	 */
	nlohmann::json ScenesJson(const ZoneFile& zone, const std::map<uint32_t, nlohmann::json>& lightingOf) {
		const ZoneScenes::SceneGraph graph(zone.scenes, zone.sceneTransitions);
		nlohmann::json scenes = nlohmann::json::array();
		std::set<uint32_t> seen;
		for (const auto& scene : zone.scenes) {
			if (!seen.insert(scene.id).second) continue; // the audio layers share their scene's id
			const auto& neighbours = graph.Neighbours(scene.id);
			const auto lighting = lightingOf.find(scene.id);
			scenes.push_back({ {"id", scene.id}, {"name", scene.name}, {"neighbours", std::vector<uint32_t>(neighbours.begin(), neighbours.end())},
				{"lighting", lighting == lightingOf.end() ? nlohmann::json() : lighting->second} });
		}
		return scenes;
	}

	/**
	 * The terrain's scene map for the viewers to find the scene at a point as the client does (ZoneScenes::SceneMap;
	 * scenery-core.js sceneAt reads it the same way): per chunk its corner, far corner, cells per side and its cells
	 * (x major) as ZoneScenes::RunLengths, base64. Null without a terrain file.
	 */
	nlohmann::json SceneMapJson(uint32_t zoneId) {
		const auto raw = ZoneRawShared(zoneId);
		if (!raw) return nullptr;
		nlohmann::json chunks = nlohmann::json::array();
		for (const auto& chunk : raw->chunks) {
			if (!chunk.IsValidForSceneLookup() || chunk.sceneMap.size() < static_cast<size_t>(chunk.colorMapResolution) * chunk.colorMapResolution) continue;
			chunks.push_back({ {"x", chunk.offsetX}, {"z", chunk.offsetZ},
				{"maxX", chunk.offsetX + static_cast<float>(chunk.width - 1) * chunk.scaleFactor}, {"maxZ", chunk.offsetZ + static_cast<float>(chunk.height - 1) * chunk.scaleFactor},
				{"size", chunk.colorMapResolution}, {"runs", ZoneDataBase64(ZoneScenes::RunLengths(chunk.sceneMap, static_cast<size_t>(chunk.colorMapResolution) * chunk.colorMapResolution))} });
		}
		return chunks.empty() ? nlohmann::json() : nlohmann::json{ {"chunks", chunks} };
	}

	std::shared_ptr<ZoneScenery> BuildZone(uint32_t zoneId) {
		const auto luzPath = LuzPath(zoneId);
		const auto luz = luzPath ? ClientAssets::ReadResFile("maps/" + *luzPath) : std::nullopt;
		if (!luz) return nullptr;
		const auto folder = luzPath->substr(0, luzPath->find_last_of('/') + 1);

		std::string error;
		const auto zoneFile = ZonePaths::Read(*luz, error);
		if (!zoneFile) return nullptr;

		ZoneScenery scenery;
		nlohmann::json assetOf = nlohmann::json::array(), positions = nlohmann::json::array(), rotations = nlohmann::json::array(), scales = nlohmann::json::array();
		nlohmann::json hidden = nlohmann::json::array(), sceneOf = nlohmann::json::array();
		std::vector<int32_t> assetShaders;
		int64_t sky = -1;
		std::vector<std::pair<WorldScene::Lighting, size_t>> sceneLighting; // each scene's, with how many objects it has
		std::map<uint32_t, nlohmann::json> lightingOf;                      // scene id -> its general layer's lighting
		for (const auto& scene : zoneFile->scenes) {
			const auto lvl = ClientAssets::ReadResFile("maps/" + folder + scene.filename);
			if (!lvl) continue;
			const auto objectsBefore = assetOf.size();
			const auto lighting = WorldScene::ReadLighting(*lvl);
			if (lighting && scene.sceneType == 0) lightingOf.try_emplace(scene.id, LightingJson(*lighting));
			if (sky < 0) {
				const auto skydome = JoinPath("", WorldScene::ReadSkydome(*lvl));
				if (skydome.ends_with(".nif") && Files().paths.contains(skydome)) sky = static_cast<int64_t>(scenery.IndexOf(skydome));
			}
			for (const auto& object : WorldScene::ReadObjects(*lvl)) {
				// A spawner is drawn as what it spawns, where the client would show it
				const auto model = ModelFor(object);
				if (model.path.empty()) continue;
				const auto asset = scenery.IndexOf(model.path);
				assetOf.push_back(asset);
				if (asset >= assetShaders.size()) assetShaders.resize(asset + 1, -1);
				if (assetShaders[asset] == -1) assetShaders[asset] = model.shader;
				hidden.push_back(model.hidden ? 1 : 0);
				for (const auto value : { object.x, object.y, object.z }) positions.push_back(Round(value, 100.0));
				for (const auto value : { object.qx, object.qy, object.qz, object.qw }) rotations.push_back(Round(value, 10000.0));
				scales.push_back(Round(object.scale, 1000.0));
				sceneOf.push_back(scene.id);
			}
			if (lighting) sceneLighting.emplace_back(*lighting, assetOf.size() - objectsBefore);
		}
		if (const auto lighting = WorldScene::ZoneLighting(sceneLighting)) scenery.lighting = LightingJson(*lighting);
		nlohmann::json manifest{
			{"zone", zoneId}, {"sky", sky}, {"assets", scenery.assets}, {"lighting", scenery.lighting}, {"format", FORMAT_VERSION},
			{"objects", { {"asset", assetOf}, {"pos", positions}, {"rot", rotations}, {"scale", scales}, {"hidden", hidden}, {"scene", sceneOf} }},
			{"scenes", ScenesJson(*zoneFile, lightingOf)}, {"sceneMap", SceneMapJson(zoneId)}
		};
		assetShaders.resize(scenery.assets.size(), -1);
		AddShaders(manifest, assetShaders);
		scenery.json = manifest.dump();
		return std::make_shared<ZoneScenery>(std::move(scenery));
	}

	OnceCache<uint32_t, std::shared_ptr<ZoneScenery>> g_Zones;

	// The zone's scenery, built when first asked for (any thread); nullptr without client files
	std::shared_ptr<ZoneScenery> Zone(uint32_t zoneId) {
		return g_Zones.Get(zoneId, [zoneId] { return BuildZone(zoneId); });
	}

	// The same, only when it is built already (never waits)
	std::shared_ptr<ZoneScenery> ZoneIfBuilt(uint32_t zoneId) {
		return g_Zones.Ready(zoneId) ? Zone(zoneId) : nullptr;
	}

	// FlairTable: flair id -> the model's res path (empty when the client lacks it), read once
	std::unordered_map<uint32_t, std::string> ReadFlairModels() {
		std::unordered_map<uint32_t, std::string> models;
		try {
			auto row = CDClientDatabase::ExecuteQuery("SELECT id, asset FROM FlairTable;");
			for (; !row.eof(); row.nextRow()) models.try_emplace(static_cast<uint32_t>(row.getIntField(0)), ResolveModel(row.getStringField(1, "")));
		} catch (const std::exception& ex) {
			LOG("Could not read the flairs: %s", ex.what());
		}
		return models;
	}

	const std::unordered_map<uint32_t, std::string>& FlairModels() {
		static const auto models = ReadFlairModels();
		return models;
	}

	/**
	 * How far from the camera flairs are drawn: the client's Flair.fx draws them fully to sqrt(60000) units and fades
	 * them out over the next 15000 of distance squared.
	 */
	constexpr double FLAIR_DISTANCE = 274.0;

	/**
	 * The flairs' manifest (as the scenery's, plus a tint per flair), from the zone's terrain file. A flair's color
	 * tints its model; 63 is full strength (the files use 0 to 63 for most flairs, a little more for brighter ones).
	 */
	std::optional<std::string> BuildFlairs(uint32_t zoneId, ZoneScenery& scenery) {
		const auto raw = ZoneRawShared(zoneId);
		const auto& models = FlairModels();
		std::vector<const std::string*> modelOf;
		nlohmann::json positions = nlohmann::json::array(), rotations = nlohmann::json::array(), scales = nlohmann::json::array(), colors = nlohmann::json::array();
		for (const auto& chunk : raw ? raw->chunks : std::vector<Raw::Chunk>{}) {
			for (const auto& flair : chunk.flairs) {
				const auto model = models.find(flair.id);
				if (model == models.end() || model->second.empty() || !std::isfinite(flair.position.x)) continue;
				modelOf.push_back(&model->second);
				for (const auto value : { flair.position.x, flair.position.y, flair.position.z }) positions.push_back(Round(value, 100.0));
				// Radians about x, y and z, applied in that order
				const double c1 = std::cos(flair.rotation.x / 2), c2 = std::cos(flair.rotation.y / 2), c3 = std::cos(flair.rotation.z / 2);
				const double s1 = std::sin(flair.rotation.x / 2), s2 = std::sin(flair.rotation.y / 2), s3 = std::sin(flair.rotation.z / 2);
				for (const auto value : { s1 * c2 * c3 + c1 * s2 * s3, c1 * s2 * c3 - s1 * c2 * s3, c1 * c2 * s3 + s1 * s2 * c3, c1 * c2 * c3 - s1 * s2 * s3 }) {
					rotations.push_back(Round(static_cast<float>(value), 10000.0));
				}
				scales.push_back(Round(flair.scaleFactor, 1000.0));
				for (const auto value : { flair.colorR, flair.colorG, flair.colorB }) colors.push_back(value);
			}
		}
		// The flairs' models join the zone's list, which mesh requests read meanwhile
		nlohmann::json assetOf = nlohmann::json::array();
		std::vector<std::string> assets;
		{
			std::lock_guard lock(g_ZoneMutex);
			for (const auto* model : modelOf) {
				assetOf.push_back(scenery.IndexOf(*model));
				scenery.flairModels.insert(*model);
			}
			assets = scenery.assets;
		}
		return nlohmann::json{
			{"zone", zoneId}, {"sky", -1}, {"assets", assets}, {"distance", FLAIR_DISTANCE}, {"colorScale", 1.0 / 63.0}, {"lighting", scenery.lighting}, {"format", FORMAT_VERSION},
			// Flair.fx for all of them: (0.85 * sun + ambient) * the flair's tint, whatever their facing
			{"technique", { {"family", "flair"}, {"look", 0}, {"alpha", "opacity"}, {"flags", 0} }},
			{"objects", { {"asset", assetOf}, {"pos", positions}, {"rot", rotations}, {"scale", scales}, {"color", colors} }}
		}.dump();
	}

	OnceCache<uint32_t, std::optional<std::string>> g_Flairs;

	/**
	 * The flairs' manifest (as the scenery's, plus a tint per flair), from the zone's terrain file. A flair's color
	 * tints its model; 63 is full strength (the files use 0 to 63 for most flairs, a little more for brighter ones).
	 * Built when first asked for (any thread).
	 */
	const std::optional<std::string>& Flairs(uint32_t zoneId, ZoneScenery& scenery) {
		return g_Flairs.Get(zoneId, [zoneId, &scenery] { return BuildFlairs(zoneId, scenery); });
	}

	constexpr uintmax_t DISK_CACHE_BYTES = 512ull * 1024 * 1024;
	const std::filesystem::path CACHE_DIR = std::filesystem::path("dDashboardServer") / "scenery_cache";

	uint64_t Fnv1a(const std::string& text) {
		uint64_t hash = 14695981039346656037ull;
		for (const auto c : text) hash = (hash ^ static_cast<uint8_t>(c)) * 1099511628211ull;
		return hash;
	}

	std::optional<std::string> ReadWhole(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file) return std::nullopt;
		const auto size = file.tellg();
		if (size <= 0) return std::nullopt;
		std::string data(static_cast<size_t>(size), '\0');
		file.seekg(0);
		if (!file.read(data.data(), size)) return std::nullopt;
		return data;
	}

	/**
	 * The converted models on disk (CACHE_DIR), kept under DISK_CACHE_BYTES by removing the least recently written
	 * files. Any thread.
	 */
	class DiskCache {
	public:
		void Store(const std::filesystem::path& target, const std::string& data) {
			std::lock_guard lock(m_Mutex);
			std::error_code ec;
			std::filesystem::create_directories(CACHE_DIR, ec);
			CountLocked();
			if (m_Total + data.size() > DISK_CACHE_BYTES) {
				std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> files;
				for (const auto& entry : std::filesystem::directory_iterator(CACHE_DIR, ec)) {
					if (entry.path().extension() == ".bin") files.emplace_back(entry.last_write_time(ec), entry.path());
				}
				std::sort(files.begin(), files.end());
				for (const auto& [time, path] : files) {
					if (m_Total + data.size() <= DISK_CACHE_BYTES * 3 / 4) break;
					const auto size = std::filesystem::file_size(path, ec);
					if (std::filesystem::remove(path, ec)) m_Total -= std::min(m_Total, size);
				}
			}
			// Written next to the target and renamed, so a reader never sees half a file
			std::ostringstream temporary;
			temporary << target.string() << "." << std::this_thread::get_id() << ".tmp";
			{
				std::ofstream file(temporary.str(), std::ios::binary | std::ios::trunc);
				if (!file.write(data.data(), static_cast<std::streamsize>(data.size()))) return;
			}
			const auto existed = std::filesystem::exists(target, ec);
			std::filesystem::rename(temporary.str(), target, ec);
			if (!ec && !existed) m_Total += data.size();
		}

		// Bytes in the cache
		uintmax_t Total() {
			std::lock_guard lock(m_Mutex);
			CountLocked();
			return m_Total;
		}

	private:
		void CountLocked() {
			if (m_Counted) return;
			m_Counted = true;
			std::error_code ec;
			for (const auto& entry : std::filesystem::directory_iterator(CACHE_DIR, ec)) {
				if (entry.path().extension() == ".tmp") std::filesystem::remove(entry.path(), ec); // left by a crash
				else m_Total += entry.is_regular_file(ec) ? entry.file_size(ec) : 0;
			}
		}

		std::mutex m_Mutex;
		uintmax_t m_Total{};
		bool m_Counted{};
	};

	DiskCache g_Disk;

	using Bytes = std::shared_ptr<const std::string>;

	// A TtlCache any thread may use
	class SharedCache {
	public:
		SharedCache(std::chrono::seconds ttl, size_t maxBytes) : m_Cache(ttl, maxBytes) {}
		Bytes Get(const std::string& key) {
			std::lock_guard lock(m_Mutex);
			const auto cached = m_Cache.Get(key);
			return cached ? *cached : nullptr;
		}
		void Put(const std::string& key, Bytes value) {
			if (!value) return;
			std::lock_guard lock(m_Mutex);
			const auto weight = value->size() + 256;
			m_Cache.Put(key, std::move(value), weight);
		}
	private:
		std::mutex m_Mutex;
		TtlCache<std::string, Bytes> m_Cache;
	};

	SharedCache g_Models(std::chrono::hours(1), MESH_CACHE_BYTES);    // "path|lod" -> NifFile::Encode's output
	SharedCache g_Embedded(std::chrono::hours(1), MESH_CACHE_BYTES);  // "path#block" -> DDS of a texture stored in a .nif

	// Conversions under way ("path|lod"), so a model asked for twice at once is converted once and both get it
	std::mutex g_ConvertingMutex;
	std::map<std::string, std::shared_future<Bytes>> g_Converting;

	std::string ModelKey(const std::string& path, uint32_t lod) { return path + "|" + std::to_string(lod); }

	// Where model `path` at `lod` is kept on disk, named after the source file's size and time so a changed client
	// file is converted again
	std::filesystem::path DiskPath(const std::string& key, const std::filesystem::path& file) {
		std::error_code ec;
		const auto size = std::filesystem::file_size(file, ec);
		const auto time = std::filesystem::last_write_time(file, ec).time_since_epoch().count();
		const auto diskKey = key + "|" + std::to_string(size) + "|" + std::to_string(time) + "|" + std::to_string(FORMAT_VERSION);
		return CACHE_DIR / (std::to_string(Fnv1a(diskKey)) + ".bin");
	}

	Bytes Convert(const std::string& path, uint32_t lod, const std::filesystem::path& file, const std::filesystem::path& target) {
		if (auto encoded = ReadWhole(target)) return std::make_shared<const std::string>(std::move(*encoded));
		const auto data = ReadWhole(file);
		if (!data) return nullptr;
		std::string error;
		const auto model = NifFile::Parse(*data, lod, error);
		if (!model) {
			LOG_DEBUG("Could not read %s: %s", path.c_str(), error.c_str());
			return nullptr;
		}
		const auto folder = FolderOf(path);
		// Per mesh: a res path, "#<block>" for one stored in the .nif, or empty; for its base and its dark texture
		const auto where = [&folder](int32_t embedded, const std::string& file) {
			if (embedded >= 0) return "#" + std::to_string(embedded);
			return file.empty() ? std::string{} : FindTexture(folder, file);
		};
		std::vector<std::string> textures, darkTextures;
		for (const auto& mesh : model->meshes) {
			textures.push_back(where(mesh.material.embeddedTexture, mesh.material.texture));
			darkTextures.push_back(where(mesh.material.embeddedDarkTexture, mesh.material.darkTexture));
		}
		auto encoded = std::make_shared<const std::string>(NifFile::Encode(*model, textures, darkTextures));
		g_Disk.Store(target, *encoded);
		return encoded;
	}

	/**
	 * Model `path` (on disk at `file`) at `lod` in NifFile::Encode's format. Converting a big .nif takes a moment, so
	 * results are kept in memory (MESH_CACHE_BYTES; unless `keep` is false, for conversions ahead of time) and on disk
	 * (DISK_CACHE_BYTES). Any thread; the same model asked for again while it converts waits for that conversion.
	 */
	Bytes Encoded(const std::string& path, uint32_t lod, const std::filesystem::path& file, bool keep = true) {
		const auto key = ModelKey(path, lod);
		if (auto cached = g_Models.Get(key)) return cached;

		std::promise<Bytes> promise;
		std::shared_future<Bytes> converting;
		bool mine = false;
		{
			std::lock_guard lock(g_ConvertingMutex);
			const auto it = g_Converting.find(key);
			if (it != g_Converting.end()) {
				converting = it->second;
			} else {
				converting = promise.get_future().share();
				g_Converting.emplace(key, converting);
				mine = true;
			}
		}
		if (!mine) {
			auto result = converting.get();
			if (keep) g_Models.Put(key, result);
			return result;
		}

		Bytes result;
		try {
			result = Convert(path, lod, file, DiskPath(key, file));
		} catch (const std::exception& ex) {
			LOG("Could not convert %s: %s", path.c_str(), ex.what());
		}
		if (keep) g_Models.Put(key, result);
		{
			std::lock_guard lock(g_ConvertingMutex);
			g_Converting.erase(key);
		}
		promise.set_value(result);
		return result;
	}

	// The "textures" list of an encoded model's header
	std::vector<std::string> TexturesOf(const std::string& encoded) {
		uint32_t length{};
		if (encoded.size() < 4) return {};
		std::memcpy(&length, encoded.data(), 4);
		if (length > encoded.size() - 4) return {};
		const auto header = nlohmann::json::parse(encoded.substr(4, length), nullptr, false);
		if (header.is_discarded() || !header.contains("textures") || !header["textures"].is_array()) return {};
		std::vector<std::string> textures;
		for (const auto& texture : header["textures"]) textures.push_back(texture.is_string() ? texture.get<std::string>() : std::string{});
		return textures;
	}

	void Binary(HTTPReply& reply, std::string body) {
		reply.status = eHTTPStatusCode::OK;
		reply.contentType = eContentType::APPLICATION_OCTET_STREAM;
		reply.message = std::move(body);
		reply.headers.push_back("Cache-Control: private, max-age=604800");
	}

	uint32_t LodOf(const HTTPContext& context) {
		return std::min(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "lod")).value_or(0), MAX_LOD);
	}

	// ---- Converting on worker threads, so a big model never holds up the web server's one thread ----

	WorkerPool& Pool() { return Workers::Pool(); }

	constexpr uintmax_t SMALL_MODEL_BYTES = 256 * 1024;       // .nif files this small convert in the pool's fast lane
	constexpr uintmax_t LARGE_MODEL_BYTES = 4 * 1024 * 1024;  // and this big wait behind everything smaller
	constexpr auto WARM_IDLE = std::chrono::seconds(90);      // converting a zone ahead stops once nobody has asked for it this long
	constexpr uintmax_t WARM_DISK_BYTES = DISK_CACHE_BYTES * 3 / 4; // and when the disk cache is this full (it never evicts for it)
	constexpr size_t WARM_MAX_MODELS = 4000;

	// When someone last asked for something of a zone, and the LOD they last asked a model at
	struct Activity {
		std::chrono::steady_clock::time_point last;
		uint32_t lod{ 1 }; // the world view's default detail
	};
	std::mutex g_ActivityMutex;
	std::map<uint32_t, Activity> g_Activity;

	void Touch(uint32_t zoneId, std::optional<uint32_t> lod = std::nullopt) {
		std::lock_guard lock(g_ActivityMutex);
		auto& activity = g_Activity[zoneId];
		activity.last = std::chrono::steady_clock::now();
		if (lod) activity.lod = *lod;
	}

	// The zone's activity while someone views it, nullopt once nobody has for WARM_IDLE
	std::optional<Activity> Viewed(uint32_t zoneId) {
		std::lock_guard lock(g_ActivityMutex);
		const auto it = g_Activity.find(zoneId);
		if (it == g_Activity.end() || std::chrono::steady_clock::now() - it->second.last > WARM_IDLE) return std::nullopt;
		return it->second;
	}

	// Flairs (small, and drawn around the camera) and small models first; big ones behind the rest
	WorkerPool::ePriority PriorityOf(const ZoneScenery* zone, const std::string& path, const std::filesystem::path& file) {
		if (zone) {
			std::lock_guard lock(g_ZoneMutex);
			if (zone->flairModels.contains(path)) return WorkerPool::ePriority::URGENT;
		}
		std::error_code ec;
		const auto size = std::filesystem::file_size(file, ec);
		if (ec || size <= SMALL_MODEL_BYTES) return WorkerPool::ePriority::URGENT;
		return size >= LARGE_MODEL_BYTES ? WorkerPool::ePriority::LARGE : WorkerPool::ePriority::NORMAL;
	}

	uint64_t WarmGroup(uint32_t zoneId) { return static_cast<uint64_t>(zoneId) + 1; }

	/**
	 * Convert models of a zone ahead of time (onto the disk cache), at the LOD its viewer last asked for, while
	 * someone views it. The lowest priority: only when nothing else waits. Smallest first; `front` puts these before
	 * the zone's other queued ones (the flairs). Any thread.
	 */
	void WarmUp(uint32_t zoneId, const std::vector<std::string>& paths, bool front) {
		if (!Pool().Running() || paths.empty()) return;
		struct Item {
			std::string path;
			std::filesystem::path file;
			uintmax_t size{};
		};
		std::vector<Item> items;
		std::set<std::string> seen;
		std::error_code ec;
		const auto res = ClientAssets::ResFolder();
		for (const auto& path : paths) {
			if (!seen.insert(path).second) continue;
			const auto file = ClientAssets::ResolveResFile(path, res);
			if (file) items.push_back({ path, *file, std::filesystem::file_size(*file, ec) });
		}
		std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.size < b.size; });
		if (items.size() > WARM_MAX_MODELS) items.resize(WARM_MAX_MODELS);
		const auto group = WarmGroup(zoneId);
		// Queued at the front in reverse, so they still run smallest first
		if (front) std::reverse(items.begin(), items.end());
		for (auto& item : items) {
			Pool().Submit(WorkerPool::ePriority::BACKGROUND, [zoneId, group, path = std::move(item.path), file = std::move(item.file)] {
				const auto activity = Viewed(zoneId);
				if (!activity || g_Disk.Total() >= WARM_DISK_BYTES) {
					Pool().Cancel(group);
					return;
				}
				const auto key = ModelKey(path, activity->lod);
				std::error_code ec;
				if (g_Models.Get(key) || std::filesystem::exists(DiskPath(key, file), ec)) return;
				Encoded(path, activity->lod, file, false);
			}, group, front);
		}
	}

	/**
	 * Warm the zone's models when its manifest is asked for (again after nobody viewed it for a while). Planning it
	 * (finding the files) is left to a worker, so a manifest in memory is still answered at once.
	 */
	void WarmScenery(uint32_t zoneId, ZoneScenery& zone, bool flairs) {
		std::vector<std::string> paths;
		{
			const bool idle = !Viewed(zoneId);
			std::lock_guard lock(g_ZoneMutex);
			if (idle) zone.warmedScenery = zone.warmedFlairs = false;
			auto& warmed = flairs ? zone.warmedFlairs : zone.warmedScenery;
			if (!warmed) {
				warmed = true;
				if (flairs) paths.assign(zone.flairModels.begin(), zone.flairModels.end());
				else paths = zone.assets;
			}
		}
		Touch(zoneId);
		if (paths.empty() || !Pool().Running()) return;
		Pool().Submit(WorkerPool::ePriority::NORMAL, [zoneId, paths = std::move(paths), flairs] { WarmUp(zoneId, paths, flairs); });
	}

	// The texture `name` of model `path` (textures[slot] of its encoded form) as a DDS file. Any thread.
	std::optional<std::string> TextureBytes(const std::string& path, const std::filesystem::path& modelFile, const std::vector<std::string>& textures,
		const std::string& name, const std::filesystem::path& res) {
		if (!name.starts_with('#')) {
			const auto file = ClientAssets::ResolveResFile(name, res);
			return file ? ReadWhole(*file) : std::nullopt;
		}
		// Stored inside the model: reading a big .nif again for each of its textures would be slow, so they're kept
		if (const auto cached = g_Embedded.Get(path + name)) return *cached;
		const auto data = ReadWhole(modelFile);
		const auto block = GeneralUtils::TryParse<int32_t>(name.substr(1));
		if (!data || !block) return std::nullopt;
		// Every texture of the file at once, since the browser asks for them together
		for (const auto& other : textures) {
			if (other == name) continue;
			const auto otherBlock = other.starts_with('#') ? GeneralUtils::TryParse<int32_t>(other.substr(1)) : std::nullopt;
			auto file = otherBlock ? NifFile::EmbeddedTexture(*data, *otherBlock) : std::nullopt;
			if (file) g_Embedded.Put(path + other, std::make_shared<const std::string>(std::move(*file)));
		}
		auto dds = NifFile::EmbeddedTexture(*data, *block);
		if (dds) g_Embedded.Put(path + name, std::make_shared<const std::string>(*dds));
		return dds;
	}

	// The model path of `asset` in the zone's manifests, building what's missing (any thread); nullopt: no such model
	std::optional<std::string> AssetPath(uint32_t zoneId, uint32_t asset) {
		const auto zone = Zone(zoneId);
		if (!zone) return std::nullopt;
		{
			std::lock_guard lock(g_ZoneMutex);
			if (asset < zone->assets.size()) return zone->assets[asset];
		}
		// The flairs' models join the list when their manifest is first built (a browser may still have it cached)
		Flairs(zoneId, *zone);
		std::lock_guard lock(g_ZoneMutex);
		if (asset < zone->assets.size()) return zone->assets[asset];
		return std::nullopt;
	}

	// The same without building anything (never waits): nullopt when it isn't known yet
	std::optional<std::string> AssetPathIfBuilt(uint32_t zoneId, uint32_t asset) {
		const auto zone = ZoneIfBuilt(zoneId);
		if (!zone) return std::nullopt;
		std::lock_guard lock(g_ZoneMutex);
		if (asset < zone->assets.size()) return zone->assets[asset];
		return std::nullopt;
	}

	/**
	 * Where model `asset` is, and how urgent converting it is, when that is known without building anything: for
	 * the web thread, which only hands the work on
	 */
	struct Known {
		std::optional<std::string> path;
		std::optional<std::filesystem::path> file;
		WorkerPool::ePriority priority{ WorkerPool::ePriority::NORMAL };
	};

	Known KnownAsset(uint32_t zoneId, uint32_t asset) {
		Known known;
		known.path = AssetPathIfBuilt(zoneId, asset);
		if (!known.path) return known;
		known.file = ClientAssets::ResolveResFile(*known.path, ClientAssets::ResFolder());
		if (known.file) known.priority = PriorityOf(ZoneIfBuilt(zoneId).get(), *known.path, *known.file);
		return known;
	}
}

namespace Scenery {
	void Preload() {
		Files();
		RenderInfos();
		FlairModels();
	}

	std::optional<std::string> ZoneJson(uint32_t zoneId) {
		const auto zone = Zone(zoneId);
		if (!zone) return std::nullopt;
		WarmScenery(zoneId, *zone, false);
		return zone->json;
	}

	std::vector<uint16_t> MultishaderLooks(const NifFile::Model& model) {
		std::vector<uint16_t> looks;
		for (const auto& mesh : model.meshes) {
			std::optional<int32_t> shader;
			if (const auto it = g_ShaderValues.find(mesh.material.shaderTag); it != g_ShaderValues.end()) shader = it->second;
			looks.push_back(NifFile::ShaderLookFor(NifFile::MultishaderPart(shader)));
		}
		return looks;
	}

	bool ZoneReady(uint32_t zoneId) {
		return g_Zones.Ready(zoneId);
	}

	bool HasModel(const WorldScene::Object& object) {
		const auto model = ModelFor(object);
		return !model.path.empty() && !model.hidden;
	}

	std::optional<std::string> FlairsJson(uint32_t zoneId) {
		const auto zone = Zone(zoneId);
		if (!zone) return std::nullopt;
		const auto& flairs = Flairs(zoneId, *zone);
		WarmScenery(zoneId, *zone, true);
		return flairs;
	}

	bool FlairsReady(uint32_t zoneId) {
		return g_Zones.Ready(zoneId) && g_Flairs.Ready(zoneId);
	}

	void ReplyMesh(HTTPReply& reply, const HTTPContext& context, uint32_t zoneId, uint32_t asset, uint32_t lod) {
		lod = std::min(lod, MAX_LOD);
		Touch(zoneId, lod);
		auto known = KnownAsset(zoneId, asset);
		if (known.path) {
			if (const auto cached = g_Models.Get(ModelKey(*known.path, lod))) return Binary(reply, *cached);
		}
		const auto res = ClientAssets::ResFolder();
		const auto deferred = Web::Defer(reply, context);
		Pool().Submit(known.priority, [deferred, zoneId, asset, lod, res, known = std::move(known)] {
			if (deferred.Cancelled()) return;
			HTTPReply out;
			const auto path = known.path ? known.path : AssetPath(zoneId, asset);
			const auto file = known.file ? known.file : path ? ClientAssets::ResolveResFile(*path, res) : std::nullopt;
			if (!path) {
				JsonError(out, eHTTPStatusCode::NOT_FOUND, "No such model in this zone");
			} else if (const auto encoded = file ? Encoded(*path, lod, *file) : nullptr) {
				Binary(out, *encoded);
			} else {
				JsonError(out, eHTTPStatusCode::NOT_FOUND, "Could not read this model");
			}
			deferred.Send(std::move(out));
		});
	}

	void ReplyTexture(HTTPReply& reply, const HTTPContext& context, uint32_t zoneId, uint32_t asset, uint32_t slot, uint32_t lod) {
		lod = std::min(lod, MAX_LOD);
		Touch(zoneId, lod);
		auto known = KnownAsset(zoneId, asset);
		// Quick when the model is converted already and the texture is a file of its own, or one kept from its model
		if (known.path) {
			if (const auto cached = g_Models.Get(ModelKey(*known.path, lod))) {
				const auto textures = TexturesOf(*cached);
				if (slot >= textures.size() || !textures[slot].starts_with('#') || g_Embedded.Get(*known.path + textures[slot])) known.priority = WorkerPool::ePriority::URGENT;
			}
		}
		const auto res = ClientAssets::ResFolder();
		const auto deferred = Web::Defer(reply, context);
		Pool().Submit(known.priority, [deferred, zoneId, asset, lod, slot, res, known = std::move(known)] {
			if (deferred.Cancelled()) return;
			HTTPReply out;
			const auto path = known.path ? known.path : AssetPath(zoneId, asset);
			const auto file = known.file ? known.file : path ? ClientAssets::ResolveResFile(*path, res) : std::nullopt;
			const auto encoded = path && file ? Encoded(*path, lod, *file) : nullptr;
			const auto textures = encoded ? TexturesOf(*encoded) : std::vector<std::string>{};
			if (!path) {
				JsonError(out, eHTTPStatusCode::NOT_FOUND, "No such model in this zone");
			} else if (slot >= textures.size() || textures[slot].empty()) {
				JsonError(out, eHTTPStatusCode::NOT_FOUND, "No such texture");
			} else if (auto dds = TextureBytes(*path, *file, textures, textures[slot], res)) {
				Binary(out, std::move(*dds));
			} else {
				JsonError(out, eHTTPStatusCode::NOT_FOUND, "Could not read this texture");
			}
			deferred.Send(std::move(out));
		});
	}

	/**
	 * The environment textures the client's shaders load themselves, by the name the viewers ask for them: the default
	 * reflection cube (LEGOPPLighting, ClearPlastic) and Metallic.fx's cubes and noise.
	 */
	static const std::map<std::string, std::string>& EnvironmentTextures() {
		static const std::map<std::string, std::string> textures{
			{ "reflection", "textures/env/default_reflection.dds" },
			{ "polished", "textures/metal/metal_reflection_polished.dds" },
			{ "brushed", "textures/metal/metal_reflection_brushed.dds" },
			{ "brushedNoise", "textures/metal/metal_reflection_brushed_noise.dds" }
		};
		return textures;
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/api/scenery/env/:name", 0,
			"An environment texture the client's shaders load themselves, as a DDS file: reflection (the default reflection cube), polished, brushed (the metal cubes) or brushedNoise",
			[](HTTPReply& reply, const HTTPContext& context) {
				const std::string name(PathSegment(context.path, 3));
				const auto& textures = EnvironmentTextures();
				const auto it = textures.find(name);
				if (it == textures.end()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such environment texture");
				// Read once: a few megabytes the views ask for with every zone
				static std::mutex mutex;
				static std::map<std::string, std::shared_ptr<const std::string>> cache;
				std::shared_ptr<const std::string> bytes;
				{
					std::lock_guard lock(mutex);
					if (const auto cached = cache.find(name); cached != cache.end()) bytes = cached->second;
				}
				if (!bytes) {
					auto read = ClientAssets::ReadResFile(it->second);
					if (!read) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "The client has no such texture");
					bytes = std::make_shared<const std::string>(std::move(*read));
					std::lock_guard lock(mutex);
					cache.try_emplace(name, bytes);
				}
				Binary(reply, *bytes);
			});

		Route(eHTTPMethod::GET, "/api/scenery/:zone/mesh/:asset", 0,
			"Model `asset` of a zone's scenery (see the scenery routes of properties and /world3d), converted from the client's .nif. Query: ?lod=0 (most detailed) to 3",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = PathId<uint32_t>(context.path, 2);
				const auto asset = PathId<uint32_t>(context.path, 4);
				if (!zone || !asset) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone or model");
				ReplyMesh(reply, context, *zone, *asset, LodOf(context));
			});

		Route(eHTTPMethod::GET, "/api/scenery/:zone/texture/:asset/:slot", 0,
			"Texture `slot` of scenery model `asset` (its \"textures\" list) as a DDS file. Query: ?lod= as for the model",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = PathId<uint32_t>(context.path, 2);
				const auto asset = PathId<uint32_t>(context.path, 4);
				const auto slot = PathId<uint32_t>(context.path, 5);
				if (!zone || !asset || !slot) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone, model or texture");
				ReplyTexture(reply, context, *zone, *asset, *slot, LodOf(context));
			});
	}
}
