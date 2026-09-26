#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dCommonVars.h"
#include "GeneralUtils.h"
#include "eCharacterVersion.h"

/**
 * Pure helpers for the economy reports: reading items out of character XML without a full XML parse, and finding
 * object ids that exist more than once. No database or web server, so they can be unit tested.
 */
namespace EconomyScan {
	struct InventoryItem {
		uint32_t inventoryType{};
		LOT lot{};
		LWOOBJID id{};
		uint32_t count{};
		bool isProxy{}; // sub-item of another item (e.g. set parts); not an item of its own
	};

	// Value of attribute `name` inside a single tag, e.g. Attribute(R"(<i l="5" id="7">)", "id") == "7"
	inline std::string_view Attribute(std::string_view tag, std::string_view name) {
		size_t pos = 0;
		while ((pos = tag.find(name, pos)) != std::string_view::npos) {
			const bool startsWord = pos > 0 && (tag[pos - 1] == ' ' || tag[pos - 1] == '\t' || tag[pos - 1] == '\n');
			const auto quote = pos + name.size() + 1;
			if (startsWord && quote < tag.size() && tag[pos + name.size()] == '=' && tag[quote] == '"') {
				const auto end = tag.find('"', quote + 1);
				if (end == std::string_view::npos) return {};
				return tag.substr(quote + 1, end - quote - 1);
			}
			pos += name.size();
		}
		return {};
	}

	/**
	 * Visit every item in the <items> section of a character's XML:
	 * <items><in t="0"><i l="lot" id="objid" c="count" .../></in>...</items>
	 */
	template<typename Visitor>
	void ForEachInventoryItem(std::string_view xml, Visitor&& visit) {
		const auto itemsStart = xml.find("<items");
		if (itemsStart == std::string_view::npos) return;
		const auto itemsEnd = std::min(xml.find("</items>", itemsStart), xml.size());
		const auto items = xml.substr(itemsStart, itemsEnd - itemsStart);

		size_t pos = 0;
		uint32_t inventoryType = 0;
		while ((pos = items.find('<', pos)) != std::string_view::npos) {
			const auto tagEnd = items.find('>', pos);
			if (tagEnd == std::string_view::npos) break;
			const auto tag = items.substr(pos, tagEnd - pos + 1);
			pos = tagEnd + 1;

			if (tag.starts_with("<in ") || tag.starts_with("<in>")) {
				inventoryType = GeneralUtils::TryParse<uint32_t>(Attribute(tag, "t")).value_or(0);
			} else if (tag.starts_with("<i ")) {
				InventoryItem item;
				item.inventoryType = inventoryType;
				item.lot = GeneralUtils::TryParse<LOT>(Attribute(tag, "l")).value_or(LOT_NULL);
				item.id = GeneralUtils::TryParse<LWOOBJID>(Attribute(tag, "id")).value_or(LWOOBJID_EMPTY);
				item.count = GeneralUtils::TryParse<uint32_t>(Attribute(tag, "c")).value_or(1);
				const auto parent = GeneralUtils::TryParse<LWOOBJID>(Attribute(tag, "parent")).value_or(LWOOBJID_EMPTY);
				item.isProxy = parent != LWOOBJID_EMPTY;
				if (item.lot != LOT_NULL) visit(item);
			}
		}
	}

	/**
	 * The character version a save was last migrated to: <obj><lvl cv="..."/> (LevelProgressionComponent). The game
	 * runs every step after it the next time the character logs in (LEVEL_LOAD_COMPLETE in WorldServer.cpp). A save
	 * without it counts as eCharacterVersion::RELEASE (0).
	 */
	inline uint32_t CharacterVersion(std::string_view xml) {
		const auto start = xml.find("<lvl");
		if (start == std::string_view::npos) return 0;
		const auto end = xml.find('>', start);
		if (end == std::string_view::npos) return 0;
		return GeneralUtils::TryParse<uint32_t>(Attribute(xml.substr(start, end - start + 1), "cv")).value_or(0);
	}

