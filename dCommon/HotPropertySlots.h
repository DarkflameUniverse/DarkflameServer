#ifndef __HOTPROPERTYSLOTS__H__
#define __HOTPROPERTYSLOTS__H__

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * The "Today's Top Properties" panel of the client's news screen (res/ui/ingame/news.gfx).
 *
 * The screen sends GetHotPropertyData (GM 1511) when it opens; the server answers with
 * NewsSendHotPropertiesInfoToClient (GM 1510), a list of properties. For each entry the client looks up the
 * PropertyTemplate row by the entry's template id (LWOCharacterComponent::HotPropertyData, 0x00cf83f0 in 1.10.64)
 * and the UI puts it in the slot for that row's spawnName. The UI only has four slots, one per small property
 * world, and ignores every other spawn name. Live always sent four entries, one per slot, in this order.
 *
 * A slot only displays what it is sent: the name, owner, reputation and so on of the entry (the property and clone
 * ids are not used). So a slot can show a property of any property world, sent with the slot's template id; only the
 * tooltip still names the slot's own world (it is the slot's fixed zone name and its template's map description).
 */
namespace HotPropertySlots {
	// The slots of news.gfx, in the order live sent them
	constexpr std::array<std::string_view, 4> NEWS_SPAWN_NAMES{ "AGSmallProperty", "NSSmallProperty", "GFSmallProperty", "FVSmallProperty" };

	// What a slot shows
	enum class eMode : uint8_t {
		AUTO = 0,   // the approved public property of the slot's location with the most reputation (not shown in another slot)
		PICKED = 1, // the property staff chose (AUTO again whenever it can't be shown any more)
		EMPTY = 2,  // nothing: the slot stays locked
	};

	inline std::string_view ModeName(eMode mode) {
		switch (mode) {
		case eMode::PICKED: return "picked";
		case eMode::EMPTY: return "empty";
		default: return "auto";
		}
	}

	inline std::optional<eMode> ParseMode(std::string_view name) {
		if (name == "auto") return eMode::AUTO;
		if (name == "picked") return eMode::PICKED;
		if (name == "empty") return eMode::EMPTY;
		return std::nullopt;
	}

	// A mode as stored in the database; anything unknown is AUTO
	inline eMode ModeFromInt(int64_t value) {
		return value == 1 ? eMode::PICKED : value == 2 ? eMode::EMPTY : eMode::AUTO;
	}

	struct TemplateRow { // PropertyTemplate
		uint32_t id{};
		uint32_t mapId{};
		std::string spawnName;
		// Rent (PropertyRent.h) and reputation (PropertyReputation.h) of the template
		int64_t minimumPrice{};
		int32_t rentDuration{};
		int32_t durationType{};
		int32_t reputationPerMinute{};
	};

	struct EntranceRow { // PropertyEntranceComponent
		uint32_t mapId{};
		std::string propertyName;
	};

	struct Slot {
		uint32_t templateId{}; // PropertyTemplate id, sent to the client as the entry's template id
		uint32_t mapId{};      // the property world
		std::string spawnName;
		bool operator==(const Slot&) const = default;
	};

	/**
	 * The slots, in NEWS_SPAWN_NAMES order: for each spawn name the news screen knows, the PropertyTemplate row of
	 * that spawn name whose map a property entrance (the rocket pads players launch to properties from) leads to.
	 * When several rows qualify the lowest id is used. A spawn name without such a row has no slot.
	 */
	inline std::vector<Slot> ResolveSlots(const std::vector<TemplateRow>& templates, const std::vector<EntranceRow>& entrances) {
		std::vector<Slot> slots;
		for (const auto spawnName : NEWS_SPAWN_NAMES) {
			const TemplateRow* best = nullptr;
			for (const auto& row : templates) {
				if (row.spawnName != spawnName) continue;
				const bool entered = std::any_of(entrances.begin(), entrances.end(), [&](const EntranceRow& entrance) {
					return entrance.mapId == row.mapId && entrance.propertyName == spawnName;
				});
				if (entered && (!best || row.id < best->id)) best = &row;
			}
			if (best) slots.push_back({ best->id, best->mapId, best->spawnName });
		}
		return slots;
	}

