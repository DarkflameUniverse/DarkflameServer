#include "UgcManifest.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

#include "BrickByBrick.h"
#include "BuildingMessages.h"
#include "ClientPackets.h"
#include "Database.h"
#include "dConfig.h"
#include "eBlueprintSaveResponseType.h"
#include "Entity.h"
#include "EntityManager.h"
#include "GhostComponent.h"
#include "Character.h"
#include "Game.h"
#include "Logger.h"
#include "MD5.h"
#include "PlayerManager.h"
#include "Sd0.h"
#include "dZoneManager.h"
#include "ChatPackets.h"
#include "ZoneInstanceManager.h"

namespace {
	// How often the waiting requests are looked up again, how long they wait at most and how many there can be
	constexpr auto RETRY_INTERVAL = std::chrono::seconds(5);
	constexpr auto MAX_WAIT = std::chrono::minutes(15);
	constexpr size_t MAX_WAITING = 512;
	// A client asks for a model's NIF, HKX and LXFML at once: its LXFML is sent once for all three
	constexpr auto LXFML_RESEND_AFTER = std::chrono::seconds(10);
	// LXFML checksums worked out, kept for the next clients (a model's LXFML only changes with a new save)
	constexpr size_t MAX_LXFML_CHECKSUMS = 4096;
	// How long after NotifyClientUGCModelReady the model is constructed again: the client flushes the model's files on
	// its load thread, and a model constructed before that is done loses the mesh it just loaded
	constexpr auto RECONSTRUCT_DELAY = std::chrono::milliseconds(1500);
	// How long after taking the model down it is constructed again: the client deletes objects later, and a
	// construction of an object it still has is dropped (the model then disappears)
	constexpr auto CONSTRUCT_AFTER_DESTRUCT = std::chrono::milliseconds(1000);

	struct Waiting {
		SystemAddress sysAddr;
		LWOOBJID blueprintId{};
		eUgcResourceType resourceType{};
		std::chrono::steady_clock::time_point since;
	};

	std::vector<Waiting> g_Waiting;
	std::chrono::steady_clock::time_point g_NextRetry{};
	// LXFMLs sent to clients (building those models) and the switches to served meshes waiting for them
	UgcManifest::ServedMeshSwitches g_Switches;
	std::map<LWOOBJID, IUgc::FileChecksum> g_LxfmlChecksums;

	// Placed models to construct again for a client once its flush is done
	struct Reconstruct {
		SystemAddress sysAddr;
		LWOOBJID objectId{};
		std::chrono::steady_clock::time_point due;
		bool destructed{}; // taken down; constructed at `due`
	};
	std::vector<Reconstruct> g_Reconstructs;

	// /reprocessproperty: the models being made again, and when the players are reloaded at the latest
	struct PropertyReload {
		std::set<LWOOBJID> blueprintIds;
		std::chrono::steady_clock::time_point since;
		std::chrono::steady_clock::time_point nextCheck;
	};
	std::optional<PropertyReload> g_PropertyReload;

	void RunDueSwitches(std::chrono::steady_clock::time_point now);

	bool ManifestOn() {
		// Off unless set: a client whose boot.cfg doesn't point it at the UGC server downloads from its built-in
		// http://127.0.0.1:80/lwoclient and is logged out when it can't connect there (docs/UgcServer.md)
		return Game::config && Game::config->GetValue("ugc_manifest") == "1";
	}

	bool ModelsOn() {
		return Game::config && Game::config->GetValue("ugc_manifest_models") == "1";
	}

	int HexDigit(char c) {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	}

	bool MeshMade(LWOOBJID blueprintId) {
		const auto checksum = Database::Get()->GetUgcFileChecksum(blueprintId, "model.nif");
		return checksum && checksum->md5.size() == 32;
	}

	void Send(const SystemAddress& sysAddr, LWOOBJID blueprintId, eUgcResourceType resourceType, const std::optional<IUgc::FileChecksum>& checksum) {
		ClientPackets::UgcManifestResponse response;
		response.blueprintId = blueprintId;
		response.resourceType = resourceType;
		if (checksum && checksum->md5.size() == 32) {
			bool valid = true;
			for (size_t i = 0; i < response.md5.size(); i++) {
				const auto high = HexDigit(checksum->md5[i * 2]);
				const auto low = HexDigit(checksum->md5[i * 2 + 1]);
				if (high < 0 || low < 0) valid = false;
				response.md5[i] = static_cast<uint8_t>((high << 4) | low);
			}
			if (valid) {
				response.valid = true;
				response.fileSize = checksum->size;
			} else {
				response.md5 = {};
			}
		}
		response.Send(sysAddr);
		LOG_DEBUG("Sent the UGC manifest of %llu (type %i): %s, %u bytes", static_cast<unsigned long long>(blueprintId),
			static_cast<int>(resourceType), response.valid ? checksum->md5.c_str() : "not known", response.fileSize);
	}