	/**
	 * Whether the character's next login gives every item in its inventories a new, unique object id. The login
	 * migration step for eCharacterVersion::PET_IDS calls InventoryComponent::RegenerateItemIDs, which gives every
	 * item except proxies (items with a parent) a persistent id; it runs for every save older than
	 * INVENTORY_PERSISTENT_IDS. Before that version items had random 32 bit ids, which is where id collisions come from.
	 */
	constexpr bool GetsNewItemIdsAtLogin(uint32_t characterVersion) {
		return characterVersion < static_cast<uint32_t>(eCharacterVersion::INVENTORY_PERSISTENT_IDS);
	}

	// Where a copy of an object id was found
	struct Location {
		enum class eKind : uint8_t { CHARACTER, MAIL };
		eKind kind{};
		LWOOBJID ownerId{};   // character id, or the mail's receiver
		uint64_t mailId{};    // for MAIL
		uint32_t inventoryType{};
		LOT lot{};
		uint32_t count{};
		uint32_t characterVersion{}; // for CHARACTER: the save's <lvl cv>

		// Whether this copy gets a new object id when its character next logs in. Mail keeps its id: the migration
		// only touches the character's inventories.
		bool NewIdAtLogin() const { return kind == eKind::CHARACTER && GetsNewItemIdsAtLogin(characterVersion); }
	};

	/**
	 * Collects every place each object id is found; an id found in more than one place is a duplicated item.
	 * Only ids are kept, so memory grows with the number of items, not the size of the XML.
	 */
	class DuplicateFinder {
	public:
		void Add(LWOOBJID id, const Location& location) {
			if (id == LWOOBJID_EMPTY) return;
			auto& entry = m_Seen[id];
			if (entry.count++ == 0) {
				entry.first = location;
			} else {
				auto& copies = m_Duplicates[id];
				if (copies.empty()) copies.push_back(entry.first);
				copies.push_back(location);
			}
		}

		size_t ItemsSeen() const { return m_Seen.size(); }

		// Ids found in more than one place and all their locations, most copies first. Use Classify() to tell
		// duplicates (the same item twice) from id collisions (different items sharing an id).
		std::vector<std::pair<LWOOBJID, std::vector<Location>>> Duplicates() const {
			std::vector<std::pair<LWOOBJID, std::vector<Location>>> result(m_Duplicates.begin(), m_Duplicates.end());
			std::ranges::stable_sort(result, [](const auto& a, const auto& b) { return a.second.size() > b.second.size(); });
			return result;
		}

	private:
		struct Entry {
			uint32_t count{};
			Location first;
		};
		std::map<LWOOBJID, Entry> m_Seen;
		std::map<LWOOBJID, std::vector<Location>> m_Duplicates;
	};

	// The places one object id was found, split by what they are
	struct CopyGroups {
		// Copies of the same LOT, one group per LOT found more than once: a real duplicate
		std::vector<std::vector<Location>> duplicates;
		// Whether different LOTs share the id. Old data reused ids across items, so this is an id collision, not a
		// dupe: nothing was copied.
		bool collision{};
	};

	inline CopyGroups Classify(const std::vector<Location>& locations) {
		std::map<LOT, std::vector<Location>> byLot;
		for (const auto& location : locations) byLot[location.lot].push_back(location);
		CopyGroups groups;
		groups.collision = byLot.size() > 1;
		for (auto& [lot, copies] : byLot) {
			if (copies.size() > 1) groups.duplicates.push_back(std::move(copies));
		}
		return groups;
	}

	// What the login migration does to an object id found in more than one place
	struct LoginFix {
		// Once the characters below have logged in, at most one copy still has this id: the problem goes away by itself.
		// False when two or more copies keep the id (their characters already migrated, or they are in mail).
		bool resolves{};
		// Characters holding a copy that gets a new id at their next login, in the order first seen
		std::vector<LWOOBJID> characters;
		// How many of those characters must log in: all of them, or all but one when every copy would get a new id
		// and each of them holds exactly one copy (whoever is left keeps the id alone)
		size_t loginsNeeded{};
	};

