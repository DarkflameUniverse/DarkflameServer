#ifndef __LIVEUPDATE__H__
#define __LIVEUPDATE__H__

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "MessageType/Master.h"
#include "dCommonVars.h"
#include "master/InstanceMigration.h"

/**
 * Live updates (docs/LiveUpdate.md): with a new build of the server binaries in place, master moves everything onto
 * processes started from it without taking the server down. The UGC server finishes its jobs and restarts, auth
 * restarts, chat hands its teams over and restarts, every world instance is replaced by a new one its players are
 * moved to (instance migrations, InstanceMigration.h), and the dashboard restarts last. Master itself keeps running:
 * updating master takes a normal restart.
 *
 * Messages (appended to MessageType::Master):
 *  LIVE_UPDATE_REQUEST  dashboard, or a world for a GM's /liveupdate -> master   LiveUpdateRequest
 *  LIVE_UPDATE_STATUS   master -> dashboard (and the world of the GM who asked)  LiveUpdateStatus
 *  LIVE_UPDATE_RETIRE   master -> chat or UGC server                            LiveUpdateRetire
 *  CHAT_HANDOFF         retiring chat -> master -> the next chat server         ChatHandoff
 *  CHAT_SERVER_READY    master -> every world, once the new chat server is up   ChatServerReady
 *  MIGRATE_PREPARE      master -> a property instance being replaced            MigratePrepare (InstanceMigration.h)
 */
namespace LiveUpdate {
	enum class eAction : uint8_t {
		START = 0,
		CANCEL,  // start nothing new; what is under way finishes
		STATUS,  // just send the status (the dashboard asks when it connects)
	};

	// What one row of the update is about
	enum class eUnitKind : uint8_t {
		DATABASE = 0, // the new build's database migrations
		UGC,
		AUTH,
		CHAT,
		WORLD,        // one world instance
		DASHBOARD,
	};

	/**
	 * Where one row is. A world goes PENDING -> (PREPARING, a property saving itself) -> STARTING (its replacement is
	 * starting) -> READY (the replacement took its first players or is waiting for them) -> DRAINING (players are
	 * being moved, or it waits for an activity to end; nobody new is sent to it) -> STOPPING -> STOPPED. A server:
	 * PENDING -> STOPPING (asked to exit) -> STARTING (the new one is launching) -> STOPPED (the new one is up).
	 */
	enum class eUnitState : uint8_t {
		PENDING = 0,
		PREPARING,
		STARTING,
		READY,
		WAITING,   // a world waiting for its players to leave by themselves (activities, character selection)
		DRAINING,
		STOPPING,
		STOPPED,   // done: replaced (or simply stopped when nobody was there)
		FAILED,    // left as it was (a world keeps running on the old build)
		SKIPPED,   // not running, not enabled, or the update was cancelled before it got there
	};

	enum class ePhase : uint8_t {
		IDLE = 0,
		RUNNING,
		CANCELLING, // cancelled; waiting for what is under way
		DONE,
		FAILED,     // stopped early (e.g. the database migrations failed); nothing after that was touched
		CANCELLED,
	};

	inline const char* KindName(eUnitKind kind) {
		switch (kind) {
		case eUnitKind::DATABASE: return "database";
		case eUnitKind::UGC: return "ugc";
		case eUnitKind::AUTH: return "auth";
		case eUnitKind::CHAT: return "chat";
		case eUnitKind::WORLD: return "world";
		case eUnitKind::DASHBOARD: return "dashboard";
		}
		return "unknown";
	}

	inline const char* StateName(eUnitState state) {
		switch (state) {
		case eUnitState::PENDING: return "pending";
		case eUnitState::PREPARING: return "preparing";
		case eUnitState::STARTING: return "starting";
		case eUnitState::READY: return "ready";
		case eUnitState::WAITING: return "waiting";
		case eUnitState::DRAINING: return "draining";
		case eUnitState::STOPPING: return "stopping";
		case eUnitState::STOPPED: return "stopped";
		case eUnitState::FAILED: return "failed";
		case eUnitState::SKIPPED: return "skipped";
		}
		return "unknown";
	}

	inline const char* PhaseName(ePhase phase) {
		switch (phase) {
		case ePhase::IDLE: return "idle";
		case ePhase::RUNNING: return "running";
		case ePhase::CANCELLING: return "cancelling";
		case ePhase::DONE: return "done";
		case ePhase::FAILED: return "failed";
		case ePhase::CANCELLED: return "cancelled";
		}
		return "unknown";
	}

