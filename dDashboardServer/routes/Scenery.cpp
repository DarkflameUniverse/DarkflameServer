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
#include "TtlCache.h"
#include "Web.h"
#include "WorkerPool.h"
#include "WorldScene.h"
#include "ZonePaths.h"

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

	const FileIndex& Files() {
		static std::optional<FileIndex> index;
		if (index) return *index;
		index.emplace();
		const auto client = Game::config->GetValue("client_location");
		if (client.empty()) return *index;
		const auto res = std::filesystem::path(client) / "res";
		std::error_code ec;
		for (const char* folder : { "mesh", "textures", "animations" }) {
			// Unpacked clients keep their original case, so the top folder is matched ignoring case too
			for (const auto& top : std::filesystem::directory_iterator(res, ec)) {
				if (Lower(top.path().filename().string()) != folder || !top.is_directory(ec)) continue;
				for (auto it = std::filesystem::recursive_directory_iterator(top.path(), ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
					if (!it->is_regular_file(ec)) continue;
					const auto relative = Lower(std::filesystem::relative(it->path(), res, ec).generic_string());
					index->paths.insert(relative);
					index->byName[Lower(it->path().filename().string())].push_back(relative);
				}
			}
		}
		return *index;
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
	};

	// Every LOT with a render component, read once (Objects has no index on id, so it is read whole too)
	const std::unordered_map<uint32_t, RenderInfo>& RenderInfos() {
		static std::optional<std::unordered_map<uint32_t, RenderInfo>> infos;
		if (infos) return *infos;
		infos.emplace();
		try {
			auto row = CDClientDatabase::ExecuteQuery(
				"SELECT cr.id, rc.render_asset FROM ComponentsRegistry cr JOIN RenderComponent rc ON rc.id = cr.component_id "
				"WHERE cr.component_type = " + std::to_string(RENDER_COMPONENT) + ";");
			for (; !row.eof(); row.nextRow()) infos->try_emplace(static_cast<uint32_t>(row.getIntField(0)), RenderInfo{ row.getStringField(1, ""), "" });
			auto types = CDClientDatabase::ExecuteQuery("SELECT id, type FROM Objects;");
			for (; !types.eof(); types.nextRow()) {
				const auto it = infos->find(static_cast<uint32_t>(types.getIntField(0)));
				if (it != infos->end() && it->second.type.empty()) it->second.type = types.getStringField(1, "");
			}
		} catch (const std::exception& ex) {
			LOG("Could not read the render components of objects: %s", ex.what());
		}
		return *infos;
	}

	struct Model {
		std::string path;  // res path of the .nif, empty for none
		bool hidden{};     // the client doesn't draw it (WorldScene::ClientDraws)
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
		static std::unordered_map<std::string, std::string> resolved;
		const auto& stored = object.nifName.empty() ? info->second.asset : object.nifName;
		auto it = resolved.find(stored);
		if (it == resolved.end()) it = resolved.emplace(stored, ResolveModel(stored)).first;
		return { it->second, draw == WorldScene::eClientDraw::HIDDEN };
	}

	std::optional<std::string> LuzPath(uint32_t zone) {
		auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT zoneName FROM ZoneTable WHERE zoneID = ? LIMIT 1;");
		stmt.bind(1, static_cast<int>(zone));
		auto result = stmt.execQuery();
		if (result.eof()) return std::nullopt;
		std::string path = result.getStringField(0, "");
		if (!path.ends_with(".luz")) return std::nullopt;
		return path;
	}

	double Round(float value, double scale) { return std::round(static_cast<double>(value) * scale) / scale; }

	struct ZoneScenery {
		std::vector<std::string> assets;      // res paths of models, indexed by the manifests
		std::map<std::string, size_t> index;  // res path -> its index in assets
		std::string json;
		std::optional<std::string> flairs;    // the flairs' manifest, built when first asked for (their models join assets)
		std::unordered_set<std::string> flairModels; // res paths of the flairs' models, converted ahead of others
		bool warmedScenery{};                 // WarmUp queued the scenery's models
		bool warmedFlairs{};                  // and the flairs'

		size_t IndexOf(const std::string& path) {
			const auto [it, added] = index.try_emplace(path, assets.size());
			if (added) assets.push_back(path);
			return it->second;
		}
	};

	std::optional<ZoneScenery>& Zone(uint32_t zoneId) {
		static std::map<uint32_t, std::optional<ZoneScenery>> cache;
		if (const auto it = cache.find(zoneId); it != cache.end()) return it->second;
		auto& entry = cache[zoneId];

		const auto luzPath = LuzPath(zoneId);
		const auto luz = luzPath ? ClientAssets::ReadResFile("maps/" + *luzPath) : std::nullopt;
		if (!luz) return entry;
		const auto folder = luzPath->substr(0, luzPath->find_last_of('/') + 1);

		ZoneScenery scenery;
		nlohmann::json assetOf = nlohmann::json::array(), positions = nlohmann::json::array(), rotations = nlohmann::json::array(), scales = nlohmann::json::array();
		nlohmann::json hidden = nlohmann::json::array();
		int64_t sky = -1;
		for (const auto& scene : ZonePaths::ReadSceneFiles(*luz)) {
			const auto lvl = ClientAssets::ReadResFile("maps/" + folder + scene);
			if (!lvl) continue;
			if (sky < 0) {
				const auto skydome = JoinPath("", WorldScene::ReadSkydome(*lvl));
				if (skydome.ends_with(".nif") && Files().paths.contains(skydome)) sky = static_cast<int64_t>(scenery.IndexOf(skydome));
			}
			for (const auto& object : WorldScene::ReadObjects(*lvl)) {
				// A spawner is drawn as what it spawns, where the client would show it
				const auto model = ModelFor(object);
				if (model.path.empty()) continue;
				assetOf.push_back(scenery.IndexOf(model.path));
				hidden.push_back(model.hidden ? 1 : 0);
				for (const auto value : { object.x, object.y, object.z }) positions.push_back(Round(value, 100.0));
				for (const auto value : { object.qx, object.qy, object.qz, object.qw }) rotations.push_back(Round(value, 10000.0));
				scales.push_back(Round(object.scale, 1000.0));
			}
		}
		scenery.json = nlohmann::json{
			{"zone", zoneId}, {"sky", sky}, {"assets", scenery.assets},
			{"objects", { {"asset", assetOf}, {"pos", positions}, {"rot", rotations}, {"scale", scales}, {"hidden", hidden} }}
		}.dump();
		entry = std::move(scenery);
		return entry;
	}

	// FlairTable: flair id -> the model's res path (empty when the client lacks it), read once
	const std::unordered_map<uint32_t, std::string>& FlairModels() {
		static std::optional<std::unordered_map<uint32_t, std::string>> models;
		if (models) return *models;
		models.emplace();
		try {
			auto row = CDClientDatabase::ExecuteQuery("SELECT id, asset FROM FlairTable;");
			for (; !row.eof(); row.nextRow()) models->try_emplace(static_cast<uint32_t>(row.getIntField(0)), ResolveModel(row.getStringField(1, "")));
		} catch (const std::exception& ex) {
			LOG("Could not read the flairs: %s", ex.what());
		}
		return *models;
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
	const std::optional<std::string>& Flairs(uint32_t zoneId, ZoneScenery& scenery) {
		if (scenery.flairs) return scenery.flairs;
		const auto raw = ZoneRaw(zoneId);
		const auto& models = FlairModels();
		nlohmann::json assetOf = nlohmann::json::array(), positions = nlohmann::json::array(), rotations = nlohmann::json::array(),
			scales = nlohmann::json::array(), colors = nlohmann::json::array();
		for (const auto& chunk : raw ? raw->chunks : std::vector<Raw::Chunk>{}) {
			for (const auto& flair : chunk.flairs) {
				const auto model = models.find(flair.id);
				if (model == models.end() || model->second.empty() || !std::isfinite(flair.position.x)) continue;
				assetOf.push_back(scenery.IndexOf(model->second));
				scenery.flairModels.insert(model->second);
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
		scenery.flairs = nlohmann::json{
			{"zone", zoneId}, {"sky", -1}, {"assets", scenery.assets}, {"distance", FLAIR_DISTANCE}, {"colorScale", 1.0 / 63.0},
			{"objects", { {"asset", assetOf}, {"pos", positions}, {"rot", rotations}, {"scale", scales}, {"color", colors} }}
		}.dump();
		return scenery.flairs;
	}

	constexpr uint32_t FORMAT_VERSION = 1; // bump when NifFile's output changes, so cached files are rebuilt
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
		std::vector<std::string> textures; // per mesh: a res path, "#<block>" for one stored in the .nif, or empty
		for (const auto& mesh : model->meshes) {
			if (mesh.material.embeddedTexture >= 0) textures.push_back("#" + std::to_string(mesh.material.embeddedTexture));
			else textures.push_back(mesh.material.texture.empty() ? std::string{} : FindTexture(folder, mesh.material.texture));
		}
		auto encoded = std::make_shared<const std::string>(NifFile::Encode(*model, textures));
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

	WorkerPool g_Pool;

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
	WorkerPool::ePriority PriorityOf(const ZoneScenery& zone, const std::string& path, const std::filesystem::path& file) {
		if (zone.flairModels.contains(path)) return WorkerPool::ePriority::URGENT;
		std::error_code ec;
		const auto size = std::filesystem::file_size(file, ec);
		if (ec || size <= SMALL_MODEL_BYTES) return WorkerPool::ePriority::URGENT;
		return size >= LARGE_MODEL_BYTES ? WorkerPool::ePriority::LARGE : WorkerPool::ePriority::NORMAL;
	}

	uint64_t WarmGroup(uint32_t zoneId) { return static_cast<uint64_t>(zoneId) + 1; }

	/**
	 * Convert models of a zone ahead of time (onto the disk cache), at the LOD its viewer last asked for, while
	 * someone views it. The lowest priority: only when nothing else waits. Smallest first; `front` puts these before
	 * the zone's other queued ones (the flairs). Web thread.
	 */
	void WarmUp(uint32_t zoneId, const std::vector<std::string>& paths, bool front) {
		if (!g_Pool.Running() || paths.empty()) return;
		Files(); // built here: workers only read it
		struct Item {
			std::string path;
			std::filesystem::path file;
			uintmax_t size{};
		};
		std::vector<Item> items;
		std::set<std::string> seen;
		std::error_code ec;
		for (const auto& path : paths) {
			if (!seen.insert(path).second) continue;
			const auto file = ClientAssets::ResolveResFile(path);
			if (file) items.push_back({ path, *file, std::filesystem::file_size(*file, ec) });
		}
		std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.size < b.size; });
		if (items.size() > WARM_MAX_MODELS) items.resize(WARM_MAX_MODELS);
		const auto group = WarmGroup(zoneId);
		// Queued at the front in reverse, so they still run smallest first
		if (front) std::reverse(items.begin(), items.end());
		for (auto& item : items) {
			g_Pool.Submit(WorkerPool::ePriority::BACKGROUND, [zoneId, group, path = std::move(item.path), file = std::move(item.file)] {
				const auto activity = Viewed(zoneId);
				if (!activity || g_Disk.Total() >= WARM_DISK_BYTES) {
					g_Pool.Cancel(group);
					return;
				}
				const auto key = ModelKey(path, activity->lod);
				std::error_code ec;
				if (g_Models.Get(key) || std::filesystem::exists(DiskPath(key, file), ec)) return;
				Encoded(path, activity->lod, file, false);
			}, group, front);
		}
	}

	// Warm the zone's models when its manifest is asked for (again after nobody viewed it for a while)
	void WarmScenery(uint32_t zoneId, ZoneScenery& zone, bool flairs) {
		if (!Viewed(zoneId)) zone.warmedScenery = zone.warmedFlairs = false;
		Touch(zoneId);
		auto& warmed = flairs ? zone.warmedFlairs : zone.warmedScenery;
		if (warmed) return;
		warmed = true;
		if (flairs) WarmUp(zoneId, { zone.flairModels.begin(), zone.flairModels.end() }, true);
		else WarmUp(zoneId, zone.assets, false);
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

	// The model path of `asset` in the zone's manifests, or a 404 reply
	const std::string* AssetPath(HTTPReply& reply, uint32_t zoneId, uint32_t asset) {
		auto& zone = Zone(zoneId);
		// The flairs' models join the list when their manifest is first built (a browser may still have it cached)
		if (zone && asset >= zone->assets.size()) Flairs(zoneId, *zone);
		if (!zone || asset >= zone->assets.size()) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such model in this zone");
			return nullptr;
		}
		return &zone->assets[asset];
	}
}

namespace Scenery {
	std::optional<std::string> ZoneJson(uint32_t zoneId) {
		auto& zone = Zone(zoneId);
		if (!zone) return std::nullopt;
		WarmScenery(zoneId, *zone, false);
		return zone->json;
	}

	bool HasModel(const WorldScene::Object& object) {
		const auto model = ModelFor(object);
		return !model.path.empty() && !model.hidden;
	}

	std::optional<std::string> FlairsJson(uint32_t zoneId) {
		auto& zone = Zone(zoneId);
		if (!zone) return std::nullopt;
		const auto& flairs = Flairs(zoneId, *zone);
		WarmScenery(zoneId, *zone, true);
		return flairs;
	}

	void ReplyMesh(HTTPReply& reply, const HTTPContext& context, uint32_t zoneId, uint32_t asset, uint32_t lod) {
		const auto* found = AssetPath(reply, zoneId, asset);
		if (!found) return;
		const auto path = *found;
		lod = std::min(lod, MAX_LOD);
		Touch(zoneId, lod);
		if (const auto cached = g_Models.Get(ModelKey(path, lod))) return Binary(reply, *cached);
		const auto file = ClientAssets::ResolveResFile(path);
		if (!file) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Could not read this model");
		Files(); // built here: workers only read it
		const auto deferred = Web::Defer(reply, context);
		g_Pool.Submit(PriorityOf(*Zone(zoneId), path, *file), [deferred, path, lod, file = *file] {
			if (deferred.Cancelled()) return;
			HTTPReply out;
			const auto encoded = Encoded(path, lod, file);
			if (encoded) Binary(out, *encoded);
			else JsonError(out, eHTTPStatusCode::NOT_FOUND, "Could not read this model");
			deferred.Send(std::move(out));
		});
	}

	void ReplyTexture(HTTPReply& reply, const HTTPContext& context, uint32_t zoneId, uint32_t asset, uint32_t slot, uint32_t lod) {
		const auto* found = AssetPath(reply, zoneId, asset);
		if (!found) return;
		const auto path = *found;
		lod = std::min(lod, MAX_LOD);
		Touch(zoneId, lod);
		const auto file = ClientAssets::ResolveResFile(path);
		if (!file) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such texture");
		Files();
		// Quick when the model is converted already and the texture is a file of its own, or one kept from its model
		auto priority = WorkerPool::ePriority::URGENT;
		if (const auto cached = g_Models.Get(ModelKey(path, lod))) {
			const auto textures = TexturesOf(*cached);
			if (slot < textures.size() && textures[slot].starts_with('#') && !g_Embedded.Get(path + textures[slot])) priority = PriorityOf(*Zone(zoneId), path, *file);
		} else {
			priority = PriorityOf(*Zone(zoneId), path, *file);
		}
		const auto deferred = Web::Defer(reply, context);
		g_Pool.Submit(priority, [deferred, path, lod, slot, file = *file, res = ClientAssets::ResFolder()] {
			if (deferred.Cancelled()) return;
			HTTPReply out;
			const auto encoded = Encoded(path, lod, file);
			const auto textures = encoded ? TexturesOf(*encoded) : std::vector<std::string>{};
			if (slot >= textures.size() || textures[slot].empty()) {
				JsonError(out, eHTTPStatusCode::NOT_FOUND, "No such texture");
			} else if (auto dds = TextureBytes(path, file, textures, textures[slot], res)) {
				Binary(out, std::move(*dds));
			} else {
				JsonError(out, eHTTPStatusCode::NOT_FOUND, "Could not read this texture");
			}
			deferred.Send(std::move(out));
		});
	}

	void Shutdown() {
		g_Pool.Stop();
	}

	void RegisterRoutes() {
		auto threads = Game::config ? Game::config->GetValue<uint32_t>("scenery_workers", 0) : 0;
		if (threads == 0) threads = static_cast<uint32_t>(WorkerPool::DefaultThreads(std::thread::hardware_concurrency()));
		threads = std::clamp<uint32_t>(threads, 2, 16);
		// Converting ahead of time never takes more than half of the threads besides the fast lane
		g_Pool.Start(threads, std::max<size_t>(1, (threads - 1) / 2));
		LOG("Converting scenery models with %u threads", threads);

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
