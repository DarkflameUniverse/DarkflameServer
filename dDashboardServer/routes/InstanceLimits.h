#pragma once

#include <cstdint>
#include <optional>
#include <string>

/**
 * Player caps and spare instances per zone, as the master server applies them (dMasterServer/InstanceManager): a
 * player is sent to a running public instance while it has fewer players than the soft cap (a friend joining them
 * while it has fewer than the hard cap), otherwise a new instance starts. The hard cap is also the most connections
 * its world server accepts. Caps not set here come from the client's ZoneTable. Pure; unit tested.
 */
namespace InstanceLimits {
	constexpr uint32_t MAX_CAP = 500;
	constexpr uint32_t MAX_SPARE = 5;

	struct Caps {
		uint32_t soft{};
		uint32_t hard{};
	};

	// The caps new instances get, as InstanceManager::GetSoftCap/GetHardCap work them out
	inline Caps Effective(std::optional<uint32_t> softCap, std::optional<uint32_t> hardCap, Caps client) {
		const auto hard = hardCap.value_or(client.hard);
		return { std::min(softCap.value_or(client.soft), hard), hard };
	}

	// What is wrong with a zone's limits as entered, if anything
	inline std::optional<std::string> Validate(uint32_t zoneId, std::optional<uint32_t> softCap, std::optional<uint32_t> hardCap, uint32_t spare, Caps client) {
		if (zoneId == 0) return "Character selection (zone 0) has no player cap";
		if (hardCap && (*hardCap < 1 || *hardCap > MAX_CAP)) return "The hard cap must be 1 to " + std::to_string(MAX_CAP);
		if (softCap && (*softCap < 1 || *softCap > MAX_CAP)) return "The soft cap must be 1 to " + std::to_string(MAX_CAP);
		const auto hard = hardCap.value_or(client.hard);
		if (softCap && *softCap > hard) return "The soft cap can't be above the hard cap (" + std::to_string(hard) + ")";
		if (spare > MAX_SPARE) return "Keep at most " + std::to_string(MAX_SPARE) + " spare instances";
		return std::nullopt;
	}
}