	/**
	 * The property worlds a slot can show a property of (its location): every PropertyTemplate map a property entrance
	 * leads to, small and medium, ordered by their lowest template id.
	 */
	inline std::vector<uint32_t> PropertyWorlds(const std::vector<TemplateRow>& templates, const std::vector<EntranceRow>& entrances) {
		std::vector<const TemplateRow*> rows;
		for (const auto& row : templates) {
			const bool entered = std::any_of(entrances.begin(), entrances.end(), [&](const EntranceRow& entrance) {
				return entrance.mapId == row.mapId && entrance.propertyName == row.spawnName;
			});
			if (entered) rows.push_back(&row);
		}
		std::sort(rows.begin(), rows.end(), [](const TemplateRow* a, const TemplateRow* b) { return a->id < b->id; });
		std::vector<uint32_t> worlds;
		for (const auto* row : rows) {
			if (std::find(worlds.begin(), worlds.end(), row->mapId) == worlds.end()) worlds.push_back(row->mapId);
		}
		return worlds;
	}

	/**
	 * The PropertyTemplate row a property world uses: the lowest id among its rows a property entrance leads to (as
	 * PropertyWorlds), or its lowest id row when no entrance leads there. nullptr when the map has no row.
	 */
	inline const TemplateRow* WorldTemplate(const std::vector<TemplateRow>& templates, const std::vector<EntranceRow>& entrances, uint32_t mapId) {
		const TemplateRow* entered = nullptr;
		const TemplateRow* any = nullptr;
		for (const auto& row : templates) {
			if (row.mapId != mapId) continue;
			if (!any || row.id < any->id) any = &row;
			const bool isEntered = std::any_of(entrances.begin(), entrances.end(), [&](const EntranceRow& entrance) {
				return entrance.mapId == row.mapId && entrance.propertyName == row.spawnName;
			});
			if (isEntered && (!entered || row.id < entered->id)) entered = &row;
		}
		return entered ? entered : any;
	}

	/**
	 * A slot's location: always the slot's own world. The news screen's tooltip names the slot's world whatever
	 * property is sent for it, so a property of another world would be shown under the wrong name. A location stored
	 * by an older version (featured_properties.zone_id) is kept in the table but not used.
	 */
	inline uint32_t Location(uint32_t /*stored*/, const Slot& slot, const std::vector<uint32_t>& /*worlds*/) {
		return slot.mapId;
	}

	constexpr int32_t PRIVACY_PUBLIC = 2; // ePropertyPrivacyOption::Public

	// Whether a property may be featured in a slot: approved by a moderator, public, and in the slot's location
	inline bool Featurable(uint32_t modApproved, int32_t privacyOption, uint32_t propertyZone, uint32_t slotMap) {
		return modApproved == 1 && privacyOption == PRIVACY_PUBLIC && propertyZone == slotMap;
	}

	// What a slot was set to show (featured_properties), with its location resolved
	struct Choice {
		eMode mode{ eMode::AUTO };
		uint32_t mapId{};        // the location: the world whose properties it shows
		int64_t propertyId{};    // the picked property (PICKED)
	};

	// An approved public property that can be shown
	struct Candidate {
		int64_t propertyId{};
		uint32_t mapId{};
		uint64_t reputation{};
	};

	// What a slot shows
	struct Showing {
		std::optional<int64_t> propertyId; // none: the slot stays locked
		bool pickFellBack{};                // PICKED, but the pick can't be shown (any more), so it shows its AUTO
		bool operator==(const Showing&) const = default;
	};

