#ifndef __IECONOMYLEDGER__H__
#define __IECONOMYLEDGER__H__

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "json.hpp"

/**
 * Economy ledger: daily coin/U-score/item flows, player-to-player transfers, where things happen in the world and
 * daily totals of player statistics, written by world servers and read by the dashboard's reports. Days are counted
 * since the Unix epoch (UTC).
 */
class IEconomyLedger {
public:
	enum class eTransferMethod : uint8_t {
		TRADE = 1,
		MAIL_SENT = 2,     // left the sender and is waiting in the mailbox
		MAIL_CLAIMED = 3,  // taken out of the mailbox by the receiver
		INVENTORY_MOVE = 4 // moved between the character's own inventories (from = to = the character)
	};

	// Sources are eLootSourceType values, plus these for changes the game has no loot source for
	static constexpr uint32_t DONATION_SOURCE = 100;        // items given to a donation vendor
	static constexpr uint32_t HARDCORE_DEATH_SOURCE = 101;  // U-score lost on death in hardcore mode
	static constexpr uint32_t HARDCORE_KILL_SOURCE = 102;   // U-score for killing an enemy in hardcore mode

	struct CurrencyFlow {
		uint32_t day{};
		LWOOBJID characterId{};
		uint32_t source{};
		int64_t gained{};
		int64_t spent{};
	};

	struct UScoreFlow {
		uint32_t day{};
		LWOOBJID characterId{};
		uint32_t source{};
		int64_t gained{};
		int64_t lost{};
	};

	struct ItemFlow {
		uint32_t day{};
		LOT lot{};
		uint32_t source{};
		bool gm{};
		int64_t created{};
		int64_t destroyed{};
	};

	enum class eMapEvent : uint8_t {
		ENEMY_KILLS = 1,           // enemy killed by a player
		ITEM_DROPS = 2,            // item dropped as loot
		COIN_DROPS = 3,            // coins dropped as loot (lot 0, quantity = coins)
		PLAYER_DEATHS = 4,         // a player died (lot = what killed them)
		PLAYER_COIN_DROPS = 5,     // coins a player dropped on death (lot 0, quantity = coins)
		SMASHABLES_SMASHED = 6,    // something other than an enemy or player smashed by a player (crates, barrels)
		QUICKBUILDS_COMPLETED = 7, // a player finished a quickbuild (lot = the quickbuild)
		POWERUP_DROPS = 8,         // a powerup (Objects.type "Powerup": life, imagination, armor, ...) dropped (lot = the powerup)
		POWERUP_PICKUPS = 9        // a player picked a powerup up (lot = the powerup); teammates may each pick up one drop
	};

	// World positions are bucketed into square cells of this many units (a minifigure is about 2 wide)
	static constexpr float MAP_CELL_SIZE = 4.0f;

	/**
	 * Property worlds run one instance per property, all on the property's zone, told apart by the clone id (the
	 * owner's property clone). Every other world has clone 0. Map events and player statistics record it, so one
	 * property's data isn't mixed with another's; rows from before that have clone 0 on property zones too.
	 */
	struct MapEvent {
		uint32_t day{};
		uint32_t zone{};
		uint32_t clone{};
		eMapEvent kind{};
		LOT lot{};
		int32_t cellX{};
		int32_t cellZ{};
		int64_t events{};
		int64_t quantity{};
	};

	// A per-character statistic (StatisticID) summed over every player in a zone for a day
	struct PlayerStat {
		uint32_t day{};
		uint32_t zone{};
		uint32_t clone{};
		uint32_t stat{};
		bool gm{};
		int64_t amount{};
	};

	struct ItemTransfer {
		int64_t time{};
		eTransferMethod method{};
		LWOOBJID itemId{};
		LWOOBJID newItemId{};
		LOT lot{};
		uint32_t count{};
		int64_t coins{};
		LWOOBJID fromCharacter{};
		LWOOBJID toCharacter{};
		uint32_t zone{};
		bool merged{};  // newItemId is a stack the receiver already had, which has a history of its own
	};

	// Add a batch of flows (summed into existing daily rows) and transfers in one transaction
	virtual void RecordEconomy(const std::vector<CurrencyFlow>& currency, const std::vector<UScoreFlow>& uscore,
		const std::vector<ItemFlow>& items, const std::vector<ItemTransfer>& transfers, const std::vector<MapEvent>& mapEvents,
		const std::vector<PlayerStat>& stats) = 0;

	// ---- Reports ----

	// Per day and source: {day, source, gained, spent}
	virtual nlohmann::json GetCurrencyFlows(uint32_t fromDay, uint32_t toDay, bool excludeStaff) = 0;

