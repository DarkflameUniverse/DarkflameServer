#include "UgcManifest.h"

#include <chrono>
#include <optional>
#include <string_view>
#include <vector>

#include "ClientPackets.h"
#include "Database.h"
#include "dConfig.h"
#include "Game.h"
#include "Logger.h"

namespace {
	// How often the waiting requests are looked up again, how long they wait at most and how many there can be
	constexpr auto RETRY_INTERVAL = std::chrono::seconds(5);
	constexpr auto MAX_WAIT = std::chrono::minutes(15);
	constexpr size_t MAX_WAITING = 512;

	struct Waiting {
		SystemAddress sysAddr;
		LWOOBJID blueprintId{};
		eUgcResourceType resourceType{};
		std::chrono::steady_clock::time_point since;
	};

	std::vector<Waiting> g_Waiting;
	std::chrono::steady_clock::time_point g_NextRetry{};

	// The UGC server's name for a blueprint's file of this type; nothing for the ones it doesn't make
	std::optional<std::string_view> FileName(eUgcResourceType type) {
		switch (type) {
		case eUgcResourceType::DDS: return "icon.dds";
		case eUgcResourceType::NIF: return "model.nif";
		default: return std::nullopt;
		}
	}

	bool Enabled() {
		// Off unless set: the 1.10.64 client downloads from http://127.0.0.1:80 whatever its boot.cfg says, and one that
		// can't connect there is logged out (docs/UgcServer.md)
		return Game::config->GetValue("ugc_manifest") == "1";
	}

	int HexDigit(char c) {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	}

	// Answers when the file's checksum is known; false when it isn't (yet)
	bool TryAnswer(const SystemAddress& sysAddr, LWOOBJID blueprintId, eUgcResourceType resourceType) {
		const auto file = FileName(resourceType);
		if (!file) return false;
		const auto checksum = Database::Get()->GetUgcFileChecksum(blueprintId, *file);
		if (!checksum || checksum->md5.size() != 32) return false;

		ClientPackets::UgcManifestResponse response;
		response.blueprintId = blueprintId;
		response.resourceType = resourceType;
		response.fileSize = checksum->size;
		for (size_t i = 0; i < response.md5.size(); i++) {
			const auto high = HexDigit(checksum->md5[i * 2]);
			const auto low = HexDigit(checksum->md5[i * 2 + 1]);
			if (high < 0 || low < 0) return false;
			response.md5[i] = static_cast<uint8_t>(high << 4 | low);
		}
		response.valid = true;
		response.Send(sysAddr);
		LOG_DEBUG("Sent the UGC manifest of %llu (type %i): %s, %u bytes", static_cast<unsigned long long>(blueprintId),
			static_cast<int>(resourceType), checksum->md5.c_str(), checksum->size);
		return true;
	}
}

void UgcManifest::OnRequest(const SystemAddress& sysAddr, LWOOBJID blueprintId, eUgcResourceType resourceType) {
	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS || !Enabled() || !FileName(resourceType)) return;
	if (TryAnswer(sysAddr, blueprintId, resourceType)) return;
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
		return now - waiting.since > MAX_WAIT || TryAnswer(waiting.sysAddr, waiting.blueprintId, waiting.resourceType);
	});
}

void UgcManifest::OnDisconnect(const SystemAddress& sysAddr) {
	std::erase_if(g_Waiting, [&sysAddr](const Waiting& waiting) { return waiting.sysAddr == sysAddr; });
}