	// How many of each location's best properties are enough to resolve the slots: every slot could draw from one world
	constexpr uint32_t CANDIDATES_PER_WORLD = NEWS_SPAWN_NAMES.size();

	/**
	 * The worlds whose best CANDIDATES_PER_WORLD candidates Resolve needs (full auto: every slot's world). The picked
	 * properties have to be added to the candidates as well, when they may be shown in their slot's location.
	 */
	inline std::vector<uint32_t> CandidateWorlds(const std::vector<Choice>& choices, bool fullAuto) {
		std::vector<uint32_t> worlds;
		for (const auto& choice : choices) {
			if ((fullAuto || choice.mode != eMode::EMPTY) && std::find(worlds.begin(), worlds.end(), choice.mapId) == worlds.end()) worlds.push_back(choice.mapId);
		}
		return worlds;
	}

	/**
	 * What each slot shows (one entry per choice, in slot order), so that no property is shown twice.
	 * candidates: the properties that may be shown (see CandidateWorlds); a property listed twice counts once.
	 * Full auto ignores the modes and picks: each slot shows its location's candidate with the most reputation, in slot
	 * order.
	 * Otherwise the picks come first, in slot order: a pick is shown when it is a candidate of the slot's location that
	 * an earlier slot doesn't show, else the slot falls back to AUTO. Then the AUTO slots, in slot order, each show the
	 * candidate of their location with the most reputation that isn't shown yet (two on one world: its #1 and #2).
	 * A slot with nothing left to show, and every EMPTY slot, shows nothing.
	 */
	inline std::vector<Showing> Resolve(const std::vector<Choice>& choices, bool fullAuto, std::vector<Candidate> candidates) {
		std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.reputation > b.reputation; });
		std::vector<Showing> showing(choices.size());
		std::vector<int64_t> used;
		const auto isUsed = [&](int64_t id) { return std::find(used.begin(), used.end(), id) != used.end(); };
		const auto best = [&](std::optional<uint32_t> mapId) -> std::optional<int64_t> {
			for (const auto& candidate : candidates) {
				if ((!mapId || candidate.mapId == *mapId) && !isUsed(candidate.propertyId)) return candidate.propertyId;
			}
			return std::nullopt;
		};
		const auto show = [&](size_t slot, std::optional<int64_t> id) {
			showing[slot].propertyId = id;
			if (id) used.push_back(*id);
		};

		if (fullAuto) {
			for (size_t i = 0; i < choices.size(); i++) show(i, best(choices[i].mapId));
			return showing;
		}

		std::vector<size_t> autos;
		for (size_t i = 0; i < choices.size(); i++) {
			const auto& choice = choices[i];
			if (choice.mode == eMode::EMPTY) continue;
			if (choice.mode == eMode::PICKED) {
				const bool shown = !isUsed(choice.propertyId) && std::any_of(candidates.begin(), candidates.end(), [&](const Candidate& candidate) {
					return candidate.propertyId == choice.propertyId && candidate.mapId == choice.mapId;
				});
				if (shown) {
					show(i, choice.propertyId);
					continue;
				}
				showing[i].pickFellBack = true;
			}
			autos.push_back(i);
		}
		for (const auto i : autos) show(i, best(choices[i].mapId));
		return showing;
	}

	/**
	 * Which entries to send, by index. news.gfx always walks four entries and, for an entry that is missing, reuses
	 * the slot of the entry before it, filling it with "undefined". So fewer than four entries are padded by
	 * repeating the last one, which only fills its own slot again. None: nothing to send, every slot stays locked.
	 */
	inline std::vector<size_t> NewsOrder(size_t count) {
		std::vector<size_t> order;
		for (size_t i = 0; i < count; i++) order.push_back(i);
		while (count > 0 && order.size() < NEWS_SPAWN_NAMES.size()) order.push_back(count - 1);
		return order;
	}
};

#endif  //!__HOTPROPERTYSLOTS__H__
