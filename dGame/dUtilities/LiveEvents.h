#ifndef __LIVEEVENTS__H__
#define __LIVEEVENTS__H__

#include <cstdint>
#include <string>

#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "LiveOpsRules.h"

class Entity;

/**
 * Live events and community challenges as this world server runs them (see ILiveOps and the dashboard's Live Events
 * and Challenges pages). The dashboard stores them; each world loads what applies to its zone and instance when it
 * starts and when told to reload (ePlayerAction::RELOAD_LIVE_OPS), and ends its part of an event on its own at the end
 * time, cleaning up everything it spawned.
 *
 * The hooks below sit where the game grants coins, U-score and loot and records statistics. They only read a value
 * that is 1 (or false) while nothing is running, so they cost nothing and change nothing then.
 */
namespace LiveEvents {
	namespace Detail {
		extern LiveOpsRules::Multipliers g_Multipliers;
		extern bool g_Counting;
		void CountStat(const Entity* player, uint32_t stat, int64_t amount);
		void CountMapEvent(const Entity* player, uint8_t kind, LOT lot, int64_t quantity);
	}

	// Coins dropped as loot, times a running coin bonus
	inline uint32_t ScaleCoins(uint32_t coins) {
		return Detail::g_Multipliers.coins == 1.0f ? coins : LiveOpsRules::Scale(coins, Detail::g_Multipliers.coins);
	}

	// U-score from a mission or achievement, times a running U-score bonus
	inline int64_t ScaleUScore(int64_t uscore) {
		return Detail::g_Multipliers.uscore == 1.0f ? uscore : LiveOpsRules::Scale(uscore, Detail::g_Multipliers.uscore);
	}

	// What loot matrix drop chances are multiplied by (1: no bonus)
	inline float LootChanceMultiplier() {
		return Detail::g_Multipliers.lootChance;
	}

	// A player's statistic went up (server-side only; what the client reports doesn't count)
	inline void OnStat(const Entity* player, uint32_t stat, uint64_t amount) {
		if (Detail::g_Counting) Detail::CountStat(player, stat, static_cast<int64_t>(amount));
	}

	// Something a player did was recorded on the map (IEconomyLedger::eMapEvent)
	inline void OnMapEvent(const Entity* player, uint8_t kind, LOT lot, int64_t quantity) {
		if (Detail::g_Counting && player) Detail::CountMapEvent(player, kind, lot, quantity);
	}

	// World server loop
	void Update();

	// Load the running events and open challenges again; answers 1 (0 in character select)
	uint32_t Reload();

	// World shutting down: write what is left and mark this instance's part of events closed
	void Shutdown();

	// /challenge [claim]: community challenges and this world's live events, and the rewards waiting
	void ChallengeCommand(Entity* entity, const SystemAddress& sysAddr, const std::string args);
}

#endif  //!__LIVEEVENTS__H__
