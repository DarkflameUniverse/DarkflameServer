#include "ZonePublish.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <sstream>

#include <openssl/evp.h>

#include "LevelFile.h"
#include "Raw.h"
#include "ZoneFile.h"
#include "eTriggerCommandType.h"
#include "eTriggerEventType.h"
#include "tinyxml2.h"

namespace {
	using namespace ZonePublish;

	std::string Hex(const unsigned char* bytes, size_t size) {
		static constexpr char digits[] = "0123456789abcdef";
		std::string out;
		out.reserve(size * 2);
		for (size_t i = 0; i < size; ++i) {
			out += digits[bytes[i] >> 4];
			out += digits[bytes[i] & 15];
		}
		return out;
	}

	void Add(File& file, std::string code, std::string message, std::optional<uint64_t> offset = std::nullopt) {
		file.ok = false;
		if (file.errors.size() >= MAX_ERRORS_PER_FILE) return;
		file.errors.push_back({ std::move(code), std::move(message), offset });
	}

	std::optional<std::string> ReadWhole(const std::filesystem::path& path, uint64_t maxBytes) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return std::nullopt;
		in.seekg(0, std::ios::end);
		const auto size = in.tellg();
		if (size < 0 || static_cast<uint64_t>(size) > maxBytes) return std::nullopt;
		in.seekg(0);
		std::string data(static_cast<size_t>(size), '\0');
		in.read(data.data(), static_cast<std::streamsize>(data.size()));
		if (in.gcount() != static_cast<std::streamsize>(data.size())) return std::nullopt;
		return data;
	}

	// Where a reader stopped, for the error
	std::optional<uint64_t> Position(std::istream& stream) {
		stream.clear();
		const auto position = stream.tellg();
		if (position < 0) return std::nullopt;
		return static_cast<uint64_t>(position);
	}

	std::string Stem(const std::string& path) {
		const auto dot = path.find_last_of('.');
		const auto slash = path.find_last_of('/');
		if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return path;
		return path.substr(0, dot);
	}

	// The .luz by itself: read to its end, in a version the server takes. false when its names can't be looked at.
	bool ReadZoneFile(File& file, const std::string& data, ZoneFile& luz) {
		std::istringstream stream(data);
		try {
			luz.Read(stream);
		} catch (const std::exception&) {
			Add(file, "parse_failed", "The zone file ends before everything in it was read", Position(stream));
			return false;
		}
		file.version = static_cast<uint32_t>(luz.fileFormatVersion);
		if (stream.fail()) {
			Add(file, "parse_failed", "The zone file ends before everything in it was read", Position(stream));
			return false;
		}
		if (luz.fileFormatVersion < ZoneFile::FileFormatVersion::PrePreAlpha || luz.fileFormatVersion > ZoneFile::FileFormatVersion::Latest) {
			Add(file, "unsupported_version", "Zone file version " + std::to_string(file.version) + " is not one the server takes (30 to 41)");
			return false;
		}
		if (luz.pathsRefused) Add(file, "parse_failed", "The zone file's paths can't be read");
		if (stream.peek() != std::char_traits<char>::eof()) {
			Add(file, "parse_failed", "The zone file has more in it after its last part", Position(stream));
		}
		return true;
	}

	/**
	 * The .luz: read to its end, a version the server reads, every scene one the zone has in the client files, and a
	 * terrain file in the zone's folder. Fills the names the rest of the set may have.
	 */
	void CheckZoneFile(File& file, const std::string& data, const Zone& zone, const std::string& folder,
		std::set<std::string>& sceneFiles, std::string& terrain) {
		ZoneFile luz;
		if (!ReadZoneFile(file, data, luz)) return;

		if (luz.scenes.empty()) Add(file, "scene_mismatch", "The zone file lists no scenes");
		for (const auto& scene : luz.scenes) {
			if (NameProblem(scene.filename) || !Canonical(scene.filename).ends_with(".lvl")) {
				Add(file, "scene_mismatch", "Scene " + std::to_string(scene.id) + " names a file that is not a scene file in the zone's folder");
				continue;
			}
			const auto path = folder + Canonical(scene.filename);
			if (!zone.clientScenes.contains(path)) {
				Add(file, "scene_mismatch", "Scene file " + path + " is not one of this zone's scenes");
				continue;
			}
			sceneFiles.insert(path);
		}

		if (NameProblem(luz.zoneRawPath) || !Canonical(luz.zoneRawPath).ends_with(".raw")) {
			Add(file, "terrain_mismatch", "The zone file names a terrain file that is not a .raw in the zone's folder");
		} else {
			terrain = folder + Canonical(luz.zoneRawPath);
		}
	}

	void CheckLevelFile(File& file, const std::string& data, uint32_t& objects, uint32_t maxObjects) {
		std::istringstream stream(data);
		LevelFile level;
		try {
			level.Read(stream);
		} catch (const std::exception&) {
			file.version = level.version;
			return Add(file, "parse_failed", "The scene file ends before everything in it was read", Position(stream));
		}
		file.version = level.version;
		if (stream.fail()) return Add(file, "parse_failed", "The scene file ends before everything in it was read", Position(stream));
		if (level.version > 48) return Add(file, "unsupported_version", "Scene file version " + std::to_string(level.version) + " is newer than the server reads");

		for (const auto& object : level.objects) {
			for (const auto& [key, value] : object.settings) {
				if (!key.starts_with(u"FAILED_TO_PARSE_")) continue;
				Add(file, "bad_object_config", "Object " + std::to_string(object.id) + " (LOT " + std::to_string(object.lot) + ") has a setting that can't be read");
				break;
			}
		}
		objects += static_cast<uint32_t>(std::min<size_t>(level.objects.size(), UINT32_MAX - objects));
		if (objects > maxObjects) {
			Add(file, "too_many_objects", "The set has more than " + std::to_string(maxObjects) + " objects in its scenes");
		}
	}

	void CheckTerrainFile(File& file, const std::string& data) {
		std::istringstream stream(data);
		Raw::Raw raw;
		const bool read = Raw::ReadRaw(stream, raw);
		file.version = raw.version;
		if (!read || stream.fail()) return Add(file, "parse_failed", "The terrain file can't be read to its end", Position(stream));
		// A terrain file the game uses has its chunks in it, and nothing after them
		if (raw.dev != 0 || raw.chunks.empty()) return Add(file, "parse_failed", "The terrain file has no terrain in it");
		if (stream.peek() != std::char_traits<char>::eof()) Add(file, "parse_failed", "The terrain file has more in it after its last chunk", Position(stream));
	}
}

