#ifndef __INSTANCEMIGRATION__H__
#define __INSTANCEMIGRATION__H__

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "MessageType/Master.h"
#include "dCommonVars.h"

/**
 * Moving everyone in one world instance to another instance of the same zone: to replace an instance (a live update:
 * a fresh world server, possibly a newer binary, takes over) or to merge two quiet instances into one.
 *
 * Normally the client shows a loading screen (see docs/SeamlessTransfer.md): the source world saves each player,
 * locks them and sends TRANSFER_TO_WORLD with the "Mythran shift" flag, which shows the client's own "Mythran
 * Dimensional Shift Succeeded!" notice, and the target sends LOAD_STATIC_ZONE. Same zone means the player lands where
 * they stood. An experimental "seamless" mode skips LOAD_STATIC_ZONE so the client keeps its scene.
 *
 * Messages (all appended to MessageType::Master):
 *  INSTANCE_MIGRATE      world (a GM's /replaceinstance or /mergeinstance) or a future web dashboard -> master
 *                        InstanceMigrationRequest
 *  MIGRATE_PLAYERS       master -> source world MigratePlayersOrder
 *  MIGRATE_STATUS        source world -> master -> every world (for the GM who asked) MigrationStatus
 *  MIGRATE_PLAYER_STATE  source world -> master -> target world CarriedPlayerState
 */
namespace InstanceMigration {
	// Length-prefixed string, capped so a bad packet can't ask for huge allocations
	inline void WriteText(RakNet::BitStream& stream, const std::string& text, uint16_t max) {
		const auto length = static_cast<uint16_t>(std::min<size_t>(text.size(), max));
		stream.Write(length);
		stream.Write(text.data(), length);
	}

	inline bool ReadText(RakNet::BitStream& stream, std::string& text, uint16_t max) {
		uint16_t length{};
		if (!stream.Read(length) || length > max) return false;
		text.resize(length);
		return length == 0 || stream.Read(text.data(), length);
	}

	enum class eKind : uint8_t {
		REPLACE = 0, // start a fresh instance of the zone, move everyone there, shut the old one down
		MERGE = 1,   // move everyone into another running instance of the zone, shut this one down
	};

	enum class eState : uint8_t {
		STARTING_TARGET = 0, // waiting for the new instance to be ready
		WARNING,             // players were told they'll be moved
		MOVING,
		DONE,
		FAILED,
	};

	inline const char* KindName(eKind kind) { return kind == eKind::MERGE ? "merge" : "replace"; }

	inline const char* StateName(eState state) {
		switch (state) {
		case eState::STARTING_TARGET: return "starting_target";
		case eState::WARNING: return "warning";
		case eState::MOVING: return "moving";
		case eState::DONE: return "done";
		case eState::FAILED: return "failed";
		}
		return "unknown";
	}

	// What master knows about one running instance, enough to plan moves
	struct InstanceView {
		uint32_t zoneId{};
		uint32_t instanceId{};
		uint32_t cloneId{};
		int32_t players{};
		int32_t softCap{};
		int32_t hardCap{};
		int32_t reserved{}; // seats held for players being moved in
		bool ready{};
		bool isPrivate{};
		bool shuttingDown{};
		bool draining{}; // its players are being moved away; nobody new is sent there

		int32_t Load() const { return players + reserved; }
	};

	enum class eRefusal : uint8_t {
		NONE = 0,
		NOT_RUNNING,
		CHARACTER_SELECT,
		PRIVATE_INSTANCE,
		PROPERTY_OR_CLONE,
		ACTIVITY_ZONE,
		SHUTTING_DOWN,
		ALREADY_MIGRATING,
		NOT_READY,
		TARGET_IS_SOURCE,
		TARGET_OTHER_ZONE,
		TARGET_FULL,
		NO_TARGET,
		MASTER_SHUTTING_DOWN,
	};

	inline const char* Describe(eRefusal refusal) {
		switch (refusal) {
		case eRefusal::NONE: return "";
		case eRefusal::NOT_RUNNING: return "That instance isn't running";
		case eRefusal::CHARACTER_SELECT: return "Character selection can't be moved";
		case eRefusal::PRIVATE_INSTANCE: return "Private instances can't be moved";
		case eRefusal::PROPERTY_OR_CLONE: return "Properties and other cloned instances can't be moved";
		case eRefusal::ACTIVITY_ZONE: return "Activity instances (races, minigames) can't be moved";
		case eRefusal::SHUTTING_DOWN: return "That instance is shutting down";
		case eRefusal::ALREADY_MIGRATING: return "That instance is already being moved or taking players in";
		case eRefusal::NOT_READY: return "That instance is still starting";
		case eRefusal::TARGET_IS_SOURCE: return "Pick a different instance to merge into";
		case eRefusal::TARGET_OTHER_ZONE: return "Instances can only be merged within the same zone";
		case eRefusal::TARGET_FULL: return "The other instance doesn't have room for everyone";
		case eRefusal::NO_TARGET: return "No other instance of that zone has room for everyone";
		case eRefusal::MASTER_SHUTTING_DOWN: return "The server is shutting down";
		}
		return "Unknown reason";
	}

