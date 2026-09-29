#include "ClientAssets.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

#include "OnceCache.h"
#include "UgcBricks.h"
#include "UgcRoutes.h"
#include "RouteUtils.h"
#include "CDClientDatabase.h"
#include "Game.h"
#include "Logger.h"
#include "Locale.h"
#include "dConfig.h"
#include "eHTTPMethod.h"
#include "GeneralUtils.h"
#include "Process.h"

using namespace RouteUtils;

namespace {
	constexpr uint32_t MAX_ITEMS_PER_REQUEST = 500;
	constexpr size_t MAX_LISTING_ENTRIES = 2000;

	std::map<LOT, nlohmann::json> g_ItemCache;
	std::map<LOT, std::vector<int>> g_ItemSetsByLot; // lot -> set ids containing it
	bool g_ItemSetsLoaded = false;

	// Read once (client_location only changes with a restart), so worker threads never read the settings
	std::filesystem::path ClientRes() {
		static const std::filesystem::path res = [] {
			const auto client = Game::config ? Game::config->GetValue("client_location") : std::string{};
			return client.empty() ? std::filesystem::path{} : std::filesystem::path(client) / "res";
		}();
		return res;
	}

	// Lowercase, forward slashes, no leading "../" or "res/"
	std::string NormalizeAssetPath(std::string path) {
		std::replace(path.begin(), path.end(), '\\', '/');
		std::transform(path.begin(), path.end(), path.begin(), ::tolower);
		while (path.starts_with("../")) path = path.substr(3);
		if (path.starts_with("res/")) path = path.substr(4);
		while (path.starts_with("/")) path = path.substr(1);
		return path;
	}

	/**
	 * Where a normalized (lowercase) res/ path is on disk. Unpacked clients keep their original mixed case, so on
	 * case-sensitive file systems each part is matched ignoring case.
	 */
	std::filesystem::path ResolveResIn(const std::filesystem::path& res, const std::string& normalized) {
		auto current = res;
		std::error_code ec;
		for (const auto& part : GeneralUtils::SplitString(normalized, '/')) {
			if (part.empty()) continue;
			if (std::filesystem::exists(current / part, ec)) {
				current /= part;
				continue;
			}
			std::filesystem::path match;
			if (std::filesystem::is_directory(current, ec)) {
				for (const auto& entry : std::filesystem::directory_iterator(current, ec)) {
					auto name = entry.path().filename().string();
					std::transform(name.begin(), name.end(), name.begin(), ::tolower);
					if (name == part) {
						match = entry.path();
						break;
					}
				}
			}
			current = match.empty() ? current / part : match;
		}
		return current;
	}

	std::filesystem::path ResolveRes(const std::string& normalized) {
		return ResolveResIn(ClientRes(), normalized);
	}