namespace ZonePublish {
	std::string FolderOf(std::string_view luzPath) {
		const auto slash = luzPath.find_last_of('/');
		return slash == std::string_view::npos ? std::string() : std::string(luzPath.substr(0, slash + 1));
	}

	std::string Canonical(std::string_view name) {
		std::string out(name);
		for (auto& c : out) {
			if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
		}
		return out;
	}

	std::optional<std::string> NameProblem(std::string_view name) {
		if (name.empty()) return "The name is empty";
		if (name.size() > MAX_NAME_LENGTH) return "The name is longer than " + std::to_string(MAX_NAME_LENGTH) + " characters";
		if (name.find('\0') != std::string_view::npos) return "The name has a NUL in it";
		if (name.find('\\') != std::string_view::npos) return "The name has a backslash in it; use forward slashes";
		if (name.front() == '/') return "The name is an absolute path";
		for (const char c : name) {
			const auto byte = static_cast<unsigned char>(c);
			if (byte < 0x20 || byte >= 0x7F) return "The name has a character in it that is not printable ASCII";
			if (std::string_view(":*?\"<>|").find(c) != std::string_view::npos) return "The name has a character in it that file names can't have";
		}
		size_t start = 0;
		while (start <= name.size()) {
			const auto end = std::min(name.find('/', start), name.size());
			const auto part = name.substr(start, end - start);
			if (part.empty()) return "The name has an empty part";
			if (part == "." || part == "..") return "The name has a \".\" or \"..\" part";
			if (part.front() == ' ' || part.back() == ' ' || part.back() == '.') return "A part of the name starts or ends with a space, or ends with a dot";
			start = end + 1;
		}
		return std::nullopt;
	}

	std::optional<std::filesystem::path> ResolveInside(const std::filesystem::path& root, const std::string& relative) {
		if (root.empty() || NameProblem(relative)) return std::nullopt;
		std::error_code code;
		const auto base = std::filesystem::weakly_canonical(root, code);
		if (code) return std::nullopt;
		const auto target = std::filesystem::weakly_canonical(base / relative, code);
		if (code) return std::nullopt;
		const auto inside = target.lexically_relative(base);
		if (inside.empty() || inside.native().starts_with("..") || inside.is_absolute()) return std::nullopt;
		return target;
	}

