#include "WorldMigration.h"

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
#include "PlayerManager.h"
#include "ServiceType.h"
#include "TradingManager.h"
#include "ClientPackets.h"
#include "dServer.h"
#include "eServerDisconnectIdentifiers.h"

using namespace InstanceMigration;

namespace {
	// Players who are dead or building wait this long to finish before they go anyway
	constexpr float MAX_WAIT_SECONDS = 15.0f;
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
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::MIGRATE_STATUS);
		status.Serialize(bitStream);
		Game::server->SendToMaster(bitStream);
	}

	void SendStatus(eState state, uint16_t remaining, const std::string& message) {
		if (g_Active) SendStatus(g_Active->order.migrationId, g_Active->order.targetInstance, state, g_Active->moved, g_Active->failed, remaining, message);
	}

	void WriteWString(RakNet::BitStream& bitStream, const std::u16string& text) {
		bitStream.Write<uint32_t>(text.size());
		for (const auto character : text) bitStream.Write<uint16_t>(character);
	}

	// LocalizedAnnouncementServerToSingleClient: the client looks both strings up in its locale (falling back to
	// the text itself) and shows the announcement popup, like its own instance-lock warning did in live
	void SendLocalizedAnnouncement(Entity* player, const std::u16string& body, const std::u16string& title) {
		const auto& sysAddr = player->GetSystemAddress();
		CBITSTREAM;
		CMSGHEADER;
		bitStream.Write(player->GetObjectID());
		bitStream.Write(MessageType::Game::LOCALIZED_ANNOUNCEMENT_SERVER_TO_SINGLE_CLIENT);
		bitStream.Write<uint32_t>(0); // body parameters (LDF), none
		bitStream.Write(false); // force open chat box
		bitStream.Write(true); // show the announcement
		bitStream.Write(true); // and put the text in chat
		WriteWString(bitStream, body);
		WriteWString(bitStream, title);
		bitStream.Write<uint32_t>(0); // title parameters (LDF), none
		SEND_PACKET;
	}

	uint16_t Remaining() {
		uint16_t remaining = 0;
		for (auto* player : PlayerManager::GetAllPlayers()) {
			if (player && !g_Leaving.contains(player->GetSystemAddress())) remaining++;
		}
		return remaining;
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
			CBITSTREAM;
			BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::MIGRATE_PLAYER_STATE);
			carried.Serialize(bitStream);
			Game::server->SendToMaster(bitStream);
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
		// Copy: moving a player can change the list (a cancelled trade, a pet)
		const auto players = PlayerManager::GetAllPlayers();
		for (auto* player : players) {
			if (!player || g_Leaving.contains(player->GetSystemAddress())) continue;
			if (active.budget < 1.0f) break;
			auto* character = player->GetCharacter();
			const bool inTrade = TradingManager::Instance()->GetPlayerTrade(player->GetObjectID()) != nullptr;
			auto& waited = active.waiting[player->GetObjectID()];
			const auto decision = DecidePlayer(inTrade, character && character->GetBuildMode(), player->GetIsDead(), waited, MAX_WAIT_SECONDS);
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

	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::INSTANCE_MIGRATE);
	request.Serialize(bitStream);
	Game::server->SendToMaster(bitStream);
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