	// Whether the players of this instance may be moved at all. Clones (properties) and activity zones (races,
	// minigames) keep state that lives only in that world, and private instances belong to someone.
	inline eRefusal CheckSource(const InstanceView& source, bool activityZone) {
		if (source.zoneId == 0) return eRefusal::CHARACTER_SELECT;
		if (source.isPrivate) return eRefusal::PRIVATE_INSTANCE;
		if (source.cloneId != 0) return eRefusal::PROPERTY_OR_CLONE;
		if (activityZone) return eRefusal::ACTIVITY_ZONE;
		if (source.shuttingDown) return eRefusal::SHUTTING_DOWN;
		if (source.draining) return eRefusal::ALREADY_MIGRATING;
		if (!source.ready) return eRefusal::NOT_READY;
		return eRefusal::NONE;
	}

	// Whether everyone in source fits into target. Merges may fill up to the hard cap: the soft cap only keeps
	// room for friends joining, and a merge is those players staying together anyway.
	inline eRefusal CheckMergeTarget(const InstanceView& target, const InstanceView& source) {
		if (target.zoneId == source.zoneId && target.instanceId == source.instanceId) return eRefusal::TARGET_IS_SOURCE;
		if (target.zoneId != source.zoneId || target.cloneId != source.cloneId) return eRefusal::TARGET_OTHER_ZONE;
		if (target.isPrivate) return eRefusal::PRIVATE_INSTANCE;
		if (target.shuttingDown) return eRefusal::SHUTTING_DOWN;
		if (target.draining) return eRefusal::ALREADY_MIGRATING;
		if (!target.ready) return eRefusal::NOT_READY;
		if (target.Load() + source.players > target.hardCap) return eRefusal::TARGET_FULL;
		return eRefusal::NONE;
	}

	// The best instance to merge source into: one where everyone fits under the soft cap if there is one, and of
	// those the fullest (so players end up together). Ties go to the older instance.
	inline std::optional<uint32_t> PickMergeTarget(const std::vector<InstanceView>& instances, const InstanceView& source) {
		const InstanceView* best = nullptr;
		bool bestUnderSoft = false;
		for (const auto& candidate : instances) {
			if (CheckMergeTarget(candidate, source) != eRefusal::NONE) continue;
			const bool underSoft = candidate.Load() + source.players <= candidate.softCap;
			if (!best || (underSoft && !bestUnderSoft) ||
				(underSoft == bestUnderSoft && (candidate.Load() > best->Load() ||
				(candidate.Load() == best->Load() && candidate.instanceId < best->instanceId)))) {
				best = &candidate;
				bestUnderSoft = underSoft;
			}
		}
		if (!best) return std::nullopt;
		return best->instanceId;
	}

	struct MergeSuggestion {
		uint32_t zoneId{};
		uint32_t sourceInstance{};
		uint32_t targetInstance{};
		int32_t players{}; // moved
		int32_t resulting{}; // in the target afterwards
	};

	// Quiet instances worth merging: per zone, the emptiest instance goes into the fullest one that stays under the
	// soft cap, until nothing more fits. Empty instances aren't listed (they shut themselves down).
	inline std::vector<MergeSuggestion> SuggestMerges(const std::vector<InstanceView>& instances) {
		std::map<uint32_t, std::vector<InstanceView>> byZone;
		for (const auto& instance : instances) {
			if (CheckSource(instance, false) != eRefusal::NONE) continue;
			byZone[instance.zoneId].push_back(instance);
		}

		std::vector<MergeSuggestion> suggestions;
		for (auto& [zone, list] : byZone) {
			if (list.size() < 2) continue;
			std::sort(list.begin(), list.end(), [](const InstanceView& a, const InstanceView& b) {
				return a.Load() != b.Load() ? a.Load() < b.Load() : a.instanceId > b.instanceId;
			});
			std::vector<bool> mergedAway(list.size(), false);
			std::vector<bool> takingIn(list.size(), false);
			for (size_t i = 0; i < list.size(); i++) {
				if (takingIn[i] || list[i].players <= 0) continue;
				// Into the fullest one it still fits in; only ones at least as full (later in the list)
				for (size_t j = list.size(); j-- > i + 1;) {
					if (mergedAway[j] || list[j].Load() + list[i].players > list[j].softCap) continue;
					suggestions.push_back({ zone, list[i].instanceId, list[j].instanceId, list[i].players, list[j].Load() + list[i].players });
					list[j].reserved += list[i].players;
					mergedAway[i] = true;
					takingIn[j] = true;
					break;
				}
			}
		}
		return suggestions;
	}