	std::string Sha256(std::string_view bytes) {
		std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
		unsigned int size = 0;
		EVP_Digest(bytes.data(), bytes.size(), digest.data(), &size, EVP_sha256(), nullptr);
		return Hex(digest.data(), size);
	}

	std::optional<std::string> Sha256File(const std::filesystem::path& file) {
		std::ifstream in(file, std::ios::binary);
		if (!in) return std::nullopt;
		auto* context = EVP_MD_CTX_new();
		if (!context) return std::nullopt;
		EVP_DigestInit_ex(context, EVP_sha256(), nullptr);
		std::array<char, 65536> buffer{};
		while (in) {
			in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
			if (in.gcount() > 0) EVP_DigestUpdate(context, buffer.data(), static_cast<size_t>(in.gcount()));
		}
		std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
		unsigned int size = 0;
		EVP_DigestFinal_ex(context, digest.data(), &size);
		EVP_MD_CTX_free(context);
		if (in.bad()) return std::nullopt;
		return Hex(digest.data(), size);
	}

	std::vector<Error> CheckTriggers(std::string_view xml) {
		std::vector<Error> errors;
		const auto add = [&errors](std::string code, std::string message) {
			if (errors.size() < MAX_ERRORS_PER_FILE) errors.push_back({ std::move(code), std::move(message), std::nullopt });
		};
		// An empty file has no triggers (the world reads it as none)
		if (xml.empty()) return errors;
		tinyxml2::XMLDocument doc;
		if (doc.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS) {
			add("parse_failed", "The triggers file is not well formed XML (line " + std::to_string(doc.ErrorLineNum()) + ")");
			return errors;
		}
		const auto* triggers = doc.FirstChildElement("triggers");
		if (!triggers) {
			add("parse_failed", "The triggers file has no <triggers> element");
			return errors;
		}
		for (const auto* trigger = triggers->FirstChildElement("trigger"); trigger; trigger = trigger->NextSiblingElement("trigger")) {
			uint32_t id = 0;
			if (trigger->QueryUnsignedAttribute("id", &id) != tinyxml2::XML_SUCCESS) add("parse_failed", "A trigger has no numeric id");
			const auto where = "Trigger " + std::to_string(id);
			for (const auto* event = trigger->FirstChildElement("event"); event; event = event->NextSiblingElement("event")) {
				const char* eventId = event->Attribute("id");
				if (!eventId || TriggerEventType::StringToTriggerEventType(eventId) == eTriggerEventType::INVALID) {
					add("unknown_trigger_event", where + " has an event the server doesn't know: " + std::string(eventId ? eventId : "(no id)"));
				}
				for (const auto* command = event->FirstChildElement("command"); command; command = command->NextSiblingElement("command")) {
					const char* commandId = command->Attribute("id");
					if (!commandId || TriggerCommandType::StringToTriggerCommandType(commandId) == eTriggerCommandType::INVALID) {
						add("unknown_trigger_command", where + " has a command the server doesn't know: " + std::string(commandId ? commandId : "(no id)"));
					}
					const char* target = command->Attribute("target");
					if (!target || !*target) {
						add("trigger_missing_target", where + " has a " + std::string(commandId ? commandId : "command") + " command without a target");
					}
				}
			}
		}
		return errors;
	}

	std::vector<Error> CheckContents(std::string_view name, const std::string& data, uint32_t* version) {
		File file;
		uint32_t objects = 0;
		const auto lower = Canonical(name);
		if (lower.ends_with(".luz")) {
			ZoneFile luz;
			ReadZoneFile(file, data, luz);
		} else if (lower.ends_with(".lvl")) CheckLevelFile(file, data, objects, UINT32_MAX);
		else if (lower.ends_with(".raw")) CheckTerrainFile(file, data);
		else if (lower.ends_with(".lutriggers")) file.errors = CheckTriggers(data);
		if (version) *version = file.version;
		return file.errors;
	}