	inline bool IsFinished(eUnitState state) {
		return state == eUnitState::STOPPED || state == eUnitState::FAILED || state == eUnitState::SKIPPED;
	}

	inline bool IsFinished(ePhase phase) {
		return phase == ePhase::DONE || phase == ePhase::FAILED || phase == ePhase::CANCELLED;
	}
}

// LIVE_UPDATE_REQUEST
struct LiveUpdateRequest : public LUBitStream {
	LiveUpdateRequest() : LUBitStream(ServiceType::MASTER, MessageType::Master::LIVE_UPDATE_REQUEST) {}

	static constexpr uint16_t MAX_BY = 64;
	static constexpr int32_t DEFAULT_WARN = -1;

	LiveUpdate::eAction action{ LiveUpdate::eAction::STATUS };
	int32_t warnSeconds{ DEFAULT_WARN }; // seconds players are warned before they are moved; -1: live_update_warn_seconds
	LWOOBJID requesterId{}; // the character who asked (told how it goes in chat); 0 for the dashboard
	std::string requestedBy;

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(static_cast<uint8_t>(action));
		stream.Write(warnSeconds);
		stream.Write(requesterId);
		InstanceMigration::WriteText(stream, requestedBy, MAX_BY);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint8_t actionValue{};
		if (!stream.Read(actionValue) || actionValue > static_cast<uint8_t>(LiveUpdate::eAction::STATUS)) return false;
		action = static_cast<LiveUpdate::eAction>(actionValue);
		if (!stream.Read(warnSeconds) || !stream.Read(requesterId)) return false;
		if (warnSeconds < DEFAULT_WARN || warnSeconds > InstanceMigrationRequest::MAX_WARN_SECONDS) return false;
		return InstanceMigration::ReadText(stream, requestedBy, MAX_BY);
	}
};

// LIVE_UPDATE_STATUS: the whole update, sent again whenever something changes
struct LiveUpdateStatus : public LUBitStream {
	LiveUpdateStatus() : LUBitStream(ServiceType::MASTER, MessageType::Master::LIVE_UPDATE_STATUS) {}

	static constexpr uint16_t MAX_TEXT = 300;
	static constexpr uint32_t MAX_UNITS = 4096;

	struct Unit {
		LiveUpdate::eUnitKind kind{};
		LiveUpdate::eUnitState state{};
		uint32_t zoneId{};
		uint32_t instanceId{};
		uint32_t cloneId{};
		uint32_t replacement{}; // the instance that took over (worlds)
		uint32_t players{};     // players there when the row was made
		uint16_t moved{};
		uint8_t isPrivate{};
		std::string message;
	};

	uint32_t updateId{}; // 0: there never was one since master started
	LiveUpdate::ePhase phase{ LiveUpdate::ePhase::IDLE };
	int64_t startedAt{};  // unix time
	int64_t finishedAt{}; // unix time, 0 while running
	LWOOBJID requesterId{};
	std::string by;
	std::string message;
	std::vector<Unit> units;

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(updateId);
		stream.Write(static_cast<uint8_t>(phase));
		stream.Write(startedAt);
		stream.Write(finishedAt);
		stream.Write(requesterId);
		InstanceMigration::WriteText(stream, by, MAX_TEXT);
		InstanceMigration::WriteText(stream, message, MAX_TEXT);
		const auto count = static_cast<uint32_t>(std::min<size_t>(units.size(), MAX_UNITS));
		stream.Write(count);
		for (uint32_t i = 0; i < count; i++) {
			const auto& unit = units[i];
			stream.Write(static_cast<uint8_t>(unit.kind));
			stream.Write(static_cast<uint8_t>(unit.state));
			stream.Write(unit.zoneId);
			stream.Write(unit.instanceId);
			stream.Write(unit.cloneId);
			stream.Write(unit.replacement);
			stream.Write(unit.players);
			stream.Write(unit.moved);
			stream.Write(unit.isPrivate);
			InstanceMigration::WriteText(stream, unit.message, MAX_TEXT);
		}
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint8_t phaseValue{};
		if (!stream.Read(updateId) || !stream.Read(phaseValue) || phaseValue > static_cast<uint8_t>(LiveUpdate::ePhase::CANCELLED)) return false;
		phase = static_cast<LiveUpdate::ePhase>(phaseValue);
		if (!stream.Read(startedAt) || !stream.Read(finishedAt) || !stream.Read(requesterId)) return false;
		if (!InstanceMigration::ReadText(stream, by, MAX_TEXT) || !InstanceMigration::ReadText(stream, message, MAX_TEXT)) return false;
		uint32_t count{};
		if (!stream.Read(count) || count > MAX_UNITS) return false;
		units.resize(count);
		for (auto& unit : units) {
			uint8_t kind{}, state{};
			if (!stream.Read(kind) || !stream.Read(state)) return false;
			if (kind > static_cast<uint8_t>(LiveUpdate::eUnitKind::DASHBOARD) || state > static_cast<uint8_t>(LiveUpdate::eUnitState::SKIPPED)) return false;
			unit.kind = static_cast<LiveUpdate::eUnitKind>(kind);
			unit.state = static_cast<LiveUpdate::eUnitState>(state);
			if (!stream.Read(unit.zoneId) || !stream.Read(unit.instanceId) || !stream.Read(unit.cloneId) || !stream.Read(unit.replacement) ||
				!stream.Read(unit.players) || !stream.Read(unit.moved) || !stream.Read(unit.isPrivate)) return false;
			if (!InstanceMigration::ReadText(stream, unit.message, MAX_TEXT)) return false;
		}
		return true;
	}
};

