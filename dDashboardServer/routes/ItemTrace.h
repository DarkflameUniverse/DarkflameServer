#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "dCommonVars.h"
#include "IEconomyLedger.h"

/**
 * Follows an item through economy_transfers. An item gets a new object id at every hop (trade, mail sent, mail
 * claimed, moving to another inventory), so the ledger rows form a graph: item_id -> new_item_id. From any id in it
 * the trace walks back to the first recorded id and forward to the latest id(s):
 * - a stack can be split: one id with several hops out (part traded, part kept). Every part is followed.
 * - a hop can merge into a stack the receiver already had (merged, or proven by that id having rows from before the
 *   hop). That stack's own earlier history is not this item's, so it is left out unless followMerges; later hops of
 *   the stack are followed, since the item is part of it from then on. Other items joining this item's stack are
 *   listed (MERGE_IN) but not followed back.
 * Pure: the rows come from a callback, so it can be unit tested. Ids are walked at most once per direction, so ids
 * that come back (a cycle) end the walk, and it stops at maxHops.
 */
namespace ItemTrace {
	using eTransferMethod = IEconomyLedger::eTransferMethod;

	struct Hop {
		int64_t row{};                // economy_transfers.id
		int64_t time{};
		eTransferMethod method{};
		LWOOBJID itemId{};
		LWOOBJID newItemId{};
		LWOOBJID fromCharacter{};
		LWOOBJID toCharacter{};
		std::optional<bool> merged;   // not recorded on rows from before the game wrote it
	};

	enum class eRole : uint8_t {
		LINE,     // on the way from the first recorded id to the searched id, or after it
		BRANCH,   // another part of a stack the searched item came from
		MERGE_IN  // other items joining one of this item's stacks (not followed back)
	};

	enum class eGap : uint8_t {
		OWNER,          // the hop starts with someone other than the one the previous hop gave the item to
		MAIL_NOT_SENT,  // a claimed mail whose sending was not recorded (sent before the ledger, or the rows were pruned)
		NO_NEW_ID       // the hop did not record the item's new id
	};

	struct TracedHop {
		Hop hop;
		eRole role{};
		bool merge{};          // landed in a stack that was already there
		bool mergeProven{};    // merge worked out from the rows (older rows without the merged column)
		std::optional<eGap> gap;
	};

	struct Chain {
		std::vector<TracedHop> hops;    // oldest first
		std::vector<LWOOBJID> ids;      // every id the item had (line and branches), in the order they were reached
		std::vector<LWOOBJID> latest;   // ids with no hop out after the item reached them: where it should be now
		LWOOBJID first{};               // the earliest recorded id on the line to the searched id
		bool truncated{};               // stopped at maxHops
		size_t queries{};
	};

	using Fetch = std::function<std::vector<Hop>(const std::vector<LWOOBJID>&)>;

	// A row of IEconomyLedger::GetTransfersForItems
	inline Hop FromRow(const nlohmann::json& row) {
		const auto id = [&](const char* key) -> LWOOBJID {
			const auto& value = row.at(key);
			return value.is_string() ? std::stoll(value.get<std::string>()) : value.get<LWOOBJID>();
		};
		Hop hop{ row.value("id", int64_t{}), row.value("time", int64_t{}), static_cast<eTransferMethod>(row.value("method", 0)),
			id("item_id"), id("new_item_id"), id("from_character"), id("to_character") };
		if (row.contains("merged") && row["merged"].is_boolean()) hop.merged = row["merged"].get<bool>();
		return hop;
	}

	namespace detail {
		using Key = std::pair<int64_t, int64_t>; // (time, row): the order hops happened in
		constexpr Key BEFORE_ALL{ INT64_MIN, INT64_MIN };

		inline Key KeyOf(const Hop& hop) { return { hop.time, hop.row }; }
	}