	std::optional<std::string> ReadFile(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file) return std::nullopt;
		// One read of the whole file: copying through stream iterators is slow (very slow in debug builds)
		const auto size = file.tellg();
		if (size < 0) return std::nullopt;
		std::string data(static_cast<size_t>(size), '\0');
		file.seekg(0);
		if (!file.read(data.data(), size)) return std::nullopt;
		return data;
	}

	std::string Phrase(const std::string& key) {
		const auto& phrase = Locale::GetPhrase(key);
		return phrase;
	}

	struct Stats {
		int imagination{ 0 };
		int life{ 0 };
		int armor{ 0 };
		nlohmann::json skills = nlohmann::json::array();
	};

	std::string SkillDescription(int skillId) {
		auto text = Phrase("SkillBehavior_" + std::to_string(skillId) + "_descriptionUI");
		const std::pair<const char*, const char*> replacements[] = {
			{"%(DamageCombo)", "Damage Combo: "}, {"%(AltCombo)", "\nSkeleton Combo: "},
			{"%(Description)", "\n"}, {"%(ChargeUp)", "\nCharge-up: "}
		};
		for (const auto& [from, to] : replacements) {
			for (auto pos = text.find(from); pos != std::string::npos; pos = text.find(from, pos)) text.replace(pos, std::strlen(from), to);
		}
		return text;
	}

	void AddSkill(Stats& stats, CppSQLite3Query& row) {
		const int imagination = row.getIntField("imBonusUI", 0);
		const int life = row.getIntField("lifeBonusUI", 0);
		const int armor = row.getIntField("armorBonusUI", 0);
		stats.imagination += imagination;
		stats.life += life;
		stats.armor += armor;
		// Stat bonus skills only restate the numbers above ("+2 AP"), so list just the other skills
		if (imagination || life || armor) return;
		const int skillId = row.getIntField("skillID", 0);
		const auto description = SkillDescription(skillId);
		if (!description.empty()) stats.skills.push_back({ {"id", skillId}, {"icon", row.getIntField("skillIcon", 0)}, {"description", description} });
	}

	nlohmann::json StatsJson(const Stats& stats) {
		return { {"imagination", stats.imagination}, {"life", stats.life}, {"armor", stats.armor}, {"skills", stats.skills} };
	}

	nlohmann::json SetBonus(int skillSetId) {
		Stats stats;
		auto stmt = CDClientDatabase::CreatePreppedStmt(
			"SELECT sb.skillID, sb.skillIcon, sb.imBonusUI, sb.lifeBonusUI, sb.armorBonusUI FROM ItemSetSkills iss "
			"JOIN SkillBehavior sb ON sb.skillID = iss.SkillID WHERE iss.SkillSetID = ?;");
		stmt.bind(1, skillSetId);
		auto row = stmt.execQuery();
		while (!row.eof()) {
			AddSkill(stats, row);
			row.nextRow();
		}
		return StatsJson(stats);
	}

	void LoadItemSets() {
		if (g_ItemSetsLoaded) return;
		g_ItemSetsLoaded = true;
		auto rows = CDClientDatabase::ExecuteQuery("SELECT setID, itemIDs FROM ItemSets;");
		while (!rows.eof()) {
			const int setId = rows.getIntField("setID");
			for (const auto& part : GeneralUtils::SplitString(rows.getStringField("itemIDs", ""), ',')) {
				std::string trimmed = part;
				trimmed.erase(0, trimmed.find_first_not_of(' '));
				if (const auto lot = GeneralUtils::TryParse<LOT>(trimmed)) g_ItemSetsByLot[*lot].push_back(setId);
			}
			rows.nextRow();
		}
	}

	nlohmann::json ItemSet(LOT lot) {
		LoadItemSets();
		const auto it = g_ItemSetsByLot.find(lot);
		if (it == g_ItemSetsByLot.end()) return nullptr;

		auto stmt = CDClientDatabase::CreatePreppedStmt(
			"SELECT setID, kitRank, kitImage, skillSetWith2, skillSetWith3, skillSetWith4, skillSetWith5, skillSetWith6 FROM ItemSets WHERE setID = ?;");
		stmt.bind(1, it->second.front());
		auto row = stmt.execQuery();
		if (row.eof()) return nullptr;

		const int setId = row.getIntField("setID");
		nlohmann::json set{
			{"id", setId},
			{"name", Phrase("ItemSets_" + std::to_string(setId) + "_kitName")},
			{"rank", row.getIntField("kitRank", 0)},
			{"icon", row.getIntField("kitImage", 0)},
			{"bonuses", nlohmann::json::array()}
		};
		const char* columns[] = { "skillSetWith2", "skillSetWith3", "skillSetWith4", "skillSetWith5", "skillSetWith6" };
		for (int i = 0; i < 5; i++) {
			const int skillSet = row.getIntField(columns[i], 0);
			if (skillSet > 0) set["bonuses"].push_back({ {"pieces", i + 2}, {"stats", SetBonus(skillSet)} });
		}
		return set;
	}
}

namespace {
	struct ObjectNames {
		std::string name;
		std::string displayName;
	};

