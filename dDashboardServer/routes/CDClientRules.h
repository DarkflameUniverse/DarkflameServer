#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "CDClientSchema.h"
#include "eMissionTaskType.h"

/**
 * What the server makes of CDClient rows, for the CDClient browser: drop chances as dGame/dUtilities/Loot.cpp rolls
 * them, vendor stock as VendorComponent fills it, mission prerequisites as MissionPrerequisites evaluates them and what
 * a mission task's numbers are as MissionTask::Progress compares them. Each follows the server code it names, so when
 * that code changes, this should too. Pure, so it is unit tested.
 */
namespace CDClientRules {
	// ---- Loot ----

	struct RarityRow {
		double randmax{};
		int32_t rarity{};
	};

	/**
	 * The chance of each rarity a drop rolls (RollLootMatrix). The rows are walked in the order the server loads them
	 * (randmax descending) and a roll in [0, 1) takes the rarity of the last row whose randmax is at or above it, so a
	 * row gets the span between its randmax and the next lower one. A roll above every row keeps the starting rarity 1.
	 */
	inline std::vector<std::pair<int32_t, double>> RarityChances(const std::vector<RarityRow>& rows) {
		std::vector<std::pair<int32_t, double>> chances;
		const auto add = [&](int32_t rarity, double chance) {
			if (chance <= 0.0) return;
			const auto it = std::ranges::find_if(chances, [&](const auto& c) { return c.first == rarity; });
			if (it == chances.end()) chances.emplace_back(rarity, chance);
			else it->second += chance;
		};
		if (rows.empty()) {
			add(1, 1.0);
			return chances;
		}
		add(1, 1.0 - std::clamp(rows.front().randmax, 0.0, 1.0));
		for (size_t i = 0; i < rows.size(); i++) {
			const double top = std::clamp(rows[i].randmax, 0.0, 1.0);
			const double bottom = i + 1 < rows.size() ? std::clamp(rows[i + 1].randmax, 0.0, 1.0) : 0.0;
			add(rows[i].rarity, top - bottom);
		}
		return chances;
	}

	/**
	 * Which entries of a loot table a drop picks from (one of them, evenly) for a rolled rarity, as RollLootMatrix
	 * does: the table is sorted by item rarity, highest first (CDLootTableTable), items above the rolled rarity are
	 * skipped, items of it are taken, and while none of it has been found a lower rarity is taken and becomes the rarity
	 * looked for. `rarities` are the items' rarities (0 without an item component) in any order; indexes into it.
	 */
	inline std::vector<size_t> Candidates(const std::vector<int32_t>& rarities, int32_t rolled) {
		std::vector<size_t> order(rarities.size());
		for (size_t i = 0; i < order.size(); i++) order[i] = i;
		std::ranges::stable_sort(order, [&](size_t a, size_t b) { return rarities[a] > rarities[b]; });
		std::vector<size_t> picked;
		int32_t maxRarity = rolled;
		bool found = false;
		for (const auto index : order) {
			const auto rarity = rarities[index];
			if (rarity == maxRarity) {
				picked.push_back(index);
				found = true;
			} else if (rarity < maxRarity && !found) {
				picked.push_back(index);
				maxRarity = rarity;
			}
		}
		return picked;
	}

	// The chance that one drop from a loot table is each of its entries, given the rarity table it is rolled with
	inline std::vector<double> ChancePerDrop(const std::vector<int32_t>& rarities, const std::vector<RarityRow>& rows) {
		std::vector<double> chances(rarities.size(), 0.0);
		for (const auto& [rarity, chance] : RarityChances(rows)) {
			const auto picked = Candidates(rarities, rarity);
			for (const auto index : picked) chances[index] += chance / picked.size();
		}
		return chances;
	}

	struct DropOdds {
		double atLeastOne{}; // chance of getting it at least once
		double expected{};   // how many on average
	};