	// Answers an icon's request when its checksum is known; false when it isn't (yet)
	bool TryAnswerIcon(const SystemAddress& sysAddr, LWOOBJID blueprintId, eUgcResourceType resourceType) {
		const auto checksum = Database::Get()->GetUgcFileChecksum(blueprintId, "icon.dds");
		if (!checksum || checksum->md5.size() != 32) return false;
		Send(sysAddr, blueprintId, resourceType, checksum);
		return true;
	}

	std::optional<IUgc::FileChecksum> StoredLxfmlChecksum(LWOOBJID blueprintId) {
		if (const auto known = g_LxfmlChecksums.find(blueprintId); known != g_LxfmlChecksums.end()) return known->second;
		const auto model = Database::Get()->GetUgcModel(blueprintId);
		if (!model) return std::nullopt;
		auto checksum = UgcManifest::LxfmlChecksum(model->lxfmlData.str());
		if (!checksum) return std::nullopt;
		if (g_LxfmlChecksums.size() >= MAX_LXFML_CHECKSUMS) g_LxfmlChecksums.clear();
		g_LxfmlChecksums.emplace(blueprintId, *checksum);
		return checksum;
	}

	// The model's LXFML to one client, which builds the model from it (as when a property loads); false when there is
	// no such model
	bool SendLxfml(const SystemAddress& sysAddr, LWOOBJID blueprintId) {
		const auto now = std::chrono::steady_clock::now();
		if (g_Switches.SentWithin(sysAddr, blueprintId, LXFML_RESEND_AFTER, now)) return true;

		auto model = Database::Get()->GetUgcModel(blueprintId);
		if (!model) return false;
		ClientPackets::BlueprintSaveResponse response;
		response.localId = LWOOBJID_EMPTY; // zero, like the LXFML sent when a property loads
		response.reasonCode = eBlueprintSaveResponseType::EverythingWorked;
		response.models.push_back({ blueprintId, model->lxfmlData.str() });
		response.Send(sysAddr);
		g_Switches.LxfmlSent(sysAddr, blueprintId, now);
		// Its mesh is wanted: a model still in its quiet period after a save is made now
		Database::Get()->ExpediteUgcModel(blueprintId);
		LOG_DEBUG("Sent the LXFML of %llu for the client to build", static_cast<unsigned long long>(blueprintId));
		return true;
	}
}

void UgcManifest::ServedMeshSwitches::LxfmlSent(const SystemAddress& sysAddr, const LWOOBJID blueprintId, const Clock::time_point now) {
	std::erase_if(m_LxfmlSent, [now](const auto& sent) { return now - sent.second >= BUILD_SETTLE; });
	const Key key{ sysAddr, blueprintId };
	m_LxfmlSent[key] = now;
	// A switch now would be undone by the build this starts
	if (const auto due = m_Due.find(key); due != m_Due.end()) due->second = std::max(due->second, now + BUILD_SETTLE);
}

bool UgcManifest::ServedMeshSwitches::SentWithin(const SystemAddress& sysAddr, const LWOOBJID blueprintId, const Clock::duration within, const Clock::time_point now) const {
	const auto sent = m_LxfmlSent.find({ sysAddr, blueprintId });
	return sent != m_LxfmlSent.end() && now - sent->second < within;
}

void UgcManifest::ServedMeshSwitches::Schedule(const SystemAddress& sysAddr, const LWOOBJID blueprintId, const Clock::time_point now) {
	const Key key{ sysAddr, blueprintId };
	auto due = now;
	if (const auto sent = m_LxfmlSent.find(key); sent != m_LxfmlSent.end()) due = std::max(due, sent->second + BUILD_SETTLE);
	if (const auto pending = m_Due.find(key); pending != m_Due.end()) due = std::max(due, pending->second);
	m_Due[key] = due;
}

std::vector<UgcManifest::ServedMeshSwitches::Switch> UgcManifest::ServedMeshSwitches::TakeDue(const Clock::time_point now) {
	std::vector<Switch> due;
	std::erase_if(m_Due, [&](const auto& pending) {
		if (pending.second > now) return false;
		due.push_back({ pending.first.first, pending.first.second });
		return true;
	});
	return due;
}

