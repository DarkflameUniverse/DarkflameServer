#include "WorldMigration.h"
#include "ZoneMessages.h"
#include "MasterPackets.h"

#include <ctime>
#include <map>
#include <optional>

#include "BitStreamUtils.h"
#include "ChatPackets.h"
#include "GeneralUtils.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "Entity.h"
#include "EntityManager.h"
#include "GameMessageHandler.h"
#include "Game.h"
#include "InventoryComponent.h"
#include "Logger.h"
#include "MessageType/Game.h"
#include "MessageType/Master.h"
#include "PetComponent.h"
#include "PropertyManagementComponent.h"
#include "ControllablePhysicsComponent.h"
#include "UserManager.h"
#include "PlayerManager.h"
#include "ServiceType.h"
#include "TradingManager.h"
#include "ClientPackets.h"
#include "dServer.h"
#include "eServerDisconnectIdentifiers.h"

using namespace InstanceMigration;

namespace {
	// A client that got its transfer and is still connected after this is dropped (it is saved already)
	constexpr float LEAVE_TIMEOUT_SECONDS = 20.0f;
	// Nobody left to move for this long: done (players still loading in finish first)
	constexpr float EMPTY_SECONDS = 3.0f;
	// Give up on a migration after this long moving; whoever is left stays (or is saved when the world shuts down)
	constexpr float MAX_MOVE_SECONDS = 150.0f;
	constexpr float STATUS_INTERVAL = 1.0f;
	// Carried state for a player who never arrives
	constexpr float CARRIED_EXPIRY_SECONDS = 300.0f;

	struct Leaving {
		LWOOBJID characterId{};
		float since{};
	};

	struct Active {
		MigratePlayersOrder order;
		float warnLeft{};
		float moving{};
		float budget{};
		float statusIn{};
		float emptyFor{};
		uint16_t moved{};
		uint16_t failed{};
		std::map<LWOOBJID, float> waiting; // players who can't go yet, and for how long
	};

	std::optional<Active> g_Active;

	// A property being saved before its new instance loads it (MIGRATE_PREPARE)
	struct Preparing {
		uint32_t migrationId{};
		float waitLeft{};
		bool told{};
	};
	std::optional<Preparing> g_Preparing;
	// The migration this property was frozen for (0: not frozen)
	uint32_t g_FrozenFor = 0;
	std::string g_LiveUpdateTold; // the last live update summary the GM who asked was told
	std::map<SystemAddress, Leaving> g_Leaving;
	std::map<LWOOBJID, std::pair<CarriedPlayerState, float>> g_Carried;
	std::function<void(const SystemAddress&)> g_Cleanup;
	std::map<uint32_t, eState> g_ToldRequester; // the last state the GM who asked was told, per migration

	void SendStatus(uint32_t migrationId, uint32_t targetInstance, eState state, uint16_t moved, uint16_t failed, uint16_t remaining, const std::string& message) {
		MigrationStatus status;
		status.migrationId = migrationId;
		status.state = state;
		status.zoneId = Game::server->GetZoneID();
		status.sourceInstance = Game::server->GetInstanceID();
		status.targetInstance = targetInstance;
		status.moved = moved;
		status.failed = failed;
		status.remaining = remaining;
		status.message = message;
		MasterPackets::SendToMaster(status);
	}

	void SendStatus(eState state, uint16_t remaining, const std::string& message) {
		if (g_Active) SendStatus(g_Active->order.migrationId, g_Active->order.targetInstance, state, g_Active->moved, g_Active->failed, remaining, message);
	}

	// LocalizedAnnouncementServerToSingleClient: the client looks both strings up in its locale (falling back to
	// the text itself) and shows the announcement popup, like its own instance-lock warning did in live
	void SendLocalizedAnnouncement(Entity* player, const std::u16string& body, const std::u16string& title) {
		GameMessages::LocalizedAnnouncementServerToSingleClient announcement;
		announcement.target = player->GetObjectID();
		announcement.body = body;
		announcement.title = title;
		announcement.SendToClient(player->GetSystemAddress());
	}

	// Character selection has users but no player entities
	bool IsCharacterSelect() {
		return Game::server->GetZoneID() == 0;
	}

