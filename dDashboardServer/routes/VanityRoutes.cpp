#include "VanityRoutes.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>

#include "RouteUtils.h"
#include "VanityJson.h"
#include "PlayerActions.h"
#include "PlayerAction.h"
#include "DashboardRoutes.h"
#include "ClientAssets.h"
#include "BinaryPathFinder.h"
#include "Game.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "Database.h"
#include "VanityEvents.h"
#include "EventsCalendar.h"
#include "eHTTPMethod.h"

#include <ctime>

using namespace RouteUtils;

namespace {
	namespace fs = std::filesystem;
	const std::string ROOT = "root.xml";

	// The markdown files the game reads, and where players see them
	const std::vector<std::pair<std::string, std::string>> TEXTS{
		{ "TESTAMENT.md", "The plaque by the Nimbus Station launch pad (zone 1200)" },
		{ "CREDITS.md", "Shown by the /credits command" },
		{ "INFO.md", "Shown by the /info command; __VERSION__, __SOURCE__, __LICENSE__ and __TIMESTAMP__ are filled in" },
	};

	fs::path Folder() { return BinaryPathFinder::GetBinaryDir() / "vanity"; }

	bool ValidXmlName(const std::string& name) {
		static const std::regex pattern("^[A-Za-z0-9_-]{1,60}\\.xml$");
		return std::regex_match(name, pattern);
	}

	std::optional<std::string> ReadText(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return std::nullopt;
		return std::string(std::istreambuf_iterator<char>(in), {});
	}

	// Keep the previous version next to it, then write
	bool WriteText(const fs::path& path, const std::string& text) {
		std::error_code ec;
		if (fs::exists(path, ec)) fs::copy_file(path, path.string() + ".bak", fs::copy_options::overwrite_existing, ec);
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out) return false;
		out << text;
		return static_cast<bool>(out);
	}

	std::optional<VanityXml::Document> ReadDocument(const std::string& name, std::string& error) {
		const auto text = ReadText(Folder() / name);
		if (!text) { error = name + " not found"; return std::nullopt; }
		return VanityXml::Read(*text, error);
	}

	nlohmann::json ItemNames(const std::set<int32_t>& lots) {
		nlohmann::json names = nlohmann::json::object();
		for (const auto lot : lots) {
			const auto name = ClientAssets::ItemName(lot);
			if (!name.empty()) names[std::to_string(lot)] = name;
		}
		return names;
	}

	struct FileNode {
		bool exists{};
		bool loaded{};            // reached from root.xml through switched-on includes, as the world loads them
		std::string error;
		size_t npcs{};
		std::vector<VanityXml::FileEntry> includes;
		std::vector<nlohmann::json> includedBy; // [{file, enabled}]
	};

	/**
	 * Every vanity file and how they include each other. The world starts at root.xml and follows each switched-on
	 * <file> entry, loading every file once (VanityUtilities ParseXml); any file may include others.
	 */
	std::map<std::string, FileNode> IncludeTree() {
		std::map<std::string, FileNode> nodes;
		std::error_code ec;
		const auto read = [&](const std::string& name) -> FileNode& {
			auto [it, inserted] = nodes.try_emplace(name);
			if (inserted) {
				std::string error;
				it->second.exists = fs::exists(Folder() / name, ec);
				if (const auto doc = it->second.exists ? ReadDocument(name, error) : std::nullopt) {
					it->second.npcs = doc->objects.size();
					it->second.includes = doc->files;
				} else if (it->second.exists) {
					it->second.error = error;
				}
			}
			return it->second;
		};
		read(ROOT);
		for (const auto& entry : fs::directory_iterator(Folder(), ec)) {
			const auto name = entry.path().filename().string();
			if (ValidXmlName(name)) read(name);
		}
		// Who includes whom (including files that don't exist yet)
		std::vector<std::string> names;
		for (const auto& [name, node] : nodes) names.push_back(name);
		for (const auto& name : names) {
			for (const auto& include : nodes[name].includes) read(include.name).includedBy.push_back({ {"file", name}, {"enabled", include.enabled} });
		}
		// What the world loads: switched-on includes reachable from root.xml, each once
		std::vector<std::string> queue{ ROOT };
		nodes[ROOT].loaded = nodes[ROOT].exists;
		while (!queue.empty()) {
			const auto current = queue.back();
			queue.pop_back();
			for (const auto& include : nodes[current].includes) {
				auto& child = nodes[include.name];
				if (!include.enabled || child.loaded || !child.exists) continue;
				child.loaded = true;
				queue.push_back(include.name);
			}
		}
		return nodes;
	}

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	nlohmann::json IncludesJson(const std::vector<VanityXml::FileEntry>& includes) {
		nlohmann::json out = nlohmann::json::array();
		for (const auto& include : includes) out.push_back({ {"name", include.name}, {"enabled", include.enabled} });
		return out;
	}

	// Read {files: [{name, enabled}]} from the page; nullopt with an error if a name isn't a vanity file name
	std::optional<std::vector<VanityXml::FileEntry>> IncludesFromJson(const nlohmann::json& json, const std::string& self, std::string& error) {
		std::vector<VanityXml::FileEntry> includes;
		std::set<std::string> seen;
		for (const auto& item : json) {
			const std::string name = item.is_object() ? item.value("name", "") : "";
			if (!ValidXmlName(name) || name == ROOT || name == self) { error = "\"" + name + "\" can't be included here"; return std::nullopt; }
			if (!seen.insert(name).second) continue;
			includes.push_back({ name, item.value("enabled", false) });
		}
		return includes;
	}
}