	/**
	 * Odds for one item of a loot matrix entry: the entry rolls with `percent`, then drops a count between minDrops and
	 * maxDrops (evenly), each drop being the item with `perDrop`. Before any live event's loot bonus.
	 */
	inline DropOdds Odds(double percent, int32_t minDrops, int32_t maxDrops, double perDrop) {
		percent = std::clamp(percent, 0.0, 1.0);
		minDrops = std::max(minDrops, 0);
		maxDrops = std::max(maxDrops, minDrops);
		double missAll = 0.0;
		const int32_t counts = maxDrops - minDrops + 1;
		for (int32_t n = minDrops; n <= maxDrops; n++) missAll += std::pow(1.0 - perDrop, n) / counts;
		return { percent * (1.0 - missAll), percent * (minDrops + maxDrops) / 2.0 * perDrop };
	}

	/**
	 * The chance a vendor has an entry's item for sale (VendorComponent::RefreshInventory): with minToDrop or
	 * maxToDrop 0 it sells the whole loot table, otherwise that many of its items at random (chance and rarity play no
	 * part), picked again every refresh.
	 */
	inline double VendorStockChance(int32_t minDrops, int32_t maxDrops, size_t tableSize) {
		if (minDrops == 0 || maxDrops == 0) return 1.0;
		if (tableSize == 0) return 0.0;
		maxDrops = std::max(maxDrops, minDrops);
		return std::min(1.0, (minDrops + maxDrops) / 2.0 / static_cast<double>(tableSize));
	}

	// ---- Missions ----

	/**
	 * Missions.prereqMissionID as MissionPrerequisites reads it: a mission (with an optional ":state" it must be in,
	 * otherwise it must be complete), then "|" (or) or "," "&" "(" (and) joining it to all the rest, which is read the
	 * same way; ")" and spaces are skipped. So "1|2,3" is 1 or (2 and 3), and brackets don't group. A term without a
	 * mission (mission 0) is always met.
	 */
	struct PrerequisiteTerm {
		uint32_t mission{};
		uint32_t state{};    // 0: must be complete
		bool orRest{};       // joined to the rest by "or" (otherwise "and"); the last term has no rest
	};

	inline std::vector<PrerequisiteTerm> ParsePrerequisites(std::string_view text) {
		std::vector<PrerequisiteTerm> terms;
		while (true) {
			PrerequisiteTerm term;
			bool inState = false, more = false;
			size_t i = 0;
			// Numbers too long for an ID are cut short rather than overflowing
			const auto append = [](uint32_t& value, char c) { if (value < 100000000u) value = value * 10 + (c - '0'); };
			for (; i < text.size(); i++) {
				const char c = text[i];
				if (c == '|' || c == ',' || c == '&' || c == '(') {
					term.orRest = c == '|';
					more = true;
					break;
				}
				if (c == ':') inState = true;
				else if (c >= '0' && c <= '9') append(inState ? term.state : term.mission, c);
			}
			if (!more) {
				term.orRest = false;
				terms.push_back(term);
				break;
			}
			terms.push_back(term);
			text.remove_prefix(i + 1);
		}
		// Terms without a mission are always met, so "and" ones change nothing ("(" starts one): leave them out
		std::vector<PrerequisiteTerm> kept;
		for (size_t i = 0; i < terms.size(); i++) {
			const bool last = i + 1 == terms.size();
			const bool drop = terms[i].mission == 0 && terms.size() > 1 && (last ? !kept.empty() && !kept.back().orRest : !terms[i].orRest);
			if (!drop) kept.push_back(terms[i]);
		}
		if (!kept.empty()) kept.back().orRest = false;
		return kept;
	}