	inline Chain Build(const LWOOBJID start, const Fetch& fetch, const size_t maxHops = 200, const bool followMerges = false) {
		using namespace detail;
		Chain chain;
		chain.first = start;
		if (start == 0) return chain;

		std::map<int64_t, Hop> rows;                  // every row fetched, by row id
		std::map<LWOOBJID, std::vector<int64_t>> out; // item_id -> rows
		std::map<LWOOBJID, std::vector<int64_t>> in;  // new_item_id -> rows
		std::set<LWOOBJID> fetched;

		std::map<int64_t, size_t> traced;             // row -> index in chain.hops
		struct Node {
			bool backDone{};
			std::optional<Key> forwardFrom;           // lowest point forward hops were followed from
			std::optional<eRole> role;                // LINE or BRANCH
			std::optional<int64_t> arrivedBy;         // the hop that gave the item this id (no merge)
		};
		std::map<LWOOBJID, Node> nodes;

		struct Task {
			LWOOBJID id{};
			bool back{};
			std::optional<Key> forwardFrom;
			eRole role{};
		};
		std::vector<Task> tasks{ { start, true, BEFORE_ALL, eRole::LINE } };

		const auto sorted = [&](const std::vector<int64_t>& list) {
			std::vector<const Hop*> hops;
			for (const auto row : list) hops.push_back(&rows.at(row));
			std::ranges::sort(hops, [](const Hop* a, const Hop* b) { return KeyOf(*a) < KeyOf(*b); });
			return hops;
		};

		// Whether a hop into `id` joined a stack that was already there
		const auto mergeOf = [&](const Hop& hop) -> std::pair<bool, bool> {
			if (hop.merged) return { *hop.merged, false };
			const auto key = KeyOf(hop);
			for (const auto* list : { &out, &in }) {
				const auto it = list->find(hop.newItemId);
				if (it == list->end()) continue;
				for (const auto row : it->second) {
					if (row != hop.row && KeyOf(rows.at(row)) < key) return { true, true };
				}
			}
			return { false, false };
		};

		// Returns false once the hop limit is reached
		const auto addHop = [&](const Hop& hop, const eRole role) {
			const auto it = traced.find(hop.row);
			if (it != traced.end()) {
				if (role == eRole::LINE) chain.hops[it->second].role = eRole::LINE;
				return true;
			}
			if (chain.hops.size() >= maxHops) {
				chain.truncated = true;
				return false;
			}
			traced.emplace(hop.row, chain.hops.size());
			chain.hops.push_back({ hop, role });
			return true;
		};

		const auto noteId = [&](const LWOOBJID id, const eRole role) {
			auto& node = nodes[id];
			if (!node.role) chain.ids.push_back(id);
			if (!node.role || role == eRole::LINE) node.role = role;
		};

		while (!tasks.empty() && !chain.truncated) {
			std::vector<LWOOBJID> toFetch;
			for (const auto& task : tasks) {
				if (task.id != 0 && !fetched.contains(task.id) && std::ranges::find(toFetch, task.id) == toFetch.end()) toFetch.push_back(task.id);
			}
			if (!toFetch.empty()) {
				chain.queries++;
				for (const auto& hop : fetch(toFetch)) {
					if (!rows.emplace(hop.row, hop).second) continue;
					if (hop.itemId != 0) out[hop.itemId].push_back(hop.row);
					if (hop.newItemId != 0) in[hop.newItemId].push_back(hop.row);
				}
				fetched.insert(toFetch.begin(), toFetch.end());
			}

			std::vector<Task> next;
			for (const auto& task : tasks) {
				if (chain.truncated) break;
				if (task.id == 0) continue;
				auto& node = nodes[task.id];
				// A node reached again as part of the line is walked again with that role; otherwise once per direction
				const bool upgrade = task.role == eRole::LINE && node.role == eRole::BRANCH;
				noteId(task.id, task.role);

				if (task.back && (!node.backDone || upgrade)) {
					node.backDone = true;
					bool creatorFound = false;
					for (const auto* hop : sorted(in[task.id])) {
						// Older rows kept the id: the same object changing hands
						if (hop->itemId == task.id) {
							if (!addHop(*hop, task.role)) break;
							continue;
						}
						const auto [merge, proven] = mergeOf(*hop);
						if (merge || creatorFound) {
							if (!addHop(*hop, eRole::MERGE_IN)) break;
							if (followMerges) next.push_back({ hop->itemId, true, std::nullopt, eRole::BRANCH });
							continue;
						}
						creatorFound = true;
						node.arrivedBy = hop->row;
						if (!addHop(*hop, task.role)) break;
						// Where it came from: that id's history, and its other parts (a split stack) as branches
						next.push_back({ hop->itemId, true, std::nullopt, task.role });
						next.push_back({ hop->itemId, false, BEFORE_ALL, eRole::BRANCH });
					}
				}

				if (task.forwardFrom && (!node.forwardFrom || *task.forwardFrom < *node.forwardFrom || upgrade)) {
					const auto from = *task.forwardFrom;
					node.forwardFrom = node.forwardFrom ? std::min(*node.forwardFrom, from) : from;
					// Other items joining this stack after the item got here
					for (const auto* hop : sorted(in[task.id])) {
						if (KeyOf(*hop) <= from || traced.contains(hop->row) || hop->itemId == task.id) continue;
						if (!addHop(*hop, eRole::MERGE_IN)) break;
						if (followMerges) next.push_back({ hop->itemId, true, std::nullopt, eRole::BRANCH });
					}
					for (const auto* hop : sorted(out[task.id])) {
						if (KeyOf(*hop) <= from) continue;
						const auto role = task.role;
						if (!addHop(*hop, role)) break;
						if (hop->newItemId == 0 || hop->newItemId == task.id) continue;
						// Only what happens to the next id after this hop is this item's (it may be an older stack)
						next.push_back({ hop->newItemId, false, KeyOf(*hop), role });
						if (followMerges) next.push_back({ hop->newItemId, true, std::nullopt, eRole::BRANCH });
					}
				}
			}
			tasks = std::move(next);
		}

		// Merges, gaps and where the item is now, from everything fetched
		for (auto& entry : chain.hops) {
			const auto& hop = entry.hop;
			if (entry.role != eRole::MERGE_IN && hop.itemId != hop.newItemId) {
				const auto [merge, proven] = mergeOf(hop);
				entry.merge = merge;
				entry.mergeProven = proven;
			}
			if (hop.itemId != 0 && hop.newItemId == 0) {
				entry.gap = eGap::NO_NEW_ID;
				continue;
			}
			if (entry.role == eRole::MERGE_IN) continue;
			// The hop that gave the item this id: the one that made the id, or the merge of this item into it
			const Hop* previous = nullptr;
			for (const auto& other : chain.hops) {
				if (other.role == eRole::MERGE_IN || other.hop.newItemId != hop.itemId || !(KeyOf(other.hop) < KeyOf(hop))) continue;
				if (!previous || KeyOf(*previous) < KeyOf(other.hop)) previous = &other.hop;
			}
			if (!previous) {
				if (hop.method == eTransferMethod::MAIL_CLAIMED) entry.gap = eGap::MAIL_NOT_SENT;
				continue;
			}
			if (previous->method == eTransferMethod::MAIL_SENT) {
				if (hop.method != eTransferMethod::MAIL_CLAIMED || hop.toCharacter != previous->toCharacter) entry.gap = eGap::OWNER;
			} else if (hop.method == eTransferMethod::MAIL_CLAIMED) {
				entry.gap = eGap::MAIL_NOT_SENT;
			} else if (hop.fromCharacter != previous->toCharacter) {
				entry.gap = eGap::OWNER;
			}
		}

		std::ranges::sort(chain.hops, [](const TracedHop& a, const TracedHop& b) { return KeyOf(a.hop) < KeyOf(b.hop); });

		// The first id: walk the line back from the searched id
		std::set<LWOOBJID> seen{ start };
		for (auto id = start; ;) {
			const auto it = nodes.find(id);
			if (it == nodes.end() || !it->second.arrivedBy) break;
			const auto previous = rows.at(*it->second.arrivedBy).itemId;
			if (previous == 0 || !seen.insert(previous).second) break;
			chain.first = id = previous;
		}

		// Latest: ids with no hop out once the item was there (merges into them count as the item staying)
		for (const auto id : chain.ids) {
			const auto& node = nodes[id];
			if (!node.forwardFrom) continue; // not followed forward (only reached going back)
			bool leftAgain = false;
			for (const auto row : out[id]) {
				const auto& hop = rows.at(row);
				if (hop.newItemId != id && KeyOf(hop) > *node.forwardFrom && traced.contains(row)) leftAgain = true;
			}
			if (!leftAgain) chain.latest.push_back(id);
		}
		return chain;
	}
}