	enum class ePlayerDecision : uint8_t {
		MOVE,              // move now
		CANCEL_TRADE_MOVE, // cancel the open trade (nothing changes hands), then move
		WAIT,              // try again next tick
	};

	// Whether a player can be moved right now. Players who are dead or building wait a little (their state there
	// isn't saved), then go anyway so a migration always finishes.
	inline ePlayerDecision DecidePlayer(bool inTrade, bool buildMode, bool dead, float waitedSeconds, float maxWaitSeconds) {
		if ((buildMode || dead) && waitedSeconds < maxWaitSeconds) return ePlayerDecision::WAIT;
		if (inTrade) return ePlayerDecision::CANCEL_TRADE_MOVE;
		return ePlayerDecision::MOVE;
	}
}

/**
 * INSTANCE_MIGRATE (a world for a GM command, or anything else connected to master): move everyone in one instance. targetInstance 0 picks a target for
 * merges (the best fit); replaces always start a fresh instance.
 */
struct InstanceMigrationRequest : public LUBitStream {
	InstanceMigrationRequest() : LUBitStream(ServiceType::MASTER, MessageType::Master::INSTANCE_MIGRATE) {}

	static constexpr uint16_t MAX_BY = 64;
	static constexpr uint16_t MAX_WARN_SECONDS = 300;

	uint32_t requestId{};
	InstanceMigration::eKind kind{ InstanceMigration::eKind::REPLACE };
	uint32_t zoneId{};
	uint32_t sourceInstance{};
	uint32_t targetInstance{};
	uint16_t warnSeconds{};
	bool shutdownSource{ true };
	bool seamless{}; // experimental: no loading screen (see MigratePlayersOrder::seamless)
	LWOOBJID requesterId{}; // the character who asked, told how it goes; 0 for none
	std::string requestedBy;

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(requestId);
		stream.Write(static_cast<uint8_t>(kind));
		stream.Write(zoneId);
		stream.Write(sourceInstance);
		stream.Write(targetInstance);
		stream.Write(std::min(warnSeconds, MAX_WARN_SECONDS));
		stream.Write<uint8_t>(shutdownSource ? 1 : 0);
		stream.Write<uint8_t>(seamless ? 1 : 0);
		stream.Write(requesterId);
		InstanceMigration::WriteText(stream, requestedBy, MAX_BY);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint8_t kindValue{}, shutdown{}, seamlessValue{};
		if (!stream.Read(requestId) || !stream.Read(kindValue) || !stream.Read(zoneId) || !stream.Read(sourceInstance) ||
			!stream.Read(targetInstance) || !stream.Read(warnSeconds) || !stream.Read(shutdown) || !stream.Read(seamlessValue) ||
			!stream.Read(requesterId)) return false;
		if (kindValue > static_cast<uint8_t>(InstanceMigration::eKind::MERGE) || warnSeconds > MAX_WARN_SECONDS) return false;
		kind = static_cast<InstanceMigration::eKind>(kindValue);
		shutdownSource = shutdown != 0;
		seamless = seamlessValue != 0;
		return InstanceMigration::ReadText(stream, requestedBy, MAX_BY);
	}
};

/**
 * MIGRATE_PLAYERS (master -> source world): send everyone to this instance. The target is already running and has
 * seats held for them.
 */
struct MigratePlayersOrder : public LUBitStream {
	MigratePlayersOrder() : LUBitStream(ServiceType::MASTER, MessageType::Master::MIGRATE_PLAYERS) {}

	static constexpr uint16_t MAX_IP = 255;

