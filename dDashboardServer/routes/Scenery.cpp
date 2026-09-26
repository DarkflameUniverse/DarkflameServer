#include "Scenery.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "ClientAssets.h"
#include "NifFile.h"
#include "ReportRoutes.h"
#include "RouteUtils.h"
#include "TtlCache.h"
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

	// Keeps the disk cache under DISK_CACHE_BYTES by removing the least recently written files
	void StoreOnDisk(const std::filesystem::path& target, const std::string& data) {
		static std::optional<uintmax_t> total;
		std::error_code ec;
		std::filesystem::create_directories(CACHE_DIR, ec);
		if (!total) {
			total = 0;
			for (const auto& entry : std::filesystem::directory_iterator(CACHE_DIR, ec)) *total += entry.is_regular_file(ec) ? entry.file_size(ec) : 0;
		}
		if (*total + data.size() > DISK_CACHE_BYTES) {
			std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> files;
			for (const auto& entry : std::filesystem::directory_iterator(CACHE_DIR, ec)) files.emplace_back(entry.last_write_time(ec), entry.path());
			std::sort(files.begin(), files.end());
			for (const auto& [time, path] : files) {
				if (*total + data.size() <= DISK_CACHE_BYTES * 3 / 4) break;
				const auto size = std::filesystem::file_size(path, ec);
				if (std::filesystem::remove(path, ec)) *total -= std::min(*total, size);
			}
		}
		const auto temporary = target.string() + ".tmp";
		{
			std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
			if (!file.write(data.data(), static_cast<std::streamsize>(data.size()))) return;
		}
		std::filesystem::rename(temporary, target, ec);
		if (!ec) *total += data.size();
	}

	/**
	 * Model `path` at `lod` in NifFile::Encode's format. Converting a big .nif takes a moment, so results are kept in
	 * memory (MESH_CACHE_BYTES) and on disk (DISK_CACHE_BYTES), keyed by the file's size and time so a changed client
	 * file is converted again.
	 */
	std::shared_ptr<const std::string> Encoded(const std::string& path, uint32_t lod) {
		static TtlCache<std::string, std::shared_ptr<const std::string>> memory(std::chrono::hours(1), MESH_CACHE_BYTES);
		const auto key = path + "|" + std::to_string(lod);
		if (auto cached = memory.Get(key)) return *cached;

		const auto file = ClientAssets::ResolveResFile(path);
		if (!file) return nullptr;
		std::error_code ec;
		const auto size = std::filesystem::file_size(*file, ec);
		const auto time = std::filesystem::last_write_time(*file, ec).time_since_epoch().count();
		const auto diskKey = key + "|" + std::to_string(size) + "|" + std::to_string(time) + "|" + std::to_string(FORMAT_VERSION);
		const auto target = CACHE_DIR / (std::to_string(Fnv1a(diskKey)) + ".bin");

		auto encoded = ReadWhole(target);
		if (!encoded) {
			const auto data = ReadWhole(*file);
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
			encoded = NifFile::Encode(*model, textures);
			StoreOnDisk(target, *encoded);
		}
		auto shared = std::make_shared<const std::string>(std::move(*encoded));
		memory.Put(key, shared, shared->size() + 256);
		return shared;
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
}

namespace Scenery {
	std::optional<std::string> ZoneJson(uint32_t zoneId) {
		const auto& zone = Zone(zoneId);
		if (!zone) return std::nullopt;
		return zone->json;
	}

	bool HasModel(const WorldScene::Object& object) {
		const auto model = ModelFor(object);
		return !model.path.empty() && !model.hidden;
	}

	std::optional<std::string> FlairsJson(uint32_t zoneId) {
		auto& zone = Zone(zoneId);
		if (!zone) return std::nullopt;
		return Flairs(zoneId, *zone);
	}

	void ReplyMesh(HTTPReply& reply, uint32_t zoneId, uint32_t asset, uint32_t lod) {
		auto& zone = Zone(zoneId);
		// The flairs' models join the list when their manifest is first built (a browser may still have it cached)
		if (zone && asset >= zone->assets.size()) Flairs(zoneId, *zone);
		if (!zone || asset >= zone->assets.size()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such model in this zone");
		const auto encoded = Encoded(zone->assets[asset], std::min(lod, MAX_LOD));
		if (!encoded) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Could not read this model");
		Binary(reply, *encoded);
	}

	void ReplyTexture(HTTPReply& reply, uint32_t zoneId, uint32_t asset, uint32_t slot, uint32_t lod) {
		auto& zone = Zone(zoneId);
		if (zone && asset >= zone->assets.size()) Flairs(zoneId, *zone);
		if (!zone || asset >= zone->assets.size()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such model in this zone");
		const auto encoded = Encoded(zone->assets[asset], std::min(lod, MAX_LOD));
		const auto textures = encoded ? TexturesOf(*encoded) : std::vector<std::string>{};
		if (slot >= textures.size() || textures[slot].empty()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such texture");
		const auto& texture = textures[slot];
		std::optional<std::string> dds;
		if (texture.starts_with('#')) {
			// Stored inside the model: reading a big .nif again for each of its textures would be slow, so they're kept
			static TtlCache<std::string, std::shared_ptr<const std::string>> embedded(std::chrono::hours(1), MESH_CACHE_BYTES);
			const auto key = zone->assets[asset] + texture;
			if (const auto cached = embedded.Get(key)) return Binary(reply, **cached);
			const auto data = ClientAssets::ReadResFile(zone->assets[asset]);
			const auto block = GeneralUtils::TryParse<int32_t>(texture.substr(1));
			if (data && block) {
				// Every texture of the file at once, since the browser asks for them together
				for (const auto& other : textures) {
					const auto otherBlock = other.starts_with('#') ? GeneralUtils::TryParse<int32_t>(other.substr(1)) : std::nullopt;
					auto file = otherBlock ? NifFile::EmbeddedTexture(*data, *otherBlock) : std::nullopt;
					if (!file) continue;
					const auto bytes = file->size();
					embedded.Put(zone->assets[asset] + other, std::make_shared<const std::string>(std::move(*file)), bytes);
				}
				dds = NifFile::EmbeddedTexture(*data, *block);
			}
		} else {
			dds = ClientAssets::ReadResFile(texture);
		}
		if (!dds) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Could not read this texture");
		Binary(reply, std::move(*dds));
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/api/scenery/:zone/mesh/:asset", 0,
			"Model `asset` of a zone's scenery (see the scenery routes of properties and /world3d), converted from the client's .nif. Query: ?lod=0 (most detailed) to 3",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = PathId<uint32_t>(context.path, 2);
				const auto asset = PathId<uint32_t>(context.path, 4);
				if (!zone || !asset) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone or model");
				ReplyMesh(reply, *zone, *asset, LodOf(context));
			});

		Route(eHTTPMethod::GET, "/api/scenery/:zone/texture/:asset/:slot", 0,
			"Texture `slot` of scenery model `asset` (its \"textures\" list) as a DDS file. Query: ?lod= as for the model",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto zone = PathId<uint32_t>(context.path, 2);
				const auto asset = PathId<uint32_t>(context.path, 4);
				const auto slot = PathId<uint32_t>(context.path, 5);
				if (!zone || !asset || !slot) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone, model or texture");
				ReplyTexture(reply, *zone, *asset, *slot, LodOf(context));
			});
	}
}
