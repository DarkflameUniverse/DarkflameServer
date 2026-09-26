#include "VanityEvents.h"

#include <fstream>
#include <map>
#include <set>

#include "GeneralUtils.h"

VanityEvents::Merged VanityEvents::Merge(std::vector<VanityXml::Object> base, const std::vector<Overlay>& overlays) {
	Merged merged;
	merged.objects = std::move(base);
	std::map<std::string, std::vector<std::string>> touched; // NPC name -> the events that changed it, in order
	for (const auto& overlay : overlays) {
		std::set<std::string> names;
		for (const auto& object : overlay.objects) if (!object.name.empty()) names.insert(object.name);
		for (const auto& name : overlay.removals) {
			const bool present = std::any_of(merged.objects.begin(), merged.objects.end(), [&](const auto& o) { return o.name == name; });
			if (!present && !names.contains(name)) merged.warnings.push_back(overlay.event + ": there is no NPC named \"" + name + "\" to take out");
			names.insert(name);
		}
		std::erase_if(merged.objects, [&](const auto& o) { return !o.name.empty() && names.contains(o.name); });
		merged.objects.insert(merged.objects.end(), overlay.objects.begin(), overlay.objects.end());
		for (const auto& name : names) touched[name].push_back(overlay.event);
	}
	for (auto& [npc, events] : touched) {
		if (events.size() > 1) merged.conflicts.push_back({ npc, std::move(events) });
	}
	return merged;
}

std::vector<std::string> VanityEvents::SplitNames(const std::string& text) {
	std::vector<std::string> names;
	for (auto line : GeneralUtils::SplitString(text, '\n')) {
		const auto first = line.find_first_not_of(" \t\r");
		if (first == std::string::npos) continue;
		line = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
		if (std::find(names.begin(), names.end(), line) == names.end()) names.push_back(line);
	}
	return names;
}