void RegisterVanityRoutes() {
	Route(eHTTPMethod::GET, "/api/vanity", Perm("vanity_manage"),
		"Every vanity file: its NPCs, which files it includes and is included by, whether the worlds load it (reached from root.xml "
		"through switched-on includes), whether they load it now (with the vanity events that are on) and the events that use it "
		"(events: [{id, name, on, use: overlay|on|off}]). Also the plaque texts and zone names",
		[](HTTPReply& reply, const HTTPContext&) {
			// The scheduled events whose vanity parts use each file: as their overlay, or switching it on or off
			const auto now = Now();
			const auto uses = EventsCalendar::VanityUses(now);
			// What the worlds load now, with the events that are on (the same call the worlds make)
			std::map<std::string, VanityEvents::FileLoad> loadedNow;
			for (auto& file : VanityEvents::LoadWorld(Folder(), ROOT, EventsCalendar::VanityChangesOn(now)).files) loadedNow[file.name] = std::move(file);
			nlohmann::json files = nlohmann::json::array();
			for (const auto& [name, node] : IncludeTree()) {
				const auto now_ = loadedNow.find(name);
				nlohmann::json entry{ {"name", name}, {"root", name == ROOT}, {"exists", node.exists}, {"loaded", node.loaded}, {"npcs", node.npcs},
					{"includes", IncludesJson(node.includes)}, {"includedBy", node.includedBy},
					{"events", uses["files"].contains(name) ? uses["files"][name] : nlohmann::json::array()},
					{"loadedNow", now_ != loadedNow.end() && now_->second.loaded}, {"switchedNow", now_ == loadedNow.end() ? "" : now_->second.switchedBy} };
				// For the simple on/off switch: the entry in root.xml, if there is one
				for (const auto& by : node.includedBy) if (by["file"] == ROOT) entry["enabled"] = by["enabled"];
				if (!entry.contains("enabled")) entry["enabled"] = false;
				entry["listed"] = std::any_of(node.includedBy.begin(), node.includedBy.end(), [](const auto& by) { return by["file"] == ROOT; });
				if (!node.error.empty()) entry["error"] = node.error;
				files.push_back(entry);
			}
			nlohmann::json texts = nlohmann::json::array();
			for (const auto& [name, where] : TEXTS) texts.push_back({ {"name", name}, {"where", where} });
			nlohmann::json zones = nlohmann::json::array();
			for (const auto& [id, name] : ZoneNames().items()) {
				if (const auto zone = GeneralUtils::TryParse<uint32_t>(id); zone && *zone > 0) zones.push_back({ {"id", *zone}, {"name", name} });
			}
			std::sort(zones.begin(), zones.end(), [](const auto& a, const auto& b) { return a["id"].template get<uint32_t>() < b["id"].template get<uint32_t>(); });
			std::string rootError;
			std::error_code ec;
			if (!fs::exists(Folder() / ROOT, ec)) rootError = "root.xml not found in " + Folder().string();
			JsonSuccess(reply, { {"folder", Folder().string()}, {"rootError", rootError}, {"files", files}, {"texts", texts}, {"zones", zones}, {"events", uses["events"]},
				{"disabled", Game::config->GetValue("disable_vanity") == "1"} });
		});

	Route(eHTTPMethod::GET, "/api/vanity/files/:name", Perm("vanity_manage"), "The NPCs in one vanity file, with item names and the vanity events that change them (npcEvents: {name: [{id, name, on, use: replace|remove}]})",
		[](HTTPReply& reply, const HTTPContext& context) {
			const std::string name(PathSegment(context.originalPath, 3));
			if (!ValidXmlName(name)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Not a vanity file name");
			std::string error;
			const auto doc = ReadDocument(name, error);
			if (!doc) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, error);
			nlohmann::json objects = nlohmann::json::array();
			std::set<int32_t> lots;
			std::set<std::string> names;
			for (const auto& object : doc->objects) {
				objects.push_back(VanityJson::ToJson(object));
				lots.insert(object.lot);
				lots.insert(object.equipment.begin(), object.equipment.end());
				if (!object.name.empty()) names.insert(object.name);
			}
			// The events that change this file's NPCs: an overlay with the same name replaces them, a removal takes them out
			nlohmann::json npcEvents = nlohmann::json::object();
			const auto uses = EventsCalendar::VanityUses(Now());
			for (const auto& [npc, events] : uses["removes"].items()) {
				if (names.contains(npc)) for (const auto& event : events) npcEvents[npc].push_back(event);
			}
			for (const auto& [file, events] : uses["files"].items()) {
				if (file == name) continue;
				std::set<std::string> inOverlay;
				for (const auto& object : VanityEvents::LoadFiles(Folder(), file).objects) if (names.contains(object.name)) inOverlay.insert(object.name);
				for (const auto& event : events) {
					if (event["use"] != "overlay") continue;
					auto replace = event;
					replace["use"] = "replace";
					replace["file"] = file;
					for (const auto& npc : inOverlay) npcEvents[npc].push_back(replace);
				}
			}
			JsonSuccess(reply, { {"name", name}, {"root", name == ROOT}, {"objects", objects}, {"files", IncludesJson(doc->files)}, {"itemNames", ItemNames(lots)},
				{"npcEvents", npcEvents} });
		});

	Route(eHTTPMethod::POST, "/api/vanity/files/:name", Perm("vanity_manage"),
		"Save a vanity file. Body: {objects: [{name, lot, equipment, phrases, config, locations}], files (optional): [{name, enabled}] (the files it includes)}. The old file is kept as .bak",
		[](HTTPReply& reply, const HTTPContext& context) {
			const std::string name(PathSegment(context.originalPath, 3));
			const auto body = ParseBody(context);
			if (!ValidXmlName(name)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Not a vanity file name");
			if (!body || !body->contains("objects") || !(*body)["objects"].is_array()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Send {objects: [...]}");
			std::string error;
			auto doc = ReadDocument(name, error);
			if (!doc) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, error);
			// The file's own includes, when sent (kept as they are otherwise)
			if (body->contains("files")) {
				if (!(*body)["files"].is_array()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "files is a list");
				auto includes = IncludesFromJson((*body)["files"], name, error);
				if (!includes) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
				doc->files = std::move(*includes);
			}
			doc->objects.clear();
			for (const auto& item : (*body)["objects"]) {
				auto object = VanityJson::FromJson(item, error);
				if (!object) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
				doc->objects.push_back(std::move(*object));
			}
			if (!WriteText(Folder() / name, VanityXml::Write(*doc))) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Couldn't write " + name);
			Audit(context, "edit_vanity", name + ": " + std::to_string(doc->objects.size()) + " NPC(s)");
			JsonSuccess(reply, { {"message", "Saved " + name + ". Respawn the vanity NPCs to see it in game."} });
		});

	Route(eHTTPMethod::POST, "/api/vanity/files", Perm("vanity_manage"), "Create an empty vanity file, included (switched off) by root.xml or {parent}. Body: {name, parent}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			std::string name = body ? body->value("name", "") : "";
			if (!name.ends_with(".xml")) name += ".xml";
			if (!ValidXmlName(name) || name == ROOT) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Use letters, digits, - and _ for the name");
			std::error_code ec;
			if (fs::exists(Folder() / name, ec)) return JsonError(reply, eHTTPStatusCode::CONFLICT, name + " already exists");
			// Included (switched off) by root.xml, or by another file when {parent} says so
			const std::string parentName = body->value("parent", ROOT);
			if (!ValidXmlName(parentName)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Not a vanity file name: " + parentName);
			std::string error;
			auto parent = ReadDocument(parentName, error);
			if (!parent) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, parentName + ": " + error);
			if (!WriteText(Folder() / name, VanityXml::Write({}))) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Couldn't write " + name);
			parent->files.push_back({ name, false });
			WriteText(Folder() / parentName, VanityXml::Write(*parent));
			Audit(context, "edit_vanity", "Created " + name + " (included by " + parentName + ")");
			JsonSuccess(reply, { {"name", name}, {"message", "Created " + name + ", switched off in " + parentName} });
		});

	Route(eHTTPMethod::POST, "/api/vanity/root", Perm("vanity_manage"),
		"Switch an include on or off: the <file> entry for {name} in root.xml, or in {parent}. Adds the entry if it's missing. Body: {name, enabled, parent}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			const std::string name = body ? body->value("name", "") : "";
			const std::string parentName = body ? body->value("parent", ROOT) : ROOT;
			if (!ValidXmlName(name) || name == ROOT || name == parentName) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Not a file that can be included");
			if (!ValidXmlName(parentName)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Not a vanity file name: " + parentName);
			std::string error;
			auto parent = ReadDocument(parentName, error);
			if (!parent) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, parentName + ": " + error);
			const bool enabled = body->value("enabled", false);
			const auto it = std::find_if(parent->files.begin(), parent->files.end(), [&](const auto& f) { return f.name == name; });
			if (it == parent->files.end()) parent->files.push_back({ name, enabled });
			else it->enabled = enabled;
			if (!WriteText(Folder() / parentName, VanityXml::Write(*parent))) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Couldn't write " + parentName);
			Audit(context, "edit_vanity", name + (enabled ? " switched on" : " switched off") + " in " + parentName);
			JsonSuccess(reply, { {"message", name + (enabled ? " is on" : " is off") + " in " + parentName + ". Respawn the vanity NPCs to see it in game."} });
		});

	Route(eHTTPMethod::POST, "/api/vanity/files/:name/delete", Perm("vanity_manage"), "Delete a vanity file and take it out of every file that includes it (a .bak copy is kept)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const std::string name(PathSegment(context.originalPath, 3));
			if (!ValidXmlName(name) || name == ROOT) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Not a vanity file name");
			// Take it out of every file that includes it
			for (const auto& [file, node] : IncludeTree()) {
				if (std::none_of(node.includes.begin(), node.includes.end(), [&](const auto& f) { return f.name == name; })) continue;
				std::string error;
				if (auto doc = ReadDocument(file, error)) {
					std::erase_if(doc->files, [&](const auto& f) { return f.name == name; });
					WriteText(Folder() / file, VanityXml::Write(*doc));
				}
			}
			std::error_code ec;
			fs::rename(Folder() / name, Folder() / (name + ".bak"), ec);
			Audit(context, "edit_vanity", "Deleted " + name);
			JsonSuccess(reply, { {"message", "Deleted " + name + " (kept as " + name + ".bak)"} });
		});

	Route(eHTTPMethod::GET, "/api/vanity/text/:name", Perm("vanity_manage"), "One of the plaque texts (TESTAMENT.md, CREDITS.md, INFO.md)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const std::string name(PathSegment(context.originalPath, 3));
			if (std::none_of(TEXTS.begin(), TEXTS.end(), [&](const auto& t) { return t.first == name; })) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Unknown text");
			JsonSuccess(reply, { {"name", name}, {"text", ReadText(Folder() / name).value_or("")} });
		});

	Route(eHTTPMethod::POST, "/api/vanity/text/:name", Perm("vanity_manage"), "Change a plaque text. Body: {text}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const std::string name(PathSegment(context.originalPath, 3));
			const auto body = ParseBody(context);
			if (std::none_of(TEXTS.begin(), TEXTS.end(), [&](const auto& t) { return t.first == name; })) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Unknown text");
			if (!body || !(*body)["text"].is_string() || (*body)["text"].get<std::string>().size() > 20000) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Send {text} (up to 20000 characters)");
			if (!WriteText(Folder() / name, (*body)["text"].get<std::string>())) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Couldn't write " + name);
			Audit(context, "edit_vanity", "Changed " + name);
			JsonSuccess(reply, { {"message", "Saved " + name} });
		});

	Route(eHTTPMethod::GET, "/api/vanity/names", Perm("vanity_manage"), "Item names for LOTs. Query: ?lots=1,2,3",
		[](HTTPReply& reply, const HTTPContext& context) {
			std::set<int32_t> lots;
			for (const auto& part : GeneralUtils::SplitString(QueryValue(context.queryString, "lots"), ',')) {
				if (const auto lot = GeneralUtils::TryParse<int32_t>(part); lot && *lot > 0 && lots.size() < 200) lots.insert(*lot);
			}
			JsonSuccess(reply, { {"itemNames", ItemNames(lots)} });
		});

	Route(eHTTPMethod::POST, "/api/vanity/preview", Perm("vanity_manage"),
		"What the worlds would load: the vanity files with the vanity parts of the scheduled events on at {at} (unix, default now), or of the events {ids} "
		"instead. Returns {at, events (in merge order), files (each file met and why it is or isn't loaded), baseFiles (the same without events), xml, npcs, "
		"baseNpcs, fileNpcs, conflicts, fileConflicts, warnings}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body || !body->is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			JsonSuccess(reply, EventsCalendar::VanityPreview(*body));
		});

	Route(eHTTPMethod::GET, "/api/vanity/pickers", Perm("vanity_manage"),
		"For a vanity part of a scheduled event: every vanity file ({name, loaded (as the files say), npcs}) and every named NPC in them ({name, files})",
		[](HTTPReply& reply, const HTTPContext&) {
			nlohmann::json files = nlohmann::json::array();
			std::map<std::string, std::vector<std::string>> npcs; // name -> the files it is in
			for (const auto& [name, node] : IncludeTree()) {
				if (!node.exists || name == ROOT) continue;
				files.push_back({ {"name", name}, {"loaded", node.loaded}, {"npcs", node.npcs} });
				std::string error;
				if (const auto doc = ReadDocument(name, error)) {
					for (const auto& object : doc->objects) {
						if (!object.name.empty() && (npcs[object.name].empty() || npcs[object.name].back() != name)) npcs[object.name].push_back(name);
					}
				}
			}
			nlohmann::json list = nlohmann::json::array();
			for (const auto& [name, in] : npcs) list.push_back({ {"name", name}, {"files", in} });
			JsonSuccess(reply, { {"files", files}, {"npcs", list} });
		});

	Route(eHTTPMethod::POST, "/api/vanity/reload", Perm("vanity_manage"), "Respawn the vanity NPCs in every running world from the files",
		[](HTTPReply& reply, const HTTPContext& context) {
			PlayerActionRequest request;
			request.action = ePlayerAction::RELOAD_VANITY;
			Audit(context, "reload_vanity", "Respawned vanity NPCs");
			const auto requestId = PlayerActions::Request(request, context.accountId, [](const PlayerActionResult& result) {
				return PlayerActions::Outcome{ true, result.affected ? "Respawned in " + std::to_string(result.affected) + " world(s)" : "No world with vanity NPCs is running" };
			});
			JsonSuccess(reply, { {"requestId", requestId} });
		});
}
