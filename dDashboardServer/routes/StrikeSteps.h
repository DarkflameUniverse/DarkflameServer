#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "eStrikeStep.h"

/**
 * Strike thresholds: what happens on its own when an account reaches a number of active strikes (Settings, Dashboard:
 * strike_warn_at, strike_mute_at with strike_mute_days, strike_ban_at with strike_ban_days; 0 turns a step off).
 * Pure, so it is unit tested.
 */
namespace StrikeSteps {
	struct Threshold {
		eStrikeStep step{};
		uint32_t at{};   // active strikes that set it off
		int64_t days{};  // MUTE, BAN: how long (BAN: 0 is permanent)
	};

	/**
	 * The step to apply now that the account has `active` strikes: the one with the highest threshold reached (the
	 * most severe on a tie), unless that step was already applied for that number of strikes (`applied`: step and the
	 * count it was applied for, while those strikes count).
	 */
	inline std::optional<Threshold> Choose(const std::vector<Threshold>& thresholds, uint32_t active, const std::vector<std::pair<eStrikeStep, uint32_t>>& applied) {
		std::optional<Threshold> chosen;
		for (const auto& threshold : thresholds) {
			if (threshold.at == 0 || threshold.at > active) continue;
			if (!chosen || threshold.at > chosen->at || (threshold.at == chosen->at && threshold.step > chosen->step)) chosen = threshold;
		}
		if (!chosen) return std::nullopt;
		for (const auto& [step, count] : applied) {
			if (step == chosen->step && count == chosen->at) return std::nullopt;
		}
		return chosen;
	}
}