	uint16_t Remaining() {
		uint16_t remaining = 0;
		if (IsCharacterSelect()) {
			for (const auto& [sysAddr, user] : UserManager::Instance()->GetUsers()) {
				if (user && !g_Leaving.contains(sysAddr)) remaining++;
			}
			return remaining;
		}
		for (auto* player : PlayerManager::GetAllPlayers()) {
			if (player && !g_Leaving.contains(player->GetSystemAddress())) remaining++;
		}
		return remaining;
	}

	// Someone at character selection: nothing to save, they just connect to the new one, which sends them their
	// characters again
	void MoveUser(const SystemAddress& sysAddr) {
		const auto& order = g_Active->order;
		g_Leaving[sysAddr] = { LWOOBJID_EMPTY, 0.0f };
		ClientPackets::TransferToWorld transfer;
		transfer.serverIP = LUString(order.targetIp);
		transfer.serverPort = order.targetPort;
		transfer.mythranShift = order.mythranShift;
		transfer.Send(sysAddr);
		LOG("Migration %u: sent %s from character selection to instance %u", order.migrationId, sysAddr.ToString(), order.targetInstance);
	}

	// Save, lock and send one player over. Returns false when they couldn't be sent.
	bool MovePlayer(Entity* player) {
		auto* character = player->GetCharacter();
		if (!character) return false;
		const auto& order = g_Active->order;
		const auto sysAddr = player->GetSystemAddress();
		const auto playerId = player->GetObjectID();

		// Nothing changes hands in a cancelled trade
		if (const auto& trade = TradingManager::Instance()->GetPlayerTrade(playerId)) {
			TradingManager::Instance()->CancelTrade(playerId, trade->GetTradeId());
		}

		CarriedPlayerState carried;
		carried.targetZone = order.targetZone;
		carried.targetInstance = order.targetInstance;
		carried.characterId = playerId;
		carried.seamless = order.seamless;
		if (auto* pet = PetComponent::GetActivePet(playerId)) carried.petItemId = pet->GetItemId();
		// Where they stand: the saved character doesn't keep it on properties (or Moon Base)
		if (auto* physics = player->GetComponent<ControllablePhysicsComponent>()) {
			const auto& position = physics->GetPosition();
			const auto& rotation = physics->GetRotation();
			carried.hasPosition = true;
			carried.x = position.x;
			carried.y = position.y;
			carried.z = position.z;
			carried.rotW = rotation.w;
			carried.rotX = rotation.x;
			carried.rotY = rotation.y;
			carried.rotZ = rotation.z;
		}

		// Same zone, so they spawn where they stood (Entity.cpp only uses a spawn point when the zone changes)
		character->SetZoneID(order.targetZone);
		character->SetZoneInstance(order.targetInstance);
		character->SetZoneClone(order.targetClone);
		character->SetTargetScene("");
		if (auto* characterComponent = player->GetComponent<CharacterComponent>()) characterComponent->SetLastRocketConfig(u"");
		character->SaveXMLToDatabase();

		// From here on nothing they do counts: the target loads what was just saved
		g_Leaving[sysAddr] = { playerId, 0.0f };

		{
			MasterPackets::SendToMaster(carried);
		}

		// Seamless: take every object we sent away first. The client deletes them but keeps its own player object when
		// that ghost goes (LWOGhostComponent::OnGhostReceiveDestruction), and the target's construction of the
		// player is adopted by it. Same ordered channel as the transfer, so these arrive before it.
		if (order.seamless) Game::entityManager->DestructAllEntities(sysAddr);

		ClientPackets::TransferToWorld transfer;
		transfer.serverIP = LUString(order.targetIp);
		transfer.serverPort = order.targetPort;
		transfer.mythranShift = order.mythranShift;
		transfer.Send(sysAddr);
		LOG("Migration %u: sent %s (%llu) to instance %u", order.migrationId, character->GetName().c_str(), playerId, order.targetInstance);
		return true;
	}