	// Every object's name and displayName, read once: the CDClient has no index on Objects.id, so looking a LOT up
	// scans the whole table, and pages name thousands of items at once
	std::unordered_map<LOT, ObjectNames> ReadObjectNames() {
		std::unordered_map<LOT, ObjectNames> table;
		try {
			auto row = CDClientDatabase::ExecuteQuery("SELECT id, name, displayName FROM Objects;");
			for (; !row.eof(); row.nextRow()) {
				// The first row wins, as the LIMIT 1 lookups this replaces did
				table.try_emplace(static_cast<LOT>(row.getIntField("id")), ObjectNames{ row.getStringField("name", ""), row.getStringField("displayName", "") });
			}
		} catch (const std::exception& ex) {
			LOG("Failed to read object names: %s", ex.what());
		}
		return table;
	}

	// Read at startup (ClientAssets::Preload), so worker threads only read it and never query the CDClient
	const std::unordered_map<LOT, ObjectNames>& ObjectNameTable() {
		static const auto table = ReadObjectNames();
		return table;
	}

	// Where TextureAsPng keeps a texture it converted
	std::filesystem::path PngCachePath(const std::string& normalized, uint32_t maxSize, bool opaque) {
		std::string cacheName = normalized + "_" + std::to_string(maxSize) + (opaque ? "_opaque" : "") + ".png";
		std::replace(cacheName.begin(), cacheName.end(), '/', '_');
		std::replace(cacheName.begin(), cacheName.end(), ' ', '_');
		return std::filesystem::path("dDashboardServer") / "icon_cache" / cacheName;
	}

	// ImageMagick conversions by cache file name: whether it worked (a failed one isn't tried again)
	OnceCache<std::string, bool> g_Conversions;
}

namespace ClientAssets {
	std::optional<std::string> ObjectName(LOT lot) {
		const auto& table = ObjectNameTable();
		const auto it = table.find(lot);
		if (it == table.end()) return std::nullopt;
		return it->second.name;
	}

	std::string ItemName(LOT lot) {
		const auto& localized = Locale::GetPhrase("Objects_" + std::to_string(lot) + "_name");
		if (!localized.empty()) return localized;
		const auto& table = ObjectNameTable();
		const auto it = table.find(lot);
		if (it == table.end()) return "LOT " + std::to_string(lot);
		return it->second.displayName.empty() ? it->second.name : it->second.displayName;
	}

	nlohmann::json ItemInfo(LOT lot) {
		if (const auto cached = g_ItemCache.find(lot); cached != g_ItemCache.end()) return cached->second;

		nlohmann::json info{ {"lot", lot}, {"name", ItemName(lot)} };

		auto description = Phrase("Objects_" + std::to_string(lot) + "_description");
		if (description.empty()) {
			auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT description FROM Objects WHERE id = ? LIMIT 1;");
			stmt.bind(1, static_cast<int>(lot));
			auto row = stmt.execQuery();
			if (!row.eof()) description = row.getStringField("description", "");
		}
		info["description"] = description;

		auto rarityStmt = CDClientDatabase::CreatePreppedStmt(
			"SELECT ic.rarity FROM ComponentsRegistry cr JOIN ItemComponent ic ON ic.id = cr.component_id WHERE cr.component_type = 11 AND cr.id = ? LIMIT 1;");
		rarityStmt.bind(1, static_cast<int>(lot));
		auto rarity = rarityStmt.execQuery();
		info["rarity"] = rarity.eof() ? 0 : rarity.getIntField("rarity", 0);

		Stats stats;
		auto skillStmt = CDClientDatabase::CreatePreppedStmt(
			"SELECT sb.skillID, sb.skillIcon, sb.imBonusUI, sb.lifeBonusUI, sb.armorBonusUI FROM ObjectSkills os "
			"JOIN SkillBehavior sb ON sb.skillID = os.skillID WHERE os.objectTemplate = ?;");
		skillStmt.bind(1, static_cast<int>(lot));
		auto skills = skillStmt.execQuery();
		while (!skills.eof()) {
			AddSkill(stats, skills);
			skills.nextRow();
		}
		info["stats"] = StatsJson(stats);
		info["set"] = ItemSet(lot);

		g_ItemCache[lot] = info;
		return info;
	}

