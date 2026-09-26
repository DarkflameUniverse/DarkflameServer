#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "json.hpp"
#include "ScheduleRules.h"
#include "VanityXml.h"

/**
 * Vanity changes made by scheduled events (the vanity part of an event, EventParts.h): a Halloween look in October, a
 * werewolf on full-moon nights. A vanity part has file switches (vanity files it switches on or off, as root.xml's
 * <file enabled> entries do), an overlay file (a vanity file that root.xml doesn't load, whose NPCs are added) and NPC
 * names to take out; its event has the priority. The world servers and the dashboard both go through LoadWorld:
 * root.xml and the files it switches on, with the events' file switches applied, then every event's overlay laid over
 * that (VanityUtilities); the dashboard respawns the NPCs when the set of vanity parts that are on changes. The vanity
 * files are never changed.
 *
 * Pure apart from LoadFiles and LoadWorld; unit tested.
 */
namespace VanityEvents {
	// The overlay of one event
	struct Overlay {
		std::string event;                      // for conflicts and warnings
		std::vector<VanityXml::Object> objects; // added; a named one replaces every NPC of that name
		std::vector<std::string> removals;      // names of NPCs taken out
	};

	// Several events that are on change the same NPC: the last one listed wins
	struct Conflict {
		std::string npc;
		std::vector<std::string> events;
	};

	struct Merged {
		std::vector<VanityXml::Object> objects;
		std::vector<Conflict> conflicts;
		std::vector<std::string> warnings;
	};

	/**
	 * Lay overlays over the base NPCs, in order (see SortForMerge). For each event: its removals and every NPC with the
	 * same name as one of its named objects are taken out, then its objects are added. Unnamed objects (props) are only
	 * ever added.
	 */
	Merged Merge(std::vector<VanityXml::Object> base, const std::vector<Overlay>& overlays);

	// Several events that are on switch the same file: the last one in merge order wins
	struct FileConflict {
		std::string file;
		std::vector<std::pair<std::string, bool>> switches; // event, on; in merge order
	};

	// The merge order: lowest priority first, so the highest priority is laid on last and wins; ties by id
	template<typename Event>
	void SortForMerge(std::vector<Event>& events) {
		std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
			return a.priority != b.priority ? a.priority < b.priority : a.id < b.id;
		});
	}

	// NPC names from a list, one per line (blank lines skipped)
	std::vector<std::string> SplitNames(const std::string& text);

	// A vanity file name the dashboard and the worlds accept: letters, digits, - and _, then .xml
	bool ValidFileName(const std::string& name);

	constexpr size_t MAX_FILE_SWITCHES = 64;

	// A vanity part's file switches: vanity file name -> on (loaded) or off (not loaded)
	using FileSwitches = std::map<std::string, bool>;

	// {"halloween.xml": true, "summer.xml": false}; names must pass ValidFileName
	std::optional<FileSwitches> ParseFileSwitches(const nlohmann::json& json, std::string& error);
	// From the stored text; empty text is no switches
	std::optional<FileSwitches> ParseFileSwitches(const std::string& text, std::string& error);
	inline std::optional<FileSwitches> ParseFileSwitches(const char* text, std::string& error) { return ParseFileSwitches(std::string(text), error); }
	nlohmann::json ToJson(const FileSwitches& switches);

	// The switch that decides a file, after every event that is on has been laid on
	struct Switch {
		bool on{};
		std::string event;
	};
	using Switches = std::map<std::string, Switch>;

	// Why a vanity file is or isn't loaded
	struct FileLoad {
		std::string name;
		std::string includedBy; // the loaded file whose <file> entry names it; empty for the start file and a file switched on that no loaded file names
		bool enabled{};         // what that entry says
		std::string switchedBy; // the event whose file switch decides it, if any
		bool loaded{};          // read: switched on (by its entry or an event), found and valid XML
	};

	struct Loaded {
		std::vector<VanityXml::Object> objects;
		std::vector<std::string> files;    // every file read, in order
		std::vector<FileLoad> trace;       // every file read or named by a file read, in the order met, each once
		std::vector<std::string> warnings; // files that couldn't be read, and skipped parts
	};

	/**
	 * Read a vanity file and every file it switches on (and those files' switched-on files), each once, in the order the
	 * worlds load them: a file's includes before its own objects. A file in `switches` is loaded or not as its switch
	 * says, wherever it is named, instead of as the <file> entry says (the start file is always read). Names that leave
	 * the folder are skipped.
	 */
	Loaded LoadFiles(const std::filesystem::path& folder, const std::string& start, const Switches& switches = {});

	// What LoadWorld needs of an event that is on: its vanity changes
	struct Changes {
		std::string name;
		std::string file;         // overlay file, may be empty
		std::string removals;     // NPC names, one per line
		std::string fileSwitches; // JSON (ParseFileSwitches)
	};

	// What the worlds spawn, and how it came about
	struct World {
		std::vector<VanityXml::Object> objects;
		std::vector<FileLoad> files;          // the vanity files met from the root on, then those switched on alone
		std::vector<std::string> events;      // the events, in merge order
		size_t fileNpcs{};                    // objects from the vanity files, before the overlays
		std::vector<Conflict> conflicts;      // NPCs changed by several events
		std::vector<FileConflict> fileConflicts;
		std::vector<std::string> warnings;
	};

	/**
	 * Everything the worlds load, from one place so the worlds and the dashboard can't disagree. `events` (the ones that
	 * are on, in merge order) switch files first, the last one to switch a file winning; then `root` is read with those
	 * switches, followed by every file switched on that it didn't reach (by name); then each event's overlay (its file,
	 * with the same switches) and removals are laid on (Merge).
	 */
	World LoadWorld(const std::filesystem::path& folder, const std::string& root, const std::vector<Changes>& events);

	/**
	 * LoadWorld with events that have id, name, file, removals, fileSwitches and priority, put in merge order first.
	 */
	template<typename Event>
	World LoadWorld(const std::filesystem::path& folder, const std::string& root, std::vector<Event> events) {
		SortForMerge(events);
		std::vector<Changes> changes;
		for (const auto& event : events) changes.push_back({ event.name, event.file, event.removals, event.fileSwitches });
		return LoadWorld(folder, root, changes);
	}
}