void UgcManifest::ServedMeshSwitches::Forget(const SystemAddress& sysAddr) {
	std::erase_if(m_LxfmlSent, [&sysAddr](const auto& sent) { return sent.first.first == sysAddr; });
	std::erase_if(m_Due, [&sysAddr](const auto& pending) { return pending.first.first == sysAddr; });
}

UgcManifest::eAction UgcManifest::Decide(const eUgcResourceType type, const bool manifestOn, const bool modelsOn, const bool meshMade) {
	if (!manifestOn) return eAction::NONE;
	switch (type) {
	case eUgcResourceType::DDS: return eAction::WAIT; // answered at once when the icon is made
	case eUgcResourceType::NIF:
	case eUgcResourceType::LXFML:
		return modelsOn && meshMade ? eAction::ANSWER : eAction::SEND_LXFML;
	case eUgcResourceType::HKX:
		// The UGC server makes no physics: the client builds it from the LXFML (and is then switched to a served mesh)
		return eAction::SEND_LXFML;
	default: return eAction::NONE;
	}
}

std::optional<IUgc::FileChecksum> UgcManifest::LxfmlChecksum(const std::string& stored) {
	std::string lxfml;
	if (stored.starts_with("<?xml") || stored.starts_with("<LXFML")) {
		lxfml = stored;
	} else {
		try {
			std::stringstream stream(stored);
			Sd0 sd0(stream);
			lxfml = sd0.GetAsStringUncompressed();
		} catch (...) {
			return std::nullopt;
		}
	}
	if (lxfml.empty()) return std::nullopt;
	MD5 md5;
	md5.update(reinterpret_cast<const unsigned char*>(lxfml.data()), static_cast<MD5::size_type>(lxfml.size()));
	md5.finalize();
	return IUgc::FileChecksum{ md5.hexdigest(), static_cast<uint32_t>(lxfml.size()) };
}

bool UgcManifest::ServesModels() {
	return ManifestOn() && ModelsOn();
}

bool UgcManifest::ServesMesh(const LWOOBJID blueprintId) {
	return ServesModels() && MeshMade(blueprintId);
}

void UgcManifest::OnRequest(const SystemAddress& sysAddr, LWOOBJID blueprintId, eUgcResourceType resourceType) {
	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) return;
	const auto manifestOn = ManifestOn();
	const auto modelsOn = ModelsOn();
	const bool isModelFile = resourceType == eUgcResourceType::NIF || resourceType == eUgcResourceType::HKX || resourceType == eUgcResourceType::LXFML;
	const auto meshMade = manifestOn && modelsOn && isModelFile && MeshMade(blueprintId);

	switch (Decide(resourceType, manifestOn, modelsOn, meshMade)) {
	case eAction::NONE: return;
	case eAction::ANSWER:
		Send(sysAddr, blueprintId, resourceType, resourceType == eUgcResourceType::LXFML
			? StoredLxfmlChecksum(blueprintId) : Database::Get()->GetUgcFileChecksum(blueprintId, "model.nif"));
		return;
	case eAction::ANSWER_UNKNOWN:
		Send(sysAddr, blueprintId, resourceType, std::nullopt);
		return;
	case eAction::SEND_LXFML:
		// Not a player model (or gone): answered as not known, so the client doesn't wait for ever
		if (!SendLxfml(sysAddr, blueprintId)) Send(sysAddr, blueprintId, resourceType, std::nullopt);
		return;
	case eAction::WAIT:
		break;
	}

	if (TryAnswerIcon(sysAddr, blueprintId, resourceType)) return;
	// Not made yet: a client wants it, so a model still in its quiet period after a save is made now (as when a client
	// asks the UGC server for it), and the request is answered when it's made
	Database::Get()->ExpediteUgcModel(blueprintId);
	for (const auto& waiting : g_Waiting) {
		if (waiting.sysAddr == sysAddr && waiting.blueprintId == blueprintId && waiting.resourceType == resourceType) return;
	}
	if (g_Waiting.size() >= MAX_WAITING) g_Waiting.erase(g_Waiting.begin());
	g_Waiting.push_back({ sysAddr, blueprintId, resourceType, std::chrono::steady_clock::now() });
}