	std::string IconPathForLot(LOT lot) {
		auto stmt = CDClientDatabase::CreatePreppedStmt(
			"SELECT rc.icon_asset, rc.IconID FROM ComponentsRegistry cr JOIN RenderComponent rc ON cr.component_id = rc.id "
			"WHERE cr.component_type = 2 AND cr.id = ? LIMIT 1;");
		stmt.bind(1, static_cast<int>(lot));
		auto result = stmt.execQuery();
		if (result.eof()) return "";
		std::string iconPath = result.getStringField("icon_asset", "");
		const int iconId = result.getIntField("IconID", 0);
		if (iconPath.empty() && iconId > 0) iconPath = IconPathForId(iconId);
		return iconPath;
	}

	std::string IconPathForId(int iconId) {
		auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT IconPath FROM Icons WHERE IconID = ? LIMIT 1;");
		stmt.bind(1, iconId);
		auto result = stmt.execQuery();
		return result.eof() ? "" : result.getStringField("IconPath", "");
	}

	bool IsSafeAssetPath(const std::string& path) {
		if (path.empty() || path.find("..") != std::string::npos || path.starts_with('/')) return false;
		return std::ranges::all_of(path, [](char c) {
			return std::isalnum(static_cast<unsigned char>(c)) || c == '/' || c == '_' || c == '-' || c == '.' || c == ' ';
		});
	}

	std::optional<std::string> TextureAsPng(const std::string& assetPath, uint32_t maxSize, bool opaque) {
		const auto res = ClientRes();
		const auto normalized = NormalizeAssetPath(assetPath);
		if (res.empty() || !IsSafeAssetPath(normalized)) return std::nullopt;

		const auto source = ResolveRes(normalized);
		std::error_code ec;
		if (!std::filesystem::is_regular_file(source, ec)) return std::nullopt;
		if (normalized.ends_with(".png")) return ReadFile(source);
		if (!normalized.ends_with(".dds")) return std::nullopt;

		const auto target = PngCachePath(normalized, maxSize, opaque);
		const auto cacheDir = target.parent_path();
		const auto cacheName = target.filename().string();
		// ImageMagick runs without a shell (an argument vector); odd characters are still refused as a precaution
		if (!std::filesystem::exists(target, ec) && !Process::HasShellMetacharacters(source.string())) {
			// Once per file, however many threads ask at once; written under another name and renamed, so a reader
			// never sees half a file
			const auto converted = g_Conversions.Get(cacheName, [&] {
				if (std::filesystem::exists(target, ec)) return true;
				std::filesystem::create_directories(cacheDir, ec);
				const auto partial = cacheDir / (cacheName + ".part.png");
				const std::string size = std::to_string(maxSize) + "x" + std::to_string(maxSize) + ">";
				std::vector<std::string> arguments{ "magick", source.string() };
				if (opaque) { arguments.push_back("-alpha"); arguments.push_back("off"); }
				arguments.insert(arguments.end(), { "-resize", size, partial.string() });
				std::error_code renameError;
				if (Process::Run(arguments) != 0 || (std::filesystem::rename(partial, target, renameError), renameError)) {
					LOG_DEBUG("Texture conversion failed for %s (is ImageMagick installed?)", normalized.c_str());
					return false;
				}
				return true;
			});
			if (!converted) return std::nullopt;
		}
		return ReadFile(target);
	}

	bool TextureAsPngReady(const std::string& assetPath, uint32_t maxSize, bool opaque) {
		const auto normalized = NormalizeAssetPath(assetPath);
		std::error_code ec;
		return normalized.ends_with(".png") || std::filesystem::exists(PngCachePath(normalized, maxSize, opaque), ec);
	}

	std::optional<std::string> ReadResFile(const std::string& relativePath) {
		const auto res = ClientRes();
		if (res.empty() || !IsSafeAssetPath(NormalizeAssetPath(relativePath))) return std::nullopt;
		return ReadFile(ResolveRes(NormalizeAssetPath(relativePath)));
	}