bool VanityEvents::ValidFileName(const std::string& name) {
	if (name.size() < 5 || name.size() > 64 || !name.ends_with(".xml")) return false;
	return std::all_of(name.begin(), name.end() - 4, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'; });
}

std::optional<VanityEvents::FileSwitches> VanityEvents::ParseFileSwitches(const nlohmann::json& json, std::string& error) {
	if (!json.is_object()) { error = "File switches are an object: {\"file.xml\": true or false}"; return std::nullopt; }
	if (json.size() > MAX_FILE_SWITCHES) { error = "At most " + std::to_string(MAX_FILE_SWITCHES) + " file switches"; return std::nullopt; }
	FileSwitches switches;
	for (const auto& [name, on] : json.items()) {
		if (!ValidFileName(name)) { error = "\"" + name + "\" isn't a vanity file name (letters, digits, - and _, then .xml)"; return std::nullopt; }
		if (!on.is_boolean()) { error = name + ": switch it on (true) or off (false)"; return std::nullopt; }
		switches[name] = on.get<bool>();
	}
	return switches;
}

std::optional<VanityEvents::FileSwitches> VanityEvents::ParseFileSwitches(const std::string& text, std::string& error) {
	if (text.find_first_not_of(" \t\r\n") == std::string::npos) return FileSwitches{};
	const auto json = nlohmann::json::parse(text, nullptr, false);
	if (json.is_discarded()) { error = "The file switches aren't valid JSON"; return std::nullopt; }
	return ParseFileSwitches(json, error);
}

nlohmann::json VanityEvents::ToJson(const FileSwitches& switches) {
	nlohmann::json json = nlohmann::json::object();
	for (const auto& [name, on] : switches) json[name] = on;
	return json;
}

namespace {
	using namespace VanityEvents;

	// Reads vanity files into one Loaded, each file once however often it is named
	class Loader {
	public:
		Loader(const std::filesystem::path& folder, const Switches& switches) : m_Folder(folder), m_Switches(switches) {}

		// Read a file (and what it switches on), unless it was read already
		void Read(FileLoad entry) {
			const auto name = entry.name;
			if (!m_Seen.insert(name).second) return;
			const auto index = Record(std::move(entry), true);
			// Any name the files give, as the worlds always took, as long as it stays in the vanity folder
			const std::filesystem::path relative(name);
			if (name.empty() || relative.is_absolute() || relative.has_root_path() || name.find("..") != std::string::npos) {
				m_Loaded.warnings.push_back("\"" + name + "\" isn't a file in the vanity folder");
				return;
			}
			std::ifstream in(m_Folder / name, std::ios::binary);
			if (!in) { m_Loaded.warnings.push_back(name + " not found"); return; }
			const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			std::string error;
			const auto document = VanityXml::Read(text, error);
			if (!document) { m_Loaded.warnings.push_back(name + ": " + error); return; }
			m_Loaded.files.push_back(name);
			m_Loaded.trace[index].loaded = true;
			for (const auto& warning : document->warnings) m_Loaded.warnings.push_back(name + ": skipped " + warning);
			for (const auto& include : document->files) {
				const auto it = m_Switches.find(include.name);
				FileLoad child{ .name = include.name, .includedBy = name, .enabled = include.enabled, .switchedBy = it == m_Switches.end() ? "" : it->second.event };
				if (it == m_Switches.end() ? include.enabled : it->second.on) Read(std::move(child));
				else Record(std::move(child), false);
			}
			m_Loaded.objects.insert(m_Loaded.objects.end(), document->objects.begin(), document->objects.end());
		}

		bool Met(const std::string& name) const { return m_Traced.contains(name); }

		Loaded Take() { return std::move(m_Loaded); }

	private:
		// The first time a file is met is where it is listed; being read later (named again, switched on) says why instead
		size_t Record(FileLoad entry, bool reading) {
			const auto [it, inserted] = m_Traced.try_emplace(entry.name, m_Loaded.trace.size());
			if (inserted) m_Loaded.trace.push_back(std::move(entry));
			else if (reading) m_Loaded.trace[it->second] = std::move(entry);
			return it->second;
		}

		const std::filesystem::path& m_Folder;
		const Switches& m_Switches;
		Loaded m_Loaded;
		std::set<std::string> m_Seen;              // read, or tried
		std::map<std::string, size_t> m_Traced;    // name -> its entry in m_Loaded.trace
	};
}

VanityEvents::Loaded VanityEvents::LoadFiles(const std::filesystem::path& folder, const std::string& start, const Switches& switches) {
	Loader loader(folder, switches);
	loader.Read({ .name = start, .enabled = true });
	return loader.Take();
}

VanityEvents::World VanityEvents::LoadWorld(const std::filesystem::path& folder, const std::string& root, const std::vector<Changes>& events) {
	World world;
	// The file switches, later events over earlier ones
	Switches switches;
	std::map<std::string, std::vector<std::pair<std::string, bool>>> switchedBy;
	for (const auto& event : events) {
		world.events.push_back(event.name);
		std::string error;
		const auto parsed = ParseFileSwitches(event.fileSwitches, error);
		if (!parsed) { world.warnings.push_back(event.name + ": file switches: " + error); continue; }
		for (const auto& [file, on] : *parsed) {
			if (file == root) { world.warnings.push_back(event.name + ": " + root + " is always loaded; its switch is ignored"); continue; }
			switches[file] = { on, event.name };
			switchedBy[file].emplace_back(event.name, on);
		}
	}
	for (auto& [file, list] : switchedBy) {
		if (list.size() > 1) world.fileConflicts.push_back({ file, std::move(list) });
	}

	// root.xml and what it switches on, then the files switched on that it didn't reach
	Loader loader(folder, switches);
	loader.Read({ .name = root, .enabled = true });
	for (const auto& [file, which] : switches) {
		if (which.on) loader.Read({ .name = file, .switchedBy = which.event });
	}
	for (const auto& [file, which] : switches) {
		if (!which.on && !loader.Met(file)) world.warnings.push_back(which.event + " switches " + file + " off, but no loaded file includes it");
	}
	auto base = loader.Take();
	world.files = std::move(base.trace);
	world.warnings.insert(world.warnings.end(), base.warnings.begin(), base.warnings.end());
	world.fileNpcs = base.objects.size();
	const std::set<std::string> baseFiles(base.files.begin(), base.files.end());

	// Then the overlays
	std::vector<Overlay> overlays;
	for (const auto& event : events) {
		Overlay overlay{ .event = event.name, .removals = SplitNames(event.removals) };
		if (!event.file.empty()) {
			if (baseFiles.contains(event.file)) world.warnings.push_back(event.name + ": its overlay file " + event.file + " is loaded as a vanity file too, so its NPCs are there anyway");
			auto loaded = LoadFiles(folder, event.file, switches);
			for (const auto& warning : loaded.warnings) world.warnings.push_back(event.name + ": " + warning);
			overlay.objects = std::move(loaded.objects);
		}
		overlays.push_back(std::move(overlay));
	}
	auto merged = Merge(std::move(base.objects), overlays);
	world.objects = std::move(merged.objects);
	world.conflicts = std::move(merged.conflicts);
	world.warnings.insert(world.warnings.end(), merged.warnings.begin(), merged.warnings.end());
	return world;
}
