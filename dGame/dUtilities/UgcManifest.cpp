#include "UgcManifest.h"

#include <chrono>
#include <map>
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
#include "Game.h"
#include "Logger.h"
#include "MD5.h"
#include "PlayerManager.h"
#include "Sd0.h"

namespace {
	// How often the waiting requests are looked up again, how long they wait at most and how many there can be
	constexpr auto RETRY_INTERVAL = std::chrono::seconds(5);
	constexpr auto MAX_WAIT = std::chrono::minutes(15);
	constexpr size_t MAX_WAITING = 512;
	// A client asks for a model's NIF, HKX and LXFML at once: its LXFML is sent once for all three
	constexpr auto LXFML_RESEND_AFTER = std::chrono::seconds(10);
	// LXFML checksums worked out, kept for the next clients (a model's LXFML only changes with a new save)
	constexpr size_t MAX_LXFML_CHECKSUMS = 4096;

	struct Waiting {
		SystemAddress sysAddr;
		LWOOBJID blueprintId{};
		eUgcResourceType resourceType{};
		std::chrono::steady_clock::time_point since;
	};

	std::vector<Waiting> g_Waiting;
	std::chrono::steady_clock::time_point g_NextRetry{};
	std::map<std::pair<SystemAddress, LWOOBJID>, std::chrono::steady_clock::time_point> g_LxfmlSent;
	std::map<LWOOBJID, IUgc::FileChecksum> g_LxfmlChecksums;

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
		std::erase_if(g_LxfmlSent, [now](const auto& sent) { return now - sent.second > LXFML_RESEND_AFTER; });
		if (g_LxfmlSent.contains({ sysAddr, blueprintId })) return true;

		auto model = Database::Get()->GetUgcModel(blueprintId);
		if (!model) return false;
		ClientPackets::BlueprintSaveResponse response;
		response.localId = LWOOBJID_EMPTY; // zero, like the LXFML sent when a property loads
		response.reasonCode = eBlueprintSaveResponseType::EverythingWorked;
		response.models.push_back({ blueprintId, model->lxfmlData.str() });
		response.Send(sysAddr);
		g_LxfmlSent[{ sysAddr, blueprintId }] = now;
		// Its mesh is wanted: a model still in its quiet period after a save is made now
		Database::Get()->ExpediteUgcModel(blueprintId);
		LOG_DEBUG("Sent the LXFML of %llu for the client to build", static_cast<unsigned long long>(blueprintId));
		return true;
	}
}

UgcManifest::eAction UgcManifest::Decide(const eUgcResourceType type, const bool manifestOn, const bool modelsOn, const bool meshMade) {
	if (!manifestOn) return eAction::NONE;
	switch (type) {
	case eUgcResourceType::DDS: return eAction::WAIT; // answered at once when the icon is made
	case eUgcResourceType::NIF:
	case eUgcResourceType::LXFML:
		return modelsOn && meshMade ? eAction::ANSWER : eAction::SEND_LXFML;
	case eUgcResourceType::HKX:
		// The UGC server makes no physics: the client uses an HKX it built before, else it asks and gets 404
		return modelsOn && meshMade ? eAction::ANSWER_UNKNOWN : eAction::SEND_LXFML;
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

bool UgcManifest::ClientBuildsModel(const LWOOBJID blueprintId) {
	return !ServesModels() || !MeshMade(blueprintId);
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

void UgcManifest::Update() {
	if (g_Waiting.empty()) return;
	const auto now = std::chrono::steady_clock::now();
	if (now < g_NextRetry) return;
	g_NextRetry = now + RETRY_INTERVAL;
	std::erase_if(g_Waiting, [now](const Waiting& waiting) {
		return now - waiting.since > MAX_WAIT || TryAnswerIcon(waiting.sysAddr, waiting.blueprintId, waiting.resourceType);
	});
}

void UgcManifest::OnDisconnect(const SystemAddress& sysAddr) {
	std::erase_if(g_Waiting, [&sysAddr](const Waiting& waiting) { return waiting.sysAddr == sysAddr; });
	std::erase_if(g_LxfmlSent, [&sysAddr](const auto& sent) { return sent.first.first == sysAddr; });
}

void UgcManifest::OnModelsMade(const std::vector<LWOOBJID>& blueprintIds) {
	for (const auto id : blueprintIds) g_LxfmlChecksums.erase(id); // made again after its LXFML changed, maybe
	if (!ServesModels() || !Game::entityManager) return;

	const auto& players = PlayerManager::GetAllPlayers();
	if (players.empty()) return;
	const auto models = Game::entityManager->GetEntitiesByLOT(BrickByBrick::MODEL_OBJECT_LOT);
	for (const auto id : blueprintIds) {
		std::vector<Entity*> shown;
		for (auto* const model : models) {
			if (model && model->GetVar<LWOOBJID>(u"blueprintid") == id) shown.push_back(model);
		}
		if (shown.empty()) continue;
		const auto checksum = Database::Get()->GetUgcFileChecksum(id, "model.nif");
		if (!checksum || checksum->md5.size() != 32) continue;

		// The new checksum first: the client keeps its cached one otherwise, and would load the file it has
		for (auto* const player : players) {
			if (player) Send(player->GetSystemAddress(), id, eUgcResourceType::NIF, checksum);
		}
		for (auto* const model : shown) {
			GameMessages::NotifyClientUGCModelReady ready;
			ready.target = model->GetObjectID();
			ready.blueprintID = id;
			ready.Send(UNASSIGNED_SYSTEM_ADDRESS);
		}
		LOG("The UGC server made model %llu again: told %zu client(s) about %zu placed model(s)", static_cast<unsigned long long>(id), players.size(), shown.size());
	}
}
