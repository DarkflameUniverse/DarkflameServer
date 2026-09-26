#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "dCommonVars.h"
#include "PlayerActions.h"
#include "ICharacterSnapshots.h"

/**
 * Character support tools: the character editor, snapshots (history, compare, restore) and the character_snapshots
 * scheduled task that keeps a copy of every changed character.
 */
void RegisterCharacterToolRoutes();

// Before Scheduler::Initialize
void RegisterCharacterTasks();

// A snapshot's character XML, or nullopt if it can't be read
std::optional<std::string> SnapshotXml(const ICharacterSnapshots::CharacterSnapshot& snapshot);

/**
 * Change a character's saved data safely: the owner is disconnected first (so their world saves and lets go), then
 * `change` turns the current XML into the new one (or returns nullopt with an error), a snapshot of the current
 * version is kept, and the change is audited with `describe`'s text. Returns a PlayerActions request id; `done` also
 * gets the outcome (for work that reports under a request id of its own).
 */
uint32_t WriteCharacterXml(LWOOBJID characterId, uint32_t ownerAccountId, const std::string& actor, uint32_t actorId, const std::string& reason,
	std::function<std::optional<std::string>(const std::string& current, std::string& error)> change,
	std::function<std::string(const std::string& before, const std::string& after)> describe = nullptr,
	std::function<void(const PlayerActions::Outcome& outcome)> done = nullptr);