	inline LoginFix ResolveOnLogin(const std::vector<Location>& copies) {
		LoginFix fix;
		size_t keepId = 0;
		std::map<LWOOBJID, size_t> copiesPerCharacter;
		for (const auto& copy : copies) {
			if (!copy.NewIdAtLogin()) {
				keepId++;
				continue;
			}
			if (copiesPerCharacter[copy.ownerId]++ == 0) fix.characters.push_back(copy.ownerId);
		}
		fix.resolves = keepId <= 1 && !fix.characters.empty();
		if (!fix.resolves) return fix;
		const bool onePerCharacter = std::ranges::all_of(copiesPerCharacter, [](const auto& entry) { return entry.second == 1; });
		fix.loginsNeeded = keepId == 0 && onePerCharacter && fix.characters.size() > 1 ? fix.characters.size() - 1 : fix.characters.size();
		return fix;
	}

	/**
	 * Characters whose coin income on a day stands out: above `multiplier` times the median income of everyone who
	 * earned coins that day, and above `minimum` so a quiet day with tiny incomes flags nothing.
	 * @param median set to the median income
	 */
	inline std::vector<std::pair<LWOOBJID, int64_t>> UnusualIncome(std::vector<std::pair<LWOOBJID, int64_t>> incomes, int64_t multiplier, int64_t minimum, int64_t& median) {
		median = 0;
		if (incomes.empty()) return {};
		std::vector<int64_t> values;
		for (const auto& [id, value] : incomes) values.push_back(value);
		std::ranges::sort(values);
		const auto middle = values.size() / 2;
		median = values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
		const auto threshold = std::max(minimum, median * multiplier);
		std::vector<std::pair<LWOOBJID, int64_t>> flagged;
		for (const auto& income : incomes) if (income.second > threshold) flagged.push_back(income);
		std::ranges::sort(flagged, [](const auto& a, const auto& b) { return a.second > b.second; });
		return flagged;
	}

	struct ItemSpike {
		LOT lot{};
		int64_t created{};
		int64_t dailyAverage{}; // over the history window, rounded up
	};

	/**
	 * Items created far more than usual on a day: above `multiplier` times their daily average over the history
	 * window and above `minimum`. Items with no history are compared against the minimum alone.
	 */
	inline std::vector<ItemSpike> ItemSpikes(const std::map<LOT, int64_t>& day, const std::map<LOT, int64_t>& history, uint32_t historyDays, int64_t multiplier, int64_t minimum) {
		std::vector<ItemSpike> spikes;
		for (const auto& [lot, created] : day) {
			const auto it = history.find(lot);
			const int64_t total = it == history.end() ? 0 : it->second;
			const int64_t average = historyDays > 0 ? (total + historyDays - 1) / historyDays : 0;
			if (created > std::max(minimum, average * multiplier)) spikes.push_back({ lot, created, average });
		}
		std::ranges::sort(spikes, [](const ItemSpike& a, const ItemSpike& b) { return a.created > b.created; });
		return spikes;
	}

	/**
	 * Clamp a requested report range. Days count from the Unix epoch.
	 * @return {from, to} with from <= to <= today and at most maxDays long; defaults to the last defaultDays days
	 */
	inline std::pair<uint32_t, uint32_t> DayRange(std::optional<uint32_t> from, std::optional<uint32_t> to, uint32_t today, uint32_t defaultDays, uint32_t maxDays) {
		uint32_t end = std::min(to.value_or(today), today);
		uint32_t start = from ? std::min(*from, today) : (end >= defaultDays - 1 ? end - (defaultDays - 1) : 0);
		if (start > end) std::swap(start, end);
		if (end - start + 1 > maxDays) start = end - (maxDays - 1);
		return { start, end };
	}
}