	void MovePlayers(float deltaTime) {
		auto& active = *g_Active;
		active.budget = std::min(active.budget + active.order.playersPerSecond * deltaTime, static_cast<float>(active.order.playersPerSecond));
		if (IsCharacterSelect()) {
			std::vector<SystemAddress> users;
			for (const auto& [sysAddr, user] : UserManager::Instance()->GetUsers()) {
				if (user && !g_Leaving.contains(sysAddr)) users.push_back(sysAddr);
			}
			for (const auto& sysAddr : users) {
				if (active.budget < 1.0f) break;
				active.budget -= 1.0f;
				MoveUser(sysAddr);
				active.moved++;
			}
			return;
		}
		// Copy: moving a player can change the list (a cancelled trade, a pet)
		const auto players = PlayerManager::GetAllPlayers();
		for (auto* player : players) {
			if (!player || g_Leaving.contains(player->GetSystemAddress())) continue;
			if (active.budget < 1.0f) break;
			auto* character = player->GetCharacter();
			const bool inTrade = TradingManager::Instance()->GetPlayerTrade(player->GetObjectID()) != nullptr;
			auto& waited = active.waiting[player->GetObjectID()];
			const auto decision = DecidePlayer(inTrade, character && character->GetBuildMode(), player->GetIsDead(), waited, active.order.maxWaitSeconds);
			if (decision == ePlayerDecision::WAIT) {
				waited += deltaTime;
				continue;
			}
			active.budget -= 1.0f;
			active.waiting.erase(player->GetObjectID());
			if (MovePlayer(player)) active.moved++;
			else active.failed++;
		}
	}

	void DropStuckClients(float deltaTime) {
		std::vector<SystemAddress> stuck;
		for (auto& [sysAddr, leaving] : g_Leaving) {
			leaving.since += deltaTime;
			if (leaving.since > LEAVE_TIMEOUT_SECONDS) stuck.push_back(sysAddr);
		}
		for (const auto& sysAddr : stuck) {
			LOG("Migration: %llu got its transfer but never left; disconnecting it (it is saved)", g_Leaving[sysAddr].characterId);
			if (g_Active) g_Active->failed++;
			if (g_Cleanup) g_Cleanup(sysAddr);
			Game::server->Disconnect(sysAddr, eServerDisconnectIdentifiers::SERVER_SHUTDOWN);
			g_Leaving.erase(sysAddr);
		}
	}
}

void WorldMigration::SetCleanupHandler(std::function<void(const SystemAddress&)> handler) {
	g_Cleanup = std::move(handler);
}

void WorldMigration::HandleOrder(const MigratePlayersOrder& order) {
	if (order.targetPort == 0) {
		if (g_Preparing && g_Preparing->migrationId == order.migrationId) {
			LOG("Migration %u cancelled while the property was being saved", order.migrationId);
			g_Preparing.reset();
		}
		// A property nobody went to the new instance of is ours again; once someone did, it stays with them
		if (g_FrozenFor == order.migrationId && (!g_Active || g_Active->moved == 0)) {
			if (auto* property = PropertyManagementComponent::Instance()) property->Unfreeze();
			g_FrozenFor = 0;
		}
		if (!g_Active || g_Active->order.migrationId != order.migrationId) return;
		LOG("Migration %u cancelled; %u player(s) were moved", order.migrationId, g_Active->moved);
		SendStatus(eState::FAILED, Remaining(), "Cancelled");
		g_Active.reset();
		return;
	}
	if (g_Active) {
		LOG("Migration %u refused: migration %u is still running here", order.migrationId, g_Active->order.migrationId);
		SendStatus(order.migrationId, order.targetInstance, eState::FAILED, 0, 0, Remaining(), "Another migration is running on this instance");
		return;
	}

	g_Active = Active{};
	g_Active->order = order;
	g_Active->warnLeft = order.warnSeconds;
	LOG("Migration %u: moving everyone to %s:%u (zone %u instance %u) in %u s", order.migrationId, order.targetIp.c_str(), order.targetPort,
		order.targetZone, order.targetInstance, order.warnSeconds);
	if (order.warnSeconds > 0) {
		for (auto* player : PlayerManager::GetAllPlayers()) {
			if (player) SendLocalizedAnnouncement(player, u"UI_INSTANCE_LOCKED_ANNOUNCE_BODY", u"UI_INSTANCE_LOCKED_ANNOUNCE_TITLE");
		}
	}
}

void WorldMigration::HandlePrepare(const MigratePrepare& prepare) {
	if (g_Active || g_Preparing) {
		LOG("Migration %u refused: another migration is running here", prepare.migrationId);
		SendStatus(prepare.migrationId, 0, eState::FAILED, 0, 0, Remaining(), "Another migration is running on this instance");
		return;
	}
	g_Preparing = Preparing{ prepare.migrationId, static_cast<float>(prepare.maxWaitSeconds), false };
	LOG("Migration %u: saving this instance's property for its replacement (waiting up to %u s for builders)", prepare.migrationId, prepare.maxWaitSeconds);
}