	std::optional<std::filesystem::path> ResolveResFile(const std::string& relativePath) {
		const auto normalized = NormalizeAssetPath(relativePath);
		if (ClientRes().empty() || !IsSafeAssetPath(normalized)) return std::nullopt;
		auto path = ResolveRes(normalized);
		std::error_code ec;
		if (!std::filesystem::is_regular_file(path, ec)) return std::nullopt;
		return path;
	}

	void Preload() {
		ClientRes();
		ObjectNameTable();
	}

	std::filesystem::path ResFolder() {
		return ClientRes();
	}

	std::optional<std::filesystem::path> ResolveResFile(const std::string& relativePath, const std::filesystem::path& res) {
		const auto normalized = NormalizeAssetPath(relativePath);
		if (res.empty() || !IsSafeAssetPath(normalized)) return std::nullopt;
		auto path = ResolveResIn(res, normalized);
		std::error_code ec;
		if (!std::filesystem::is_regular_file(path, ec)) return std::nullopt;
		return path;
	}

	std::optional<std::string> FindResFile(const std::string& folder, const std::string& fileName) {
		const auto normalized = NormalizeAssetPath(folder);
		if (ClientRes().empty() || !IsSafeAssetPath(normalized)) return std::nullopt;
		const auto root = ResolveRes(normalized);
		auto wanted = fileName;
		std::transform(wanted.begin(), wanted.end(), wanted.begin(), ::tolower);
		std::error_code ec;
		for (auto it = std::filesystem::recursive_directory_iterator(root, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
			auto name = it->path().filename().string();
			std::transform(name.begin(), name.end(), name.begin(), ::tolower);
			if (name != wanted || !it->is_regular_file(ec)) continue;
			auto relative = std::filesystem::relative(it->path(), ClientRes(), ec).generic_string();
			std::transform(relative.begin(), relative.end(), relative.begin(), ::tolower);
			return relative;
		}
		return std::nullopt;
	}

	std::optional<nlohmann::json> ListDirectory(const std::string& relativePath) {
		const auto res = ClientRes();
		const auto normalized = NormalizeAssetPath(relativePath);
		if (res.empty() || (!normalized.empty() && !IsSafeAssetPath(normalized))) return std::nullopt;

		std::error_code ec;
		const auto dir = normalized.empty() ? res : ResolveRes(normalized);
		if (!std::filesystem::is_directory(dir, ec)) return std::nullopt;

		nlohmann::json entries = nlohmann::json::array();
		for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
			if (entries.size() >= MAX_LISTING_ENTRIES) break;
			const auto name = entry.path().filename().string();
			const bool isDir = entry.is_directory(ec);
			entries.push_back({
				{"name", name},
				{"path", normalized.empty() ? name : normalized + "/" + name},
				{"dir", isDir},
				{"size", isDir ? 0 : static_cast<int64_t>(entry.file_size(ec))}
			});
		}
		std::sort(entries.begin(), entries.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
			if (a["dir"] != b["dir"]) return a["dir"].get<bool>();
			return a["name"].get<std::string>() < b["name"].get<std::string>();
		});
		return nlohmann::json{ {"path", normalized}, {"entries", entries}, {"truncated", entries.size() >= MAX_LISTING_ENTRIES} };
	}
}

namespace {
	void ReplyPng(HTTPReply& reply, const std::optional<std::string>& png) {
		if (!png) {
			reply.status = eHTTPStatusCode::NOT_FOUND;
			reply.message = "";
			return;
		}
		reply.status = eHTTPStatusCode::OK;
		reply.message = *png;
		reply.contentType = eContentType::IMAGE_PNG;
		reply.headers.push_back("Cache-Control: private, max-age=86400");
	}
}

