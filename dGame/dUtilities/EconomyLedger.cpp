#include "EconomyLedger.h"
#include "DashboardNotify.h"
#include "LiveEvents.h"

#include <cmath>
#include <ctime>

#include "Character.h"
#include "Database.h"
#include "Entity.h"
#include "Game.h"
#include "Logger.h"
#include "User.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "eGameMasterLevel.h"

namespace {
	EconomyLedger::Accumulator g_Pending;
	uint32_t g_TransferDepth = 0;

	// Staff items are flagged so reports can leave out items spawned by moderators and developers
	bool IsStaff(const Entity* owner) {
		const auto* character = owner ? owner->GetCharacter() : nullptr;
		const auto* user = character ? character->GetParentUser() : nullptr;
		return user && user->GetMaxGMLevel() >= eGameMasterLevel::MODERATOR;
	}
}

namespace EconomyLedger {
	void Accumulator::AddCoins(uint32_t day, LWOOBJID characterId, uint32_t source, int64_t delta) {
		if (delta == 0) return;
		auto& [gained, spent] = m_Currency[{ day, characterId, source }];
		(delta > 0 ? gained : spent) += delta > 0 ? delta : -delta;
	}

	void Accumulator::AddUScore(uint32_t day, LWOOBJID characterId, uint32_t source, int64_t delta) {
		if (delta == 0) return;
		auto& [gained, lost] = m_UScore[{ day, characterId, source }];
		(delta > 0 ? gained : lost) += delta > 0 ? delta : -delta;
	}

	void Accumulator::AddItems(uint32_t day, LOT lot, uint32_t source, bool gm, int64_t delta) {
		if (delta == 0) return;
		auto& [created, destroyed] = m_Items[{ day, lot, source, gm }];
		(delta > 0 ? created : destroyed) += delta > 0 ? delta : -delta;
	}

	void Accumulator::AddTransfer(const IEconomyLedger::ItemTransfer& transfer) {
		m_Transfers.push_back(transfer);
	}

	void Accumulator::AddMapEvent(uint32_t day, uint32_t zone, uint32_t clone, IEconomyLedger::eMapEvent kind, LOT lot, float x, float z, int64_t quantity) {
		const auto cellX = static_cast<int32_t>(std::floor(x / IEconomyLedger::MAP_CELL_SIZE));
		const auto cellZ = static_cast<int32_t>(std::floor(z / IEconomyLedger::MAP_CELL_SIZE));
		auto& [events, total] = m_MapEvents[{ day, zone, clone, static_cast<uint8_t>(kind), lot, cellX, cellZ }];
		events++;
		total += quantity;
	}

	void Accumulator::AddStat(uint32_t day, uint32_t zone, uint32_t clone, uint32_t stat, bool gm, int64_t amount) {
		if (amount <= 0) return;
		m_Stats[{ day, zone, clone, stat, gm }] += amount;
	}

	bool Accumulator::Empty() const {
		return m_Currency.empty() && m_UScore.empty() && m_Items.empty() && m_Transfers.empty() && m_MapEvents.empty() && m_Stats.empty();
	}

	Accumulator::Batch Accumulator::Take() {
		Batch batch;
		for (const auto& [key, value] : m_Currency) {
			const auto& [day, character, source] = key;
			batch.currency.push_back({ day, character, source, value.first, value.second });
		}
		for (const auto& [key, value] : m_UScore) {
			const auto& [day, character, source] = key;
			batch.uscore.push_back({ day, character, source, value.first, value.second });
		}
		for (const auto& [key, value] : m_Items) {
			const auto& [day, lot, source, gm] = key;
			batch.items.push_back({ day, lot, source, gm, value.first, value.second });
		}
		for (const auto& [key, value] : m_MapEvents) {
			const auto& [day, zone, clone, kind, lot, cellX, cellZ] = key;
			batch.mapEvents.push_back({ day, zone, clone, static_cast<IEconomyLedger::eMapEvent>(kind), lot, cellX, cellZ, value.first, value.second });
		}
		for (const auto& [key, amount] : m_Stats) {
			const auto& [day, zone, clone, stat, gm] = key;
			batch.stats.push_back({ day, zone, clone, stat, gm, amount });
		}
		batch.transfers = std::move(m_Transfers);
		m_Currency.clear();
		m_UScore.clear();
		m_Items.clear();
		m_Transfers.clear();
		m_MapEvents.clear();
		m_Stats.clear();
		return batch;
	}

	// The instance's clone id: a property's (the owner's property clone) on property worlds, 0 on every other world
	uint32_t CurrentClone() {
		return Game::zoneManager ? Game::zoneManager->GetZoneID().GetCloneID() : 0;
	}

	uint32_t Today() {
		return static_cast<uint32_t>(std::time(nullptr) / (24 * 60 * 60));
	}

	void RecordCoins(LWOOBJID characterId, int64_t delta, eLootSourceType source) {
		g_Pending.AddCoins(Today(), characterId, static_cast<uint32_t>(source), delta);
	}

	void RecordUScore(LWOOBJID characterId, int64_t delta, uint32_t source) {
		g_Pending.AddUScore(Today(), characterId, source, delta);
	}

	void RecordItems(const Entity* owner, LOT lot, int64_t delta, uint32_t source) {
		if (g_TransferDepth > 0) return;
		// Moves between a player's own inventories are not creation or destruction
		if (source == static_cast<uint32_t>(eLootSourceType::RELOCATE)) return;
		RecordItemsUnsuppressed(owner, lot, delta, source);
	}

	void RecordItemsUnsuppressed(const Entity* owner, LOT lot, int64_t delta, uint32_t source) {
		if (!owner || !owner->IsPlayer()) return;
		g_Pending.AddItems(Today(), lot, source, IsStaff(owner), delta);
	}

	void RecordTransfer(IEconomyLedger::ItemTransfer transfer) {
		if (transfer.time == 0) transfer.time = std::time(nullptr);
		g_Pending.AddTransfer(transfer);
	}

	void RecordMapEvent(IEconomyLedger::eMapEvent kind, LOT lot, const NiPoint3& position, int64_t quantity, const Entity* player) {
		if (!Game::server) return;
		g_Pending.AddMapEvent(Today(), Game::server->GetZoneID(), CurrentClone(), kind, lot, position.x, position.z, quantity);
		if (player && player->IsPlayer()) LiveEvents::OnMapEvent(player, static_cast<uint8_t>(kind), lot, quantity);
	}

	void RecordStat(const Entity* player, StatisticID stat, uint64_t amount) {
		if (!Game::server || !player || !player->IsPlayer()) return;
		g_Pending.AddStat(Today(), Game::server->GetZoneID(), CurrentClone(), static_cast<uint32_t>(stat), IsStaff(player), static_cast<int64_t>(amount));
		LiveEvents::OnStat(player, static_cast<uint32_t>(stat), amount);
	}

	void Flush() {
		if (g_Pending.Empty()) return;
		auto batch = g_Pending.Take();
		try {
			Database::Get()->RecordEconomy(batch.currency, batch.uscore, batch.items, batch.transfers, batch.mapEvents, batch.stats);
			DashboardNotify::Changed("economy");
		} catch (const std::exception& ex) {
			// Losing a few seconds of report data is better than taking the world server down
			LOG("Failed to write economy ledger: %s", ex.what());
		}
	}

	ScopedItemTransfer::ScopedItemTransfer() {
		g_TransferDepth++;
	}

	ScopedItemTransfer::~ScopedItemTransfer() {
		g_TransferDepth--;
	}
}