void WorldMigration::ApplyCarriedPosition(Entity* player) {
	if (!player) return;
	const auto it = g_Carried.find(player->GetObjectID());
	if (it == g_Carried.end() || !it->second.first.hasPosition) return;
	const auto& state = it->second.first;
	auto* physics = player->GetComponent<ControllablePhysicsComponent>();
	if (!physics) return;
	physics->SetPosition(NiPoint3(state.x, state.y, state.z));
	physics->SetRotation(NiQuaternion(state.rotW, state.rotX, state.rotY, state.rotZ));
}

void WorldMigration::StoreCarriedState(const CarriedPlayerState& state) {
	if (state.targetZone != Game::server->GetZoneID() || state.targetInstance != Game::server->GetInstanceID()) return;
	g_Carried[state.characterId] = { state, 0.0f };
}

void WorldMigration::Update(float deltaTime) {
	for (auto it = g_Carried.begin(); it != g_Carried.end();) {
		it->second.second += deltaTime;
		if (it->second.second > CARRIED_EXPIRY_SECONDS) it = g_Carried.erase(it);
		else ++it;
	}
	DropStuckClients(deltaTime);

	if (g_Preparing) {
		auto* property = PropertyManagementComponent::Instance();
		if (property && property->GetBuilderCount() > 0 && g_Preparing->waitLeft > 0.0f) {
			if (!g_Preparing->told) {
				g_Preparing->told = true;
				const auto seconds = std::to_string(static_cast<int>(g_Preparing->waitLeft));
				for (auto* player : PlayerManager::GetAllPlayers()) {
					if (player) ChatPackets::SendSystemMessage(player->GetSystemAddress(),
						GeneralUtils::ASCIIToUTF16("The server is being updated: building on this property ends in " + seconds + " seconds."));
				}
			}
			g_Preparing->waitLeft -= deltaTime;
		} else {
			const auto migrationId = g_Preparing->migrationId;
			g_Preparing.reset();
			if (property) property->FreezeForHandOff();
			g_FrozenFor = migrationId;
			SendStatus(migrationId, 0, eState::PREPARED, 0, 0, Remaining(), property ? "Property saved" : "Nothing to save");
		}
	}

	if (!g_Active) return;

	auto& active = *g_Active;
	if (active.warnLeft > 0.0f) {
		active.warnLeft -= deltaTime;
		if (active.warnLeft > 0.0f) return;
		SendStatus(eState::MOVING, Remaining(), "Moving players");
	}

	active.moving += deltaTime;
	MovePlayers(deltaTime);

	const auto remaining = Remaining();
	active.emptyFor = remaining == 0 ? active.emptyFor + deltaTime : 0.0f;
	// Done once nobody is left to move and everyone sent away has disconnected (or was dropped)
	if (active.emptyFor >= EMPTY_SECONDS && g_Leaving.empty()) {
		SendStatus(eState::DONE, 0, std::to_string(active.moved) + " player(s) moved" +
			(active.failed ? ", " + std::to_string(active.failed) + " could not be moved" : ""));
		LOG("Migration %u done: %u moved, %u failed", active.order.migrationId, active.moved, active.failed);
		g_Active.reset();
		return;
	}
	if (active.moving > MAX_MOVE_SECONDS) {
		active.failed += remaining;
		SendStatus(eState::DONE, remaining, std::to_string(active.moved) + " player(s) moved; gave up on " + std::to_string(remaining));
		g_Active.reset();
		return;
	}

	active.statusIn -= deltaTime;
	if (active.statusIn <= 0.0f) {
		active.statusIn = STATUS_INTERVAL;
		SendStatus(eState::MOVING, remaining, "");
	}
}

bool WorldMigration::IsLeaving(const SystemAddress& sysAddr) {
	return g_Leaving.contains(sysAddr);
}

void WorldMigration::OnDisconnected(const SystemAddress& sysAddr) {
	g_Leaving.erase(sysAddr);
}

bool WorldMigration::ArrivesSeamlessly(LWOOBJID characterId) {
	const auto it = g_Carried.find(characterId);
	return it != g_Carried.end() && it->second.first.seamless;
}

void WorldMigration::OnSeamlessArrival(Entity* player) {
	if (!player) return;
	const auto playerId = player->GetObjectID();
	const auto sysAddr = player->GetSystemAddress();
	LOG("Player %llu arrived without reloading the zone", playerId);
	// The client already finished loading on the old instance and won't send PlayerLoaded again: do what it would
	// trigger, once the constructions have had a moment to arrive
	player->AddCallbackTimer(1.0f, [playerId, sysAddr]() {
		if (!Game::entityManager->GetEntity(playerId)) return;
		RakNet::BitStream empty;
		GameMessageHandler::HandleMessage(empty, sysAddr, playerId, MessageType::Game::PLAYER_LOADED);
	});
}