void RegisterClientAssetRoutes() {
	Route(eHTTPMethod::GET, "/api/bricks/materials.js", 0,
		"The brick colours (MatID -> [r, g, b, a]) from Materials.xml in the client's res/brickdb.zip, as a script setting "
		"window.LDD_MATERIALS for the 3D viewers, and window.LDD_GLITTER: the colours the UGC server makes glitter with its settings "
		"(shader_glitter on: glitter_material_types and glitter_colors) and the glitter's settings (flecks, and sparkles when shader_glitter_sparkle is on). "
		"The colours are read once; an empty table when the client's brick database can't be read",
		[](HTTPReply& reply, const HTTPContext&) {
			// Read on first use (thread-safe static init); the client only changes with a restart
			struct Colours {
				std::string script;
				std::map<uint32_t, std::string> types; // MatID -> MaterialType
			};
			static const Colours colours = [] {
				Colours out;
				nlohmann::json table = nlohmann::json::object();
				const auto zip = ClientAssets::ReadResFile("brickdb.zip");
				const auto xml = zip ? UgcBricks::ReadZipEntry(*zip, "Materials.xml") : std::nullopt;
				if (xml) {
					for (const auto& [id, m] : UgcBricks::ParseMaterials(*xml)) {
						table[std::to_string(id)] = { m.r, m.g, m.b, m.a };
						out.types[id] = m.type;
					}
				} else {
					LOG("Couldn't read Materials.xml from the client's brickdb.zip; the 3D viewers' bricks will be grey");
				}
				out.script = "window.LDD_MATERIALS = " + table.dump() + ";\n";
				return out;
			}();
			// The glitter colours as the UGC server picks them (UgcServer.cpp ReadSettings), from its current settings
			const auto list = [](const std::string& name) {
				std::set<std::string> items;
				std::stringstream stream(UgcRoutes::Setting(name).value_or(""));
				std::string item;
				while (std::getline(stream, item, ',')) {
					std::erase_if(item, [](unsigned char c) { return std::isspace(c); });
					if (!item.empty() && item != "none") items.insert(item);
				}
				return items;
			};
			nlohmann::json glitter = nlohmann::json::array();
			if (GeneralUtils::TryParse<uint32_t>(UgcRoutes::Setting("shader_glitter").value_or("")).value_or(0) != 0) {
				const auto types = list("glitter_material_types");
				std::set<uint32_t> ids;
				for (const auto& [id, type] : colours.types) if (types.contains(type)) ids.insert(id);
				for (const auto& id : list("glitter_colors")) if (const auto value = GeneralUtils::TryParse<uint32_t>(id)) ids.insert(*value);
				for (const auto id : ids) glitter.push_back(id);
			}
			const nlohmann::json settings{ { "colors", glitter },
				{ "tile", GeneralUtils::TryParse<float>(UgcRoutes::Setting("glitter_size").value_or("")).value_or(1.6f) },
				{ "flecks", GeneralUtils::TryParse<uint32_t>(UgcRoutes::Setting("glitter_density").value_or("")).value_or(80) },
				{ "fleckSize", GeneralUtils::TryParse<float>(UgcRoutes::Setting("glitter_fleck_size").value_or("")).value_or(0.05f) },
				{ "fleckOpacity", GeneralUtils::TryParse<float>(UgcRoutes::Setting("glitter_fleck_opacity").value_or("")).value_or(80.0f) },
				{ "speed", GeneralUtils::TryParse<float>(UgcRoutes::Setting("glitter_speed").value_or("")).value_or(1.0f) },
				{ "sparkles", GeneralUtils::TryParse<uint32_t>(UgcRoutes::Setting("shader_glitter_sparkle").value_or("")).value_or(79) != 0 },
				{ "sparkleSize", GeneralUtils::TryParse<float>(UgcRoutes::Setting("glitter_sparkle_size").value_or("")).value_or(0.1f) },
				{ "sparkleAmount", GeneralUtils::TryParse<float>(UgcRoutes::Setting("glitter_sparkle_amount").value_or("")).value_or(5.0f) },
				{ "sparkleTint", GeneralUtils::TryParse<float>(UgcRoutes::Setting("glitter_sparkle_tint").value_or("")).value_or(30.0f) },
				{ "sparkleBrightness", GeneralUtils::TryParse<float>(UgcRoutes::Setting("glitter_sparkle_brightness").value_or("")).value_or(100.0f) } };
			reply.status = eHTTPStatusCode::OK;
			reply.message = colours.script + "window.LDD_GLITTER = " + settings.dump() + ";\n";
			reply.contentType = eContentType::TEXT_JAVASCRIPT;
			// Not kept: the glitter settings change from the settings page, and the preview should follow
			reply.headers.push_back("Cache-Control: private, no-cache");
		});

	Route(eHTTPMethod::GET, "/api/icon/:lot", 0, "PNG icon for an item LOT (requires client_location and ImageMagick)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto lot = PathId<LOT>(context.path, 2);
			std::string path = lot ? ClientAssets::IconPathForLot(*lot) : "";
			auto png = path.empty() ? std::nullopt : ClientAssets::TextureAsPng(path, 64);
			// Fall back to the game's "unknown item" icon
			if (lot && !png) png = ClientAssets::TextureAsPng("textures/ui/inventory/unknown.dds", 64);
			ReplyPng(reply, png);
		});

	Route(eHTTPMethod::GET, "/api/icon_id/:id", 0, "PNG icon by icon ID (skills, item sets)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto iconId = PathId<int>(context.path, 2);
			const std::string path = iconId ? ClientAssets::IconPathForId(*iconId) : "";
			ReplyPng(reply, path.empty() ? std::nullopt : ClientAssets::TextureAsPng(path, 64));
		});

	ReadRoute(eHTTPMethod::POST, "/api/items/info", 0, "Item details for tooltips. Body: {lots: [..]} (max 500)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body || !body->contains("lots") || !(*body)["lots"].is_array()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "lots must be an array");
			nlohmann::json items = nlohmann::json::object();
			uint32_t count = 0;
			for (const auto& lot : (*body)["lots"]) {
				if (!lot.is_number_integer() || ++count > MAX_ITEMS_PER_REQUEST) continue;
				const auto value = lot.get<LOT>();
				items[std::to_string(value)] = ClientAssets::ItemInfo(value);
			}
			JsonReply(reply, eHTTPStatusCode::OK, items);
		});

	Route(eHTTPMethod::GET, "/api/client/list", Perm("client_files"), "List a folder of the game client's res directory. Query: ?path=",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto listing = ClientAssets::ListDirectory(QueryValue(context.queryString, "path"));
			if (!listing) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Folder not found (is client_location set?)");
			JsonReply(reply, eHTTPStatusCode::OK, *listing);
		});

	Route(eHTTPMethod::GET, "/api/client/texture", Perm("client_files"), "A client DDS/PNG texture as PNG. Query: ?path=&size= (max 1024)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const uint32_t size = std::clamp<uint32_t>(GeneralUtils::TryParse<uint32_t>(QueryValue(context.queryString, "size")).value_or(512), 16, 1024);
			ReplyPng(reply, ClientAssets::TextureAsPng(QueryValue(context.queryString, "path"), size));
		});

	Route(eHTTPMethod::GET, "/api/client/file", Perm("client_files"), "Download a raw client file. Query: ?path=",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto normalized = NormalizeAssetPath(QueryValue(context.queryString, "path"));
			const auto res = ClientRes();
			std::error_code ec;
			if (res.empty() || !ClientAssets::IsSafeAssetPath(normalized) || !std::filesystem::is_regular_file(ResolveRes(normalized), ec)) {
				return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "File not found");
			}
			const auto data = ReadFile(ResolveRes(normalized));
			if (!data) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "File not found");
			reply.status = eHTTPStatusCode::OK;
			reply.message = *data;
			reply.contentType = eContentType::APPLICATION_OCTET_STREAM;
			const auto name = std::filesystem::path(normalized).filename().string();
			reply.headers.push_back("Content-Disposition: attachment; filename=\"" + name + "\"");
		});
}