// LIVE_UPDATE_RETIRE: no payload. Chat hands its teams over first; the UGC server finishes the jobs it is running.
struct LiveUpdateRetire : public LUBitStream {
	LiveUpdateRetire() : LUBitStream(ServiceType::MASTER, MessageType::Master::LIVE_UPDATE_RETIRE) {}
};

// CHAT_SERVER_READY: no payload
struct ChatServerReady : public LUBitStream {
	ChatServerReady() : LUBitStream(ServiceType::MASTER, MessageType::Master::CHAT_SERVER_READY) {}
};

/**
 * CHAT_HANDOFF: the teams a retiring chat server had. Who is online is not carried: every world sends its players to
 * the new chat server again (CHAT_SERVER_READY), and friends lists are read from the database.
 */
struct ChatHandoff : public LUBitStream {
	ChatHandoff() : LUBitStream(ServiceType::MASTER, MessageType::Master::CHAT_HANDOFF) {}

	static constexpr uint32_t MAX_TEAMS = 20000;
	static constexpr uint8_t MAX_MEMBERS = 16;

	struct Team {
		LWOOBJID teamId{};
		LWOOBJID leaderId{};
		std::vector<LWOOBJID> members;
		uint8_t lootFlag{};
		bool local{};
		uint32_t zoneId{};
		uint32_t instanceId{};
		uint32_t cloneId{};
	};

	std::vector<Team> teams;

	void Serialize(RakNet::BitStream& stream) const override {
		const auto count = static_cast<uint32_t>(std::min<size_t>(teams.size(), MAX_TEAMS));
		stream.Write(count);
		for (uint32_t i = 0; i < count; i++) {
			const auto& team = teams[i];
			stream.Write(team.teamId);
			stream.Write(team.leaderId);
			const auto members = static_cast<uint8_t>(std::min<size_t>(team.members.size(), MAX_MEMBERS));
			stream.Write(members);
			for (uint8_t m = 0; m < members; m++) stream.Write(team.members[m]);
			stream.Write(team.lootFlag);
			stream.Write<uint8_t>(team.local ? 1 : 0);
			stream.Write(team.zoneId);
			stream.Write(team.instanceId);
			stream.Write(team.cloneId);
		}
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint32_t count{};
		if (!stream.Read(count) || count > MAX_TEAMS) return false;
		teams.resize(count);
		for (auto& team : teams) {
			uint8_t members{}, local{};
			if (!stream.Read(team.teamId) || !stream.Read(team.leaderId) || !stream.Read(members) || members > MAX_MEMBERS) return false;
			team.members.resize(members);
			for (auto& member : team.members) {
				if (!stream.Read(member)) return false;
			}
			if (!stream.Read(team.lootFlag) || !stream.Read(local) || !stream.Read(team.zoneId) || !stream.Read(team.instanceId) || !stream.Read(team.cloneId)) return false;
			team.local = local != 0;
		}
		return true;
	}
};

#endif  //!__LIVEUPDATE__H__