	// Per day and source: {day, source, gained, lost}
	virtual nlohmann::json GetUScoreFlows(uint32_t fromDay, uint32_t toDay, bool excludeStaff) = 0;

	// Per day and source for one item (lot > 0) or all items: {day, source, created, destroyed}
	virtual nlohmann::json GetItemFlows(uint32_t fromDay, uint32_t toDay, LOT lot, bool excludeStaff) = 0;

	// Characters with the largest coin income in the range: {character_id, name, gained, spent}
	virtual nlohmann::json GetTopEarners(uint32_t fromDay, uint32_t toDay, uint32_t limit, bool excludeStaff) = 0;

	// Items created most in the range: {lot, created, destroyed}
	virtual nlohmann::json GetTopItems(uint32_t fromDay, uint32_t toDay, uint32_t limit, bool excludeStaff) = 0;

	// Trades and mail (not inventory moves), newest first, optionally for one character (either side) or one lot; DataTables format
	virtual nlohmann::json GetTransfers(uint32_t start, uint32_t length, LWOOBJID characterId, LOT lot) = 0;

	// Transfers (every method) that involve any of the object ids as item_id or new_item_id, oldest first. Rows have
	// "merged": true/false, or null for rows written before the game recorded it. The trace walks the chain with this.
	virtual nlohmann::json GetTransfersForItems(const std::vector<LWOOBJID>& itemIds) = 0;

	// Which zones and instances a map or statistics query covers
	struct PlaceFilter {
		std::vector<uint32_t> zones;     // empty: every zone
		std::optional<uint32_t> clone;   // nullopt: every instance of those zones; else one clone (0: not a property instance)

		// " AND zone IN (...) AND clone_id = n" for the filter (ids are numbers, so inlining them is safe)
		std::string Sql() const {
			std::string sql;
			if (!zones.empty()) {
				sql += " AND zone IN (";
				for (size_t i = 0; i < zones.size(); i++) sql += (i ? "," : "") + std::to_string(zones[i]);
				sql += ")";
			}
			if (clone) sql += " AND clone_id = " + std::to_string(*clone);
			return sql;
		}
	};

	// Zones and clones with map events of a kind in the range: {zone, clone, events, quantity}
	virtual nlohmann::json GetMapZones(eMapEvent kind, uint32_t fromDay, uint32_t toDay) = 0;

	// LOTs with the most events of a kind in a zone (one clone, or every instance): {lot, events, quantity}
	virtual nlohmann::json GetMapLots(uint32_t zone, std::optional<uint32_t> clone, eMapEvent kind, uint32_t fromDay, uint32_t toDay, uint32_t limit) = 0;

	// Events per cell for a zone (one clone, or every instance) and kind, optionally one LOT (lot > 0): {x, z, events, quantity}
	virtual nlohmann::json GetMapCells(uint32_t zone, std::optional<uint32_t> clone, eMapEvent kind, uint32_t fromDay, uint32_t toDay, LOT lot) = 0;

	// Map events of every kind per day in the places: {day, kind, events, quantity}
	virtual nlohmann::json GetMapEventsPerDay(uint32_t fromDay, uint32_t toDay, const PlaceFilter& place) = 0;

	// Map events of a kind per day, zone, clone and LOT in the places: {day, zone, clone, lot, events, quantity}.
	// For kinds with few LOTs (powerups), at most `limit` rows.
	virtual nlohmann::json GetMapEventsByLot(eMapEvent kind, uint32_t fromDay, uint32_t toDay, const PlaceFilter& place, uint32_t limit) = 0;

	// Player statistics per day in the places: {day, stat, amount}
	virtual nlohmann::json GetPlayerStatsPerDay(uint32_t fromDay, uint32_t toDay, bool excludeStaff, const PlaceFilter& place) = 0;

	// Player statistics per zone and clone: {zone, clone, stat, amount}
	virtual nlohmann::json GetPlayerStatsPerZone(uint32_t fromDay, uint32_t toDay, bool excludeStaff) = 0;

	/**
	 * Who the property clones belong to: {owners: [{clone, character_id, name}] from charinfo.prop_clone_id (a clone
	 * exists before its properties are claimed), properties: [{clone, zone, id, name, owner_id}]}
	 */
	virtual nlohmann::json GetCloneOwners(const std::vector<uint32_t>& clones) = 0;

	struct MailAttachment {
		uint64_t mailId{};
		LWOOBJID receiverId{};
		LWOOBJID itemId{};
		LOT lot{};
		uint32_t count{};
	};

	// Every attachment still waiting to be claimed, for holdings and duplicate scans
	virtual void ForEachMailAttachment(const std::function<void(const MailAttachment&)>& visit) = 0;
};

#endif  //!__IECONOMYLEDGER__H__