void UgcManifest::SwitchClient(const SystemAddress& sysAddr, const LWOOBJID blueprintId, const IUgc::FileChecksum& checksum, const std::vector<LWOOBJID>& modelIds) {
	// The new checksum first: the client keeps its cached one (its own build's) otherwise, and loads the file it has
	Send(sysAddr, blueprintId, eUgcResourceType::NIF, checksum);
	const auto due = std::chrono::steady_clock::now() + RECONSTRUCT_DELAY;
	for (const auto objectId : modelIds) {
		// Flushes the client's cached NIF, HKX and LXFML of the blueprint and preloads the NIF and HKX again
		// (LWOBlueprintComponent::OnNotifyClientUGCModelReady, 0x00ca6430); an object already drawn keeps its mesh
		GameMessages::NotifyClientUGCModelReady ready;
		ready.target = objectId;
		ready.blueprintID = blueprintId;
		ready.Send(sysAddr);
		// So the model is also taken down and constructed again once the flush is done (constructed at once, the flush
		// can land after the new object loaded its mesh, which then disappears). Its HKX's cached checksum is still the
		// client's own build's, so it keeps its collision.
		std::erase_if(g_Reconstructs, [&](const Reconstruct& r) { return r.sysAddr == sysAddr && r.objectId == objectId; });
		g_Reconstructs.push_back({ sysAddr, objectId, due });
	}
}

namespace {
	// Takes a model down for a client whose flush is done, then constructs it again a moment later (not in the same
	// batch: the client would drop the construction of an object it hasn't deleted yet); false until it's done
	bool ConstructAgain(Reconstruct& pending, const std::chrono::steady_clock::time_point now) {
		if (now < pending.due) return false;
		if (!Game::entityManager || !PlayerManager::GetPlayer(pending.sysAddr)) return true;
		auto* const model = Game::entityManager->GetEntity(pending.objectId);
		if (!model) return true;
		if (!pending.destructed) {
			Game::entityManager->DestructEntity(model, pending.sysAddr);
			pending.destructed = true;
			pending.due = now + CONSTRUCT_AFTER_DESTRUCT;
			return false;
		}
		Game::entityManager->ConstructEntity(model, pending.sysAddr);
		return true;
	}

}

namespace {
	// Sends every player in this world the made models' checksums, then transfers them back into this zone and clone:
	// the client loads the property again, and its manifest cache now has the new meshes' checksums
	void ReloadPlayers(const std::set<LWOOBJID>& blueprintIds) {
		const auto zoneId = Game::zoneManager->GetZoneID();
		for (auto* const player : PlayerManager::GetAllPlayers()) {
			if (!player) continue;
			const auto sysAddr = player->GetSystemAddress();
			for (const auto id : blueprintIds) {
				const auto checksum = Database::Get()->GetUgcFileChecksum(id, "model.nif");
				if (checksum && checksum->md5.size() == 32) Send(sysAddr, id, eUgcResourceType::NIF, checksum);
			}
			ChatPackets::SendSystemMessage(sysAddr, u"The property's models were made again: loading the property again.");
			const auto objectId = player->GetObjectID();
			ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, zoneId.GetMapID(), zoneId.GetCloneID(), false,
				[objectId](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
					auto* const entity = Game::entityManager->GetEntity(objectId);
					if (!entity || !entity->GetCharacter()) return;
					auto* const character = entity->GetCharacter();
					character->SetZoneID(zoneID);
					character->SetZoneInstance(zoneInstance);
					character->SetZoneClone(zoneClone);
					character->SaveXMLToDatabase();
					ClientPackets::TransferToWorld transfer;
					transfer.serverIP = LUString(serverIP);
					transfer.serverPort = serverPort;
					transfer.mythranShift = mythranShift;
					transfer.Send(entity->GetSystemAddress());
				});
		}
	}
}

size_t UgcManifest::ReprocessProperty(const LWOOBJID propertyId) {
	std::set<LWOOBJID> blueprintIds;
	for (const auto& model : Database::Get()->GetPropertyModels(propertyId)) {
		if (model.ugcId != 0) blueprintIds.insert(model.ugcId);
	}
	if (blueprintIds.empty()) return 0;
	Database::Get()->ResetPropertyUgcModelProcessing(propertyId);
	const auto now = std::chrono::steady_clock::now();
	g_PropertyReload = PropertyReload{ std::move(blueprintIds), now, now + RETRY_INTERVAL };
	LOG("Making the %zu models of property %llu again; players are reloaded when they're made", g_PropertyReload->blueprintIds.size(), static_cast<unsigned long long>(propertyId));
	return g_PropertyReload->blueprintIds.size();
}