	Result Validate(const Zone& zone, const std::vector<Upload>& uploads, const Limits& limits) {
		Result result;
		const auto luzPath = Canonical(zone.luzPath);
		const auto folder = FolderOf(luzPath);
		result.files.reserve(uploads.size() + 1);

		// Names first: what each file is called, twice the same, and whether the .luz is there
		std::map<std::string, size_t> byPath;
		std::optional<size_t> luzIndex;
		uint64_t total = 0;
		for (size_t i = 0; i < uploads.size(); ++i) {
			const auto& upload = uploads[i];
			File file;
			file.path = upload.name;
			file.bytes = upload.bytes;
			file.source = upload.file;
			total += upload.bytes;
			if (i >= limits.maxFiles) {
				Add(file, "too_large", "The set has more than " + std::to_string(limits.maxFiles) + " files");
			} else if (const auto problem = NameProblem(upload.name)) {
				Add(file, "name_not_allowed", *problem);
			} else {
				file.path = Canonical(upload.name);
				if (!file.path.starts_with(folder) || file.path.size() == folder.size()) {
					Add(file, "name_not_allowed", "The file is not in the zone's folder (" + folder + ")");
				} else if (!byPath.try_emplace(file.path, i).second) {
					Add(file, "duplicate", "The set has this file more than once");
				} else if (file.path == luzPath) {
					luzIndex = i;
				}
				if (upload.bytes > limits.maxFileBytes) {
					Add(file, "too_large", "The file is larger than " + std::to_string(limits.maxFileBytes) + " bytes");
				} else if (file.ok && total > limits.maxTotalBytes) {
					Add(file, "too_large", "The set is larger than " + std::to_string(limits.maxTotalBytes) + " bytes");
				}
			}
			result.files.push_back(std::move(file));
		}

		// The .luz says which other names the set may have
		std::set<std::string> sceneFiles;
		std::string terrain;
		bool luzRead = false;
		if (!luzIndex) {
			File missing;
			missing.path = luzPath;
			Add(missing, "missing_luz", "The set has no " + luzPath + "; every set has the zone's .luz");
			result.files.push_back(std::move(missing));
		} else if (auto& file = result.files[*luzIndex]; file.ok) {
			const auto data = ReadWhole(file.source, limits.maxFileBytes);
			if (!data) {
				Add(file, "parse_failed", "The file could not be read");
			} else {
				file.sha256 = Sha256(*data);
				CheckZoneFile(file, *data, zone, folder, sceneFiles, terrain);
				luzRead = file.ok;
			}
		}

		std::set<std::string> allowed{ luzPath };
		if (!terrain.empty()) allowed.insert(terrain);
		for (const auto& scene : sceneFiles) {
			allowed.insert(scene);
			allowed.insert(Stem(scene) + ".lutriggers");
		}

		for (size_t i = 0; i < result.files.size(); ++i) {
			auto& file = result.files[i];
			if (!file.ok || (luzIndex && i == *luzIndex) || file.source.empty()) continue;
			if (!luzRead) {
				// Without a good .luz nothing says which files belong; only the plainly wrong ones are named
				const bool known = file.path.ends_with(".lvl") || file.path.ends_with(".lutriggers") || file.path.ends_with(".raw");
				if (!known) Add(file, "name_not_allowed", "Only the zone's .luz, its scenes' .lvl and .lutriggers files and its .raw can be sent");
				continue;
			}
			if (!allowed.contains(file.path)) {
				if (file.path.ends_with(".raw")) Add(file, "terrain_mismatch", "The zone file names another terrain file (" + terrain + ")");
				else Add(file, "name_not_allowed", "The zone file doesn't list this file: only its scenes' .lvl and .lutriggers files and its .raw can be sent");
				continue;
			}
			const auto data = ReadWhole(file.source, limits.maxFileBytes);
			if (!data) {
				Add(file, "parse_failed", "The file could not be read");
				continue;
			}
			file.sha256 = Sha256(*data);
			if (file.path.ends_with(".lvl")) CheckLevelFile(file, *data, result.objects, limits.maxObjects);
			else if (file.path.ends_with(".lutriggers")) {
				for (auto& error : CheckTriggers(*data)) Add(file, std::move(error.code), std::move(error.message), error.offset);
			} else CheckTerrainFile(file, *data);
		}

		// What the .luz names and the set leaves out has to be there already
		if (luzRead) {
			auto& luz = result.files[*luzIndex];
			const auto there = [&](const std::string& path) { return byPath.contains(path) || (zone.hasFile && zone.hasFile(path)); };
			for (const auto& scene : sceneFiles) {
				if (!there(scene)) Add(luz, "scene_mismatch", "Scene file " + scene + " is not in the set and the zone doesn't have it");
			}
			if (!terrain.empty() && !there(terrain)) Add(luz, "terrain_mismatch", "Terrain file " + terrain + " is not in the set and the zone doesn't have it");
		}

		for (const auto& file : result.files) result.ok = result.ok && file.ok;
		return result;
	}
}