void WorldMigration::OnPlayerLoaded(Entity* player) {
	if (!player) return;
	const auto it = g_Carried.find(player->GetObjectID());
	if (it == g_Carried.end()) return;
	const auto state = it->second.first;
	g_Carried.erase(it);

	LOG("Player %llu arrived from a migrated instance", state.characterId);
	if (state.petItemId != LWOOBJID_EMPTY) {
		auto* inventory = player->GetComponent<InventoryComponent>();
		auto* item = inventory ? inventory->FindItemById(state.petItemId) : nullptr;
		if (item) inventory->SpawnPet(item);
	}
}

void WorldMigration::HandleStatus(const MigrationStatus& status) {
	if (status.requesterId == LWOOBJID_EMPTY) return;
	auto* requester = PlayerManager::GetPlayer(status.requesterId);
	if (!requester) return;
	// Each change of state once, not every progress report
	const auto told = g_ToldRequester.find(status.migrationId);
	if (told != g_ToldRequester.end() && told->second == status.state) return;
	g_ToldRequester[status.migrationId] = status.state;
	if (status.Finished()) g_ToldRequester.erase(status.migrationId);

	std::string text = "Instance " + std::string(KindName(status.kind)) + " (zone " + std::to_string(status.zoneId) + " instance " +
		std::to_string(status.sourceInstance) + (status.targetInstance ? " -> " + std::to_string(status.targetInstance) : std::string()) + "): " + StateName(status.state);
	if (status.moved || status.failed) text += ", " + std::to_string(status.moved) + " moved, " + std::to_string(status.failed) + " failed";
	if (!status.message.empty()) text += ". " + status.message;
	ChatPackets::SendSystemMessage(requester->GetSystemAddress(), GeneralUtils::ASCIIToUTF16(text));
}

void WorldMigration::RequestMigration(Entity* requester, eKind kind, uint32_t targetInstance, uint16_t warnSeconds, bool seamless) {
	static uint32_t nextId = static_cast<uint32_t>(std::time(nullptr));
	InstanceMigrationRequest request;
	// Different worlds may ask at the same time; master keys migrations by this
	request.requestId = (++nextId << 8) ^ static_cast<uint32_t>(Game::server->GetInstanceID());
	request.kind = kind;
	request.zoneId = Game::server->GetZoneID();
	request.sourceInstance = Game::server->GetInstanceID();
	request.targetInstance = kind == eKind::MERGE ? targetInstance : 0;
	request.warnSeconds = std::min(warnSeconds, InstanceMigrationRequest::MAX_WARN_SECONDS);
	request.seamless = seamless;
	if (requester) {
		request.requesterId = requester->GetObjectID();
		if (auto* character = requester->GetCharacter()) request.requestedBy = character->GetName();
	}
	LOG("Asking master to %s this instance (migration %u, requested by %s)", KindName(kind), request.requestId, request.requestedBy.c_str());

	MasterPackets::SendToMaster(request);
}

namespace {
	// [warn seconds] [seamless] from the words after the first `skip`
	bool ParseMoveOptions(const std::vector<std::string>& words, size_t skip, uint16_t& warnSeconds, bool& seamless) {
		warnSeconds = 10;
		seamless = false;
		if (words.size() > skip) {
			const auto warn = GeneralUtils::TryParse<uint16_t>(words[skip]);
			if (!warn || *warn > InstanceMigrationRequest::MAX_WARN_SECONDS) return false;
			warnSeconds = *warn;
		}
		if (words.size() > skip + 1) {
			if (words[skip + 1] != "seamless") return false;
			seamless = true;
		}
		return true;
	}
}

void WorldMigration::ReplaceInstanceCommand(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	const auto words = GeneralUtils::SplitString(args, ' ');
	uint16_t warnSeconds{};
	bool seamless{};
	if (!ParseMoveOptions(words, words.size() == 1 && words[0].empty() ? 1 : 0, warnSeconds, seamless)) {
		ChatPackets::SendSystemMessage(sysAddr, u"Usage: /replaceinstance [warn seconds, 0-300] [seamless]");
		return;
	}
	ChatPackets::SendSystemMessage(sysAddr, u"Asking master to replace this instance with a fresh one.");
	RequestMigration(entity, eKind::REPLACE, 0, warnSeconds, seamless);
}

