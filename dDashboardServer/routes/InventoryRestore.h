#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string_view>
#include <vector>

#include "dCommonVars.h"
#include "eInventoryType.h"
#include "EconomyScan.h"

/**
 * Giving items back from a character snapshot: what the character had then and doesn't have now, and how much of
 * it can be mailed back without duplicating anything. Pure (no database or game data) so it can be unit tested.
 *
 * Items are compared by LOT across all inventories, so an item moved to the vault or split into another stack is not
 * "lost". What is still waiting in the character's mailbox counts as held (a restore already sent, or anything else),
 * and a lost object that someone else now holds (traded, mailed away) is not given back either.
 */
namespace InventoryRestore {
	// The most a single mail attachment carries (the game's stack limit for mail)
	constexpr uint32_t MAX_MAIL_COUNT = 999;

	struct Item {
		LWOOBJID id{};
		LOT lot{};
		uint32_t count{};
		uint32_t inventory{};
	};

	// Items that really belong to the character: not set sub-items, not stock of a vendor, not sold back to one
	inline bool CountsAsHeld(const EconomyScan::InventoryItem& item) {
		return !item.isProxy && item.inventoryType != eInventoryType::VENDOR_BUYBACK && item.inventoryType != eInventoryType::VENDOR;
	}

	inline std::vector<Item> Held(std::string_view xml) {
		std::vector<Item> items;
		EconomyScan::ForEachInventoryItem(xml, [&](const EconomyScan::InventoryItem& item) {
			if (CountsAsHeld(item)) items.push_back({ item.id, item.lot, item.count, item.inventoryType });
		});
		return items;
	}

	struct LostObject {
		LWOOBJID id{};
		uint32_t count{};
		uint32_t inventory{}; // where it was
	};

	struct Missing {
		LOT lot{};
		uint64_t then{};      // held in the snapshot
		uint64_t now{};       // held now
		uint64_t mailed{};    // waiting in the character's mailbox now
		uint64_t elsewhere{}; // lost objects of this LOT someone else holds now
		uint64_t restorable{};
		std::vector<LostObject> lost; // objects held then and not now, that nobody else holds
	};

	struct Change {
		uint32_t inventory{};
		LOT lot{};
		uint64_t then{};
		uint64_t now{};
	};

	struct Diff {
		std::vector<Change> changes;  // per inventory and LOT, only where the count differs
		std::vector<Missing> missing; // per LOT, where the character has fewer than then
	};

	/**
	 * @param mailed LOT -> count waiting in the character's mailbox
	 * @param mailedIds object ids of those attachments (never sent again)
	 * @param elsewhere object id -> count held by someone else now (other characters, or mail to them)
	 */
	inline Diff Compare(const std::vector<Item>& then, const std::vector<Item>& now, const std::map<LOT, uint64_t>& mailed,
		const std::set<LWOOBJID>& mailedIds, const std::map<LWOOBJID, uint64_t>& elsewhere) {
		Diff diff;
		std::map<std::pair<uint32_t, LOT>, std::pair<uint64_t, uint64_t>> perInventory;
		std::map<LOT, std::pair<uint64_t, uint64_t>> perLot;
		std::set<LWOOBJID> idsNow;
		for (const auto& item : then) {
			perInventory[{ item.inventory, item.lot }].first += item.count;
			perLot[item.lot].first += item.count;
		}
		for (const auto& item : now) {
			perInventory[{ item.inventory, item.lot }].second += item.count;
			perLot[item.lot].second += item.count;
			idsNow.insert(item.id);
		}
		for (const auto& [key, counts] : perInventory) {
			if (counts.first != counts.second) diff.changes.push_back({ key.first, key.second, counts.first, counts.second });
		}

		std::map<LOT, Missing> missing;
		for (const auto& [lot, counts] : perLot) {
			if (counts.first <= counts.second) continue;
			auto& entry = missing[lot];
			entry.lot = lot;
			entry.then = counts.first;
			entry.now = counts.second;
			const auto it = mailed.find(lot);
			entry.mailed = it == mailed.end() ? 0 : it->second;
		}
		for (const auto& item : then) {
			const auto entry = missing.find(item.lot);
			if (entry == missing.end() || item.id == LWOOBJID_EMPTY || idsNow.contains(item.id) || mailedIds.contains(item.id)) continue;
			const auto held = elsewhere.find(item.id);
			if (held != elsewhere.end()) entry->second.elsewhere += std::min<uint64_t>(held->second, item.count);
			else entry->second.lost.push_back({ item.id, item.count, item.inventory });
		}
		for (auto& [lot, entry] : missing) {
			const auto accounted = entry.now + entry.mailed + entry.elsewhere;
			entry.restorable = entry.then > accounted ? entry.then - accounted : 0;
			diff.missing.push_back(std::move(entry));
		}
		return diff;
	}

	struct Mail {
		LOT lot{};
		uint32_t count{};
		LWOOBJID originalId{}; // LWOOBJID_EMPTY: the item gets a new id when claimed
	};

	/**
	 * The mails that give back `wanted` of a missing LOT (capped at what is restorable). Whole lost objects go back
	 * with their original ids while they fit, the rest as new items, each mail at most MAX_MAIL_COUNT.
	 */
	inline std::vector<Mail> Plan(const Missing& missing, uint64_t wanted) {
		std::vector<Mail> mails;
		auto left = std::min(wanted, missing.restorable);
		for (const auto& object : missing.lost) {
			if (object.count == 0 || object.count > left || object.count > MAX_MAIL_COUNT) continue;
			mails.push_back({ missing.lot, object.count, object.id });
			left -= object.count;
		}
		while (left > 0) {
			const auto count = static_cast<uint32_t>(std::min<uint64_t>(left, MAX_MAIL_COUNT));
			mails.push_back({ missing.lot, count, LWOOBJID_EMPTY });
			left -= count;
		}
		return mails;
	}
}
