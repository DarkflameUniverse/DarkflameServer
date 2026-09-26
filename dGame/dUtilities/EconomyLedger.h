#ifndef __ECONOMYLEDGER__H__
#define __ECONOMYLEDGER__H__

#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

#include "dCommonVars.h"
#include "NiPoint3.h"
#include "eLootSourceType.h"
#include "StatisticID.h"
#include "IEconomyLedger.h"

class Entity;

/**
 * Records where coins, U-score and items come from and go to, where things happen in the world and daily totals of
 * player statistics, for the dashboard's reports.
 *
 * Changes are summed in memory per day and source and written in batches (Flush), so a coin pickup costs a
 * map update rather than a database write. Player-to-player transfers are kept individually, with object ids,
 * so an item can be traced from owner to owner.
 */
namespace EconomyLedger {
	constexpr uint32_t DONATION_SOURCE = IEconomyLedger::DONATION_SOURCE;
	constexpr uint32_t HARDCORE_DEATH_SOURCE = IEconomyLedger::HARDCORE_DEATH_SOURCE;
	constexpr uint32_t HARDCORE_KILL_SOURCE = IEconomyLedger::HARDCORE_KILL_SOURCE;

	/**
	 * Pure aggregation, separated from the game so it can be unit tested.
	 */
	class Accumulator {
	public:
		void AddCoins(uint32_t day, LWOOBJID characterId, uint32_t source, int64_t delta);
		void AddUScore(uint32_t day, LWOOBJID characterId, uint32_t source, int64_t delta);
		void AddItems(uint32_t day, LOT lot, uint32_t source, bool gm, int64_t delta);
		void AddTransfer(const IEconomyLedger::ItemTransfer& transfer);
		void AddMapEvent(uint32_t day, uint32_t zone, uint32_t clone, IEconomyLedger::eMapEvent kind, LOT lot, float x, float z, int64_t quantity);
		void AddStat(uint32_t day, uint32_t zone, uint32_t clone, uint32_t stat, bool gm, int64_t amount);

		bool Empty() const;

		struct Batch {
			std::vector<IEconomyLedger::CurrencyFlow> currency;
			std::vector<IEconomyLedger::UScoreFlow> uscore;
			std::vector<IEconomyLedger::ItemFlow> items;
			std::vector<IEconomyLedger::ItemTransfer> transfers;
			std::vector<IEconomyLedger::MapEvent> mapEvents;
			std::vector<IEconomyLedger::PlayerStat> stats;
		};

		// Everything accumulated so far, leaving the accumulator empty
		Batch Take();

	private:
		std::map<std::tuple<uint32_t, LWOOBJID, uint32_t>, std::pair<int64_t, int64_t>> m_Currency;
		std::map<std::tuple<uint32_t, LWOOBJID, uint32_t>, std::pair<int64_t, int64_t>> m_UScore;
		std::map<std::tuple<uint32_t, LOT, uint32_t, bool>, std::pair<int64_t, int64_t>> m_Items;
		std::vector<IEconomyLedger::ItemTransfer> m_Transfers;
		// day, zone, clone, kind, lot, cell x, cell z
		std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint8_t, LOT, int32_t, int32_t>, std::pair<int64_t, int64_t>> m_MapEvents;
		// day, zone, clone, stat, gm
		std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, bool>, int64_t> m_Stats;
	};

	void RecordCoins(LWOOBJID characterId, int64_t delta, eLootSourceType source);
	void RecordUScore(LWOOBJID characterId, int64_t delta, uint32_t source);

	// Items created (positive delta) or destroyed (negative) in a player's inventory
	void RecordItems(const Entity* owner, LOT lot, int64_t delta, uint32_t source);

	// RecordItems that also counts inside a ScopedItemTransfer, for sinks reached through an inventory move
	void RecordItemsUnsuppressed(const Entity* owner, LOT lot, int64_t delta, uint32_t source);

	void RecordTransfer(IEconomyLedger::ItemTransfer transfer);

	// Where something happened in this world, for the dashboard's heatmaps. Recorded with this world's zone and clone
	// (a property's instance; 0 elsewhere).
	// player: whose doing it was, when known (counts for community challenges)
	void RecordMapEvent(IEconomyLedger::eMapEvent kind, LOT lot, const NiPoint3& position, int64_t quantity = 1, const Entity* player = nullptr);

	// A player's statistic went up (CharacterComponent::UpdatePlayerStatistic), summed per day, zone and clone
	void RecordStat(const Entity* player, StatisticID stat, uint64_t amount);

	// Write everything recorded so far to the database. Called periodically and on shutdown.
	void Flush();

	/**
	 * While alive, item counts are not recorded as created/destroyed. Used when items only change hands or
	 * inventories (trades, mail, moving to the vault), which are recorded as transfers or not at all.
	 */
	class ScopedItemTransfer {
	public:
		ScopedItemTransfer();
		~ScopedItemTransfer();
		ScopedItemTransfer(const ScopedItemTransfer&) = delete;
		ScopedItemTransfer& operator=(const ScopedItemTransfer&) = delete;
	};

	uint32_t Today();
}

#endif  //!__ECONOMYLEDGER__H__