void WorldMigration::MergeInstanceCommand(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	const auto words = GeneralUtils::SplitString(args, ' ');
	uint32_t target = 0;
	if (!words.empty() && !words[0].empty()) {
		const auto parsed = GeneralUtils::TryParse<uint32_t>(words[0]);
		if (!parsed) {
			ChatPackets::SendSystemMessage(sysAddr, u"Usage: /mergeinstance [target instance, 0 for the best fit] [warn seconds, 0-300] [seamless]");
			return;
		}
		target = *parsed;
	}
	uint16_t warnSeconds{};
	bool seamless{};
	if (!ParseMoveOptions(words, 1, warnSeconds, seamless)) {
		ChatPackets::SendSystemMessage(sysAddr, u"Usage: /mergeinstance [target instance, 0 for the best fit] [warn seconds, 0-300] [seamless]");
		return;
	}
	if (target == static_cast<uint32_t>(Game::server->GetInstanceID())) {
		ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(Describe(eRefusal::TARGET_IS_SOURCE)));
		return;
	}
	ChatPackets::SendSystemMessage(sysAddr, u"Asking master to merge this instance into " +
		(target ? u"instance " + GeneralUtils::ASCIIToUTF16(std::to_string(target)) : std::u16string(u"the best fit")) + u".");
	RequestMigration(entity, eKind::MERGE, target, warnSeconds, seamless);
}

namespace {
	std::string LiveUpdateSummary(const LiveUpdateStatus& status) {
		using namespace LiveUpdate;
		size_t done = 0, failed = 0;
		for (const auto& unit : status.units) {
			if (IsFinished(unit.state)) done++;
			if (unit.state == eUnitState::FAILED) failed++;
		}
		std::string text = "Live update " + std::to_string(status.updateId) + ": " + PhaseName(status.phase);
		if (!status.units.empty()) text += ", " + std::to_string(done) + " of " + std::to_string(status.units.size()) + " done";
		if (failed) text += ", " + std::to_string(failed) + " failed";
		if (!status.message.empty()) text += ". " + status.message;
		return text;
	}
}

void WorldMigration::LiveUpdateCommand(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	const auto words = GeneralUtils::SplitString(args, ' ');
	const std::string verb = words.empty() || words[0].empty() ? "status" : words[0];
	LiveUpdateRequest request;
	if (verb == "start") {
		request.action = LiveUpdate::eAction::START;
		if (words.size() > 1) {
			const auto warn = GeneralUtils::TryParse<int32_t>(words[1]);
			if (!warn || *warn < 0 || *warn > InstanceMigrationRequest::MAX_WARN_SECONDS) {
				ChatPackets::SendSystemMessage(sysAddr, u"Usage: /liveupdate start [warn seconds, 0-300]");
				return;
			}
			request.warnSeconds = *warn;
		}
	} else if (verb == "cancel") {
		request.action = LiveUpdate::eAction::CANCEL;
	} else if (verb == "status") {
		request.action = LiveUpdate::eAction::STATUS;
	} else {
		ChatPackets::SendSystemMessage(sysAddr, u"Usage: /liveupdate [start [warn seconds] | cancel | status]");
		return;
	}
	if (entity) {
		request.requesterId = entity->GetObjectID();
		if (auto* character = entity->GetCharacter()) request.requestedBy = character->GetName();
	}
	g_LiveUpdateTold.clear();
	LOG("Asking master to %s a live update (requested by %s)", verb.c_str(), request.requestedBy.c_str());
	MasterPackets::SendToMaster(request);
	if (request.action == LiveUpdate::eAction::START) ChatPackets::SendSystemMessage(sysAddr, u"Asking master to move everything onto the binaries on disk now.");
}

void WorldMigration::HandleLiveUpdateStatus(const LiveUpdateStatus& status) {
	if (status.requesterId == LWOOBJID_EMPTY) return;
	auto* requester = PlayerManager::GetPlayer(status.requesterId);
	if (!requester) return;
	const auto text = LiveUpdateSummary(status);
	if (text == g_LiveUpdateTold) return;
	g_LiveUpdateTold = text;
	ChatPackets::SendSystemMessage(requester->GetSystemAddress(), GeneralUtils::ASCIIToUTF16(text));
}