	uint32_t migrationId{};
	uint32_t targetZone{};
	uint32_t targetInstance{};
	uint32_t targetClone{};
	std::string targetIp;
	uint16_t targetPort{};
	uint16_t warnSeconds{};
	uint16_t playersPerSecond{ 10 };
	bool mythranShift{ true }; // the client's own "moved to a new dimension" notice after the transfer
	/**
	 * Experimental: keep the client's scene. The source destroys every object it sent the client (the client keeps
	 * its own player object when its ghost goes), then transfers; the target skips LOAD_STATIC_ZONE and constructs
	 * everything at once, and the client adopts its existing player object for the new ghost
	 * (LwoClientGhostManager::OnReceiveConstruction). Not tested with the real client yet.
	 */
	bool seamless{};

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(migrationId);
		stream.Write(targetZone);
		stream.Write(targetInstance);
		stream.Write(targetClone);
		InstanceMigration::WriteText(stream, targetIp, MAX_IP);
		stream.Write(targetPort);
		stream.Write(warnSeconds);
		stream.Write(playersPerSecond);
		stream.Write<uint8_t>(mythranShift ? 1 : 0);
		stream.Write<uint8_t>(seamless ? 1 : 0);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint8_t shift{}, seamlessValue{};
		if (!stream.Read(migrationId) || !stream.Read(targetZone) || !stream.Read(targetInstance) || !stream.Read(targetClone) ||
			!InstanceMigration::ReadText(stream, targetIp, MAX_IP) || !stream.Read(targetPort) || !stream.Read(warnSeconds) ||
			!stream.Read(playersPerSecond) || !stream.Read(shift) || !stream.Read(seamlessValue)) return false;
		mythranShift = shift != 0;
		seamless = seamlessValue != 0;
		if (playersPerSecond == 0) playersPerSecond = 1;
		return true;
	}
};

/**
 * MIGRATE_STATUS (source world -> master, master -> every world): progress of one migration. Master passes it on
 * with who asked for it, and sends its own (STARTING_TARGET, FAILED) the same way.
 */
struct MigrationStatus : public LUBitStream {
	MigrationStatus() : LUBitStream(ServiceType::MASTER, MessageType::Master::MIGRATE_STATUS) {}

	static constexpr uint16_t MAX_MESSAGE = 300;

	uint32_t migrationId{};
	InstanceMigration::eState state{ InstanceMigration::eState::STARTING_TARGET };
	InstanceMigration::eKind kind{ InstanceMigration::eKind::REPLACE };
	uint32_t zoneId{};
	uint32_t sourceInstance{};
	uint32_t targetInstance{};
	uint16_t moved{};
	uint16_t remaining{};
	uint16_t failed{};
	LWOOBJID requesterId{}; // filled in by master
	std::string message;

	bool Finished() const { return state == InstanceMigration::eState::DONE || state == InstanceMigration::eState::FAILED; }

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(migrationId);
		stream.Write(static_cast<uint8_t>(state));
		stream.Write(static_cast<uint8_t>(kind));
		stream.Write(zoneId);
		stream.Write(sourceInstance);
		stream.Write(targetInstance);
		stream.Write(moved);
		stream.Write(remaining);
		stream.Write(failed);
		stream.Write(requesterId);
		InstanceMigration::WriteText(stream, message, MAX_MESSAGE);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint8_t stateValue{}, kindValue{};
		if (!stream.Read(migrationId) || !stream.Read(stateValue) || !stream.Read(kindValue) || !stream.Read(zoneId) ||
			!stream.Read(sourceInstance) || !stream.Read(targetInstance) || !stream.Read(moved) || !stream.Read(remaining) ||
			!stream.Read(failed) || !stream.Read(requesterId)) return false;
		if (stateValue > static_cast<uint8_t>(InstanceMigration::eState::FAILED) || kindValue > static_cast<uint8_t>(InstanceMigration::eKind::MERGE)) return false;
		state = static_cast<InstanceMigration::eState>(stateValue);
		kind = static_cast<InstanceMigration::eKind>(kindValue);
		return InstanceMigration::ReadText(stream, message, MAX_MESSAGE);
	}
};

/**
 * MIGRATE_PLAYER_STATE (source world -> master -> target world): what a moved player had that their saved
 * character doesn't keep, put back when they finish loading in the target. Everything else (position, health,
 * imagination, armor, buffs, inventory, missions) is in the character XML, saved just before the transfer.
 */
struct CarriedPlayerState : public LUBitStream {
	CarriedPlayerState() : LUBitStream(ServiceType::MASTER, MessageType::Master::MIGRATE_PLAYER_STATE) {}

	uint32_t targetZone{};
	uint32_t targetInstance{};
	LWOOBJID characterId{};
	LWOOBJID petItemId{}; // the pet that was out (its item), summoned again; 0 for none
	bool seamless{}; // the client kept its scene: skip LOAD_STATIC_ZONE and load the player at once

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(targetZone);
		stream.Write(targetInstance);
		stream.Write(characterId);
		stream.Write(petItemId);
		stream.Write<uint8_t>(seamless ? 1 : 0);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint8_t seamlessValue{};
		if (!stream.Read(targetZone) || !stream.Read(targetInstance) || !stream.Read(characterId) || !stream.Read(petItemId) || !stream.Read(seamlessValue)) return false;
		seamless = seamlessValue != 0;
		return true;
	}
};

#endif  //!__INSTANCEMIGRATION__H__