void UgcManifest::Update() {
	const auto now = std::chrono::steady_clock::now();
	if (g_PropertyReload && now >= g_PropertyReload->nextCheck) {
		g_PropertyReload->nextCheck = now + RETRY_INTERVAL;
		bool waiting = false;
		for (const auto id : g_PropertyReload->blueprintIds) {
			const auto info = Database::Get()->GetUgcProcessInfo(id);
			waiting = waiting || (info && info->state == IUgc::eProcessState::PENDING);
		}
		if (!waiting || now - g_PropertyReload->since > MAX_WAIT) {
			ReloadPlayers(g_PropertyReload->blueprintIds);
			g_PropertyReload.reset();
		}
	}
	RunDueSwitches(now);
	std::erase_if(g_Reconstructs, [now](Reconstruct& pending) { return ConstructAgain(pending, now); });

	if (g_Waiting.empty() || now < g_NextRetry) return;
	g_NextRetry = now + RETRY_INTERVAL;
	std::erase_if(g_Waiting, [now](const Waiting& waiting) {
		return now - waiting.since > MAX_WAIT || TryAnswerIcon(waiting.sysAddr, waiting.blueprintId, waiting.resourceType);
	});
}

void UgcManifest::OnDisconnect(const SystemAddress& sysAddr) {
	std::erase_if(g_Waiting, [&sysAddr](const Waiting& waiting) { return waiting.sysAddr == sysAddr; });
	g_Switches.Forget(sysAddr);
	std::erase_if(g_Reconstructs, [&sysAddr](const Reconstruct& pending) { return pending.sysAddr == sysAddr; });
}

namespace {
	// The switches that are due: each client is sent the served checksum and its models shown to it are switched
	void RunDueSwitches(const std::chrono::steady_clock::time_point now) {
		if (g_Switches.Pending() == 0) return;
		const auto due = g_Switches.TakeDue(now);
		if (due.empty() || !Game::entityManager) return;
		const auto models = Game::entityManager->GetEntitiesByLOT(BrickByBrick::MODEL_OBJECT_LOT);
		for (const auto& pending : due) {
			// Models of a /reprocessproperty: the players are reloaded once they're all made
			if (g_PropertyReload && g_PropertyReload->blueprintIds.contains(pending.blueprintId)) continue;
			auto* const player = PlayerManager::GetPlayer(pending.sysAddr);
			if (!player) continue;
			auto* const ghost = player->GetComponent<GhostComponent>();
			std::vector<LWOOBJID> shown;
			for (auto* const model : models) {
				if (!model || model->GetVar<LWOOBJID>(u"blueprintid") != pending.blueprintId) continue;
				// Not shown to this client (yet): it loads the served mesh when it's constructed
				if (model->GetIsGhostingCandidate() && (!ghost || !ghost->IsObserved(model->GetObjectID()))) continue;
				shown.push_back(model->GetObjectID());
			}
			if (shown.empty()) continue;
			const auto checksum = Database::Get()->GetUgcFileChecksum(pending.blueprintId, "model.nif");
			if (!checksum || checksum->md5.size() != 32) continue;
			UgcManifest::SwitchClient(pending.sysAddr, pending.blueprintId, *checksum, shown);
			LOG("Switching %s to the served mesh of model %llu (%zu placed)", pending.sysAddr.ToString(), static_cast<unsigned long long>(pending.blueprintId), shown.size());
		}
	}
}

void UgcManifest::OnLxfmlSent(const SystemAddress& sysAddr, const LWOOBJID blueprintId) {
	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) return;
	g_Switches.LxfmlSent(sysAddr, blueprintId, std::chrono::steady_clock::now());
}

void UgcManifest::OnModelsMade(const std::vector<LWOOBJID>& blueprintIds) {
	for (const auto id : blueprintIds) g_LxfmlChecksums.erase(id); // made again after its LXFML changed, maybe
	if (!ServesModels() || !Game::entityManager) return;

	const auto& players = PlayerManager::GetAllPlayers();
	if (players.empty()) return;
	const auto models = Game::entityManager->GetEntitiesByLOT(BrickByBrick::MODEL_OBJECT_LOT);
	const auto now = std::chrono::steady_clock::now();
	for (const auto id : blueprintIds) {
		// Models of a /reprocessproperty: the players are reloaded once they're all made
		if (g_PropertyReload && g_PropertyReload->blueprintIds.contains(id)) continue;
		const auto placed = std::count_if(models.begin(), models.end(), [id](Entity* model) { return model && model->GetVar<LWOOBJID>(u"blueprintid") == id; });
		if (placed == 0) continue;
		// A client sent the model's LXFML lately may still be building it, and its build would undo the switch: it's
		// switched once it can't be building it any more, the others now
		for (auto* const player : players) {
			if (player) g_Switches.Schedule(player->GetSystemAddress(), id, now);
		}
		LOG("The UGC server made model %llu (%zu placed here): switching %zu client(s) to it", static_cast<unsigned long long>(id), static_cast<size_t>(placed), players.size());
	}
	RunDueSwitches(now);
}
