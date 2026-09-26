#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "IServerConfig.h"
#include "IServerOperations.h"

/**
 * The scheduled events' rules. The game has eight event settings, event_1..event_8 (sharedconfig.ini): auth sends
 * them to the client at login, and a world server loads objects gated on a feature named in one of them
 * (dZoneManager/Level.cpp). An event's feature part puts its feature in a free slot when it starts and puts the slot
 * back when it ends. A slot is free when nothing (files, environment or the dashboard) gives it a value. Pure; unit
 * tested.
 */
namespace EventSchedule {
	constexpr uint8_t SLOTS = 8;
	constexpr const char* FILE = "sharedconfig.ini";
	constexpr size_t MAX_FEATURE_LENGTH = 128;
	constexpr int64_t MAX_LENGTH_SECONDS = 366 * 86400;

	using eEventState = IServerOperations::eEventState;
	using ScheduledEvent = IServerOperations::ScheduledEvent;

	inline std::string SlotSetting(uint8_t slot) {
		return "event_" + std::to_string(slot);
	}

	// The slot number of an event_N setting name, or 0
	inline uint8_t SlotOf(const std::string& name) {
		if (name.size() != 7 || !name.starts_with("event_") || name[6] < '1' || name[6] > '0' + SLOTS) return 0;
		return static_cast<uint8_t>(name[6] - '0');
	}

	// Which slots have a value anywhere, from the reported settings of every file; index 1..8
	inline std::array<bool, SLOTS + 1> BusySlots(const std::vector<IServerConfig::Setting>& rows) {
		std::array<bool, SLOTS + 1> busy{};
		for (const auto& row : rows) {
			const auto slot = SlotOf(row.name);
			if (slot == 0) continue;
			if ((row.fileValue && !row.fileValue->empty()) || (row.webValue && !row.webValue->empty())) busy[slot] = true;
		}
		return busy;
	}

	inline std::optional<uint8_t> FreeSlot(const std::array<bool, SLOTS + 1>& busy) {
		for (uint8_t slot = 1; slot <= SLOTS; slot++) {
			if (!busy[slot]) return slot;
		}
		return std::nullopt;
	}

	// The sharedconfig.ini row of a slot, if the servers or the dashboard ever wrote one
	inline const IServerConfig::Setting* SlotRow(const std::vector<IServerConfig::Setting>& rows, uint8_t slot) {
		const auto name = SlotSetting(slot);
		for (const auto& row : rows) {
			if (row.file == FILE && row.name == name) return &row;
		}
		return nullptr;
	}

	// The slot a feature is already in (a setting or another event put it there), or 0
	inline uint8_t SlotHolding(const std::vector<IServerConfig::Setting>& rows, const std::string& feature) {
		for (const auto& row : rows) {
			const auto slot = SlotOf(row.name);
			if (slot && ((row.fileValue && *row.fileValue == feature) || (row.webValue && *row.webValue == feature))) return slot;
		}
		return 0;
	}

	// What is wrong with a feature name, if anything
	inline std::optional<std::string> ValidateFeature(const std::string& feature) {
		if (feature.empty() || feature.size() > MAX_FEATURE_LENGTH) return "Pick a feature";
		if (feature.find_first_of(",\r\n=") != std::string::npos) return "That isn't a feature name";
		return std::nullopt;
	}

	// What is wrong with the times of an event that is on once, if anything (checked when they are set or changed)
	inline std::optional<std::string> ValidateOnce(int64_t startsAt, int64_t endsAt, int64_t now) {
		if (startsAt <= 0 || endsAt <= 0) return "Pick a start and an end";
		if (endsAt <= startsAt) return "The end has to be after the start";
		if (endsAt <= now) return "The end is already in the past";
		if (endsAt - startsAt > MAX_LENGTH_SECONDS) return "An event can last at most a year";
		return std::nullopt;
	}

	/**
	 * The state an event is in after an update: ACTIVE while it is on or a part still has to be ended; an event that is
	 * on once ENDED after its end (MISSED if it never started: the dashboard was down throughout); CANCELLED stays while
	 * it is off; SCHEDULED otherwise.
	 */
	inline eEventState NextState(eEventState current, bool once, int64_t endsAt, bool on, bool applied, bool off, int64_t now) {
		if (on || applied) return eEventState::ACTIVE;
		if (current == eEventState::CANCELLED && off) return eEventState::CANCELLED;
		if (once && now >= endsAt) {
			if (current == eEventState::ACTIVE || current == eEventState::ENDED) return eEventState::ENDED;
			return current == eEventState::CANCELLED ? eEventState::CANCELLED : eEventState::MISSED;
		}
		return eEventState::SCHEDULED;
	}
}
