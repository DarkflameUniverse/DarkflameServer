#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "json.hpp"
#include "VanityEvents.h"

/**
 * The parts of a scheduled event (scheduled_events.parts): what it switches on while it is on and off again after.
 * Stored as JSON, [{kind, config, applied, state, status}]:
 *   feature       {feature}: the feature is put in a free event_1..event_8 setting (state: slot, previousValue, previousWebWins)
 *   vanity        {file, removals, fileSwitches}: vanity changes (VanityEvents.h); the worlds respawn their vanity NPCs
 *   live_event    {type, title, message, zones, instance, config}: a live event runs while it is on (state: id)
 *   announcement  {title, message, zones, atStart, repeat, endMessage}: said when it starts, every `repeat` (a cron
 *                 schedule, UTC) while it is on, and endMessage when it ends (state: nextAt)
 *   restart       {when: start|end, minutes, reason}: a restart with in-game warnings is scheduled when it starts or ends
 * `applied` is whether the dashboard started the part and hasn't ended it since, so starting and ending happen once
 * each, whatever restarts in between. The dashboard does the work; the world servers only read the vanity parts.
 * Pure; unit tested.
 */
namespace EventParts {
	// Stored by name, so only append
	enum class eKind : uint8_t { FEATURE, VANITY, LIVE_EVENT, ANNOUNCEMENT, RESTART };

	// "feature", "vanity", "live_event", ...
	std::string KindName(eKind kind);
	std::optional<eKind> KindOf(const std::string& name);

	struct Part {
		eKind kind{ eKind::FEATURE };
		nlohmann::json config = nlohmann::json::object(); // what it does, as staff set it
		bool applied{};                                   // started and not ended since
		nlohmann::json state = nlohmann::json::object();  // what starting it left to undo or carry on
		std::string status;                               // the last thing that happened to it
	};

	// The stored parts; empty text is none. nullopt and error when it can't be read
	std::optional<std::vector<Part>> Parse(const std::string& text, std::string& error);
	std::optional<std::vector<Part>> Parse(const nlohmann::json& json, std::string& error);
	inline std::optional<std::vector<Part>> Parse(const char* text, std::string& error) { return Parse(std::string(text), error); }

	nlohmann::json ToJson(const Part& part);
	std::string Write(const std::vector<Part>& parts);

	// What a part needs when its event is `on`
	enum class eStep { NONE, START, END, CONTINUE };
	eStep StepFor(const Part& part, bool on);

	// Parts that only do something the moment their event starts or ends (announcements, restarts): changing one
	// while the event is on doesn't start it again (an announcement already said isn't said twice)
	bool Momentary(eKind kind);

	/**
	 * Staff changed an event's parts. Parts that stay as they were (the same kind and config) keep what the dashboard
	 * did with them, and so does a changed momentary part (matched by kind, in order); the rest start afresh.
	 * `retired`: parts that were started and are gone or changed now, to be undone.
	 */
	struct Reconciled {
		std::vector<Part> parts;
		std::vector<Part> retired;
	};
	Reconciled Reconcile(const std::vector<Part>& before, std::vector<Part> after);

	// An event's vanity parts, as VanityEvents::LoadWorld takes them (none if its parts can't be read)
	std::vector<VanityEvents::Changes> VanityChanges(const std::string& eventName, const std::string& parts);
}