	/**
	 * What a mission task's numbers are (MissionTask::Progress, and what the server passes to it): `target` and the
	 * comma separated targetGroup are compared with the value the task is progressed with, taskParam1's numbers with
	 * the value or the associate. Each list names what the numbers can be; empty means a number the browser can't link.
	 */
	struct TaskMeaning {
		bool progressed{ true };           // false: Progress logs the type as invalid and never counts it
		std::vector<CDClientSchema::eLink> target;
		bool targetGroupIds{};             // targetGroup holds more IDs like target
		bool targetGroupText{};            // targetGroup is compared as text (a point of interest, a score name)
		bool targetUsed{ true };           // false: target plays no part
		std::vector<CDClientSchema::eLink> parameters;
		bool racingParameter{};            // taskParam1's first number is an eRacingTaskParam
	};

	inline TaskMeaning MeaningOf(eMissionTaskType type) {
		using enum eMissionTaskType;
		using Link = CDClientSchema::eLink;
		TaskMeaning meaning;
		switch (type) {
		// Progressed with the LOT of what was smashed, scripted, collected, interacted with, tamed or gathered
		case SMASH: case SCRIPT: case COLLECTION: case INTERACT: case PET_TAMING: case GATHER:
			meaning.target = { Link::OBJECT };
			meaning.targetGroupIds = true;
			break;
		// Only `target` is compared (GetTarget)
		case TALK_TO_NPC: case USE_ITEM:
			meaning.target = { Link::OBJECT };
			break;
		// The emote is in taskParam1; `target` is the LOT it was done at
		case EMOTE:
			meaning.target = { Link::OBJECT };
			meaning.parameters = { Link::EMOTE };
			break;
		// The skill is in taskParam1; the targets are the LOT it hit
		case USE_SKILL:
			meaning.target = { Link::OBJECT };
			meaning.targetGroupIds = true;
			meaning.parameters = { Link::SKILL };
			break;
		// An activity ID, or the LOT of a quick build or cannon
		case ACTIVITY:
			meaning.target = { Link::ACTIVITY, Link::OBJECT };
			meaning.targetGroupIds = true;
			break;
		// The minigame's activity ID (or its LOT without one); targetGroup names the score, targetValue is the least
		case PERFORM_ACTIVITY:
			meaning.target = { Link::ACTIVITY, Link::OBJECT };
			meaning.targetGroupText = true;
			break;
		// Only the point of interest's name counts
		case EXPLORE:
			meaning.targetUsed = false;
			meaning.targetGroupText = true;
			break;
		case META:
			meaning.target = { Link::MISSION };
			meaning.targetGroupIds = true;
			break;
		// Progressed with the skill ID of the powerup
		case POWERUP:
			meaning.target = { Link::SKILL };
			meaning.targetGroupIds = true;
			break;
		// Player flag IDs
		case PLAYER_FLAG:
		// Progressed with 0 and the reputation earned as the count
		case EARN_REPUTATION:
		case VISIT_PROPERTY:
			meaning.targetGroupIds = true;
			break;
		// Targets are zones, tracks or missions depending on the racing parameter
		case RACING:
			meaning.targetGroupIds = true;
			meaning.racingParameter = true;
			break;
		// Any progress counts
		case PLACE_MODEL: case ADD_BEHAVIOR: case DONATION:
			meaning.targetUsed = false;
			break;
		default:
			meaning.progressed = false;
			meaning.targetUsed = false;
			break;
		}
		return meaning;
	}

	// Numbers in a comma separated list, as MissionTask reads targetGroup and taskParam1 (anything else is skipped)
	inline std::vector<int64_t> NumberList(std::string_view text) {
		std::vector<int64_t> numbers;
		while (!text.empty()) {
			const auto comma = text.find(',');
			// TryParse skips leading spaces but not trailing ones, so "1 ,2" loses the 1 on the server too
			if (const auto number = GeneralUtils::TryParse<uint32_t>(text.substr(0, comma))) numbers.push_back(*number);
			if (comma == std::string_view::npos) break;
			text.remove_prefix(comma + 1);
		}
		return numbers;
	}
}
