#include "TradingManager.h"
#include "EconomyLedger.h"
#include "dServer.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "TradeMessages.h"
#include "InventoryComponent.h"
#include "ObjectIDManager.h"
#include "Game.h"
#include "Logger.h"
#include "Item.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"
#include <ranges>

namespace {
	std::unique_ptr<Trade> g_EmptyTrade;
}

TradingManager* TradingManager::m_Address = nullptr;

Trade::Trade(LWOOBJID tradeId, LWOOBJID participantA, LWOOBJID participantB) {
	m_TradeId = tradeId;
	m_ParticipantA = participantA;
	m_ParticipantB = participantB;
}

Trade::~Trade() {

}

LWOOBJID Trade::GetTradeId() const {
	return m_TradeId;
}

bool Trade::IsParticipant(LWOOBJID playerId) const {
	return m_ParticipantA == playerId || m_ParticipantB == playerId;
}

LWOOBJID Trade::GetParticipantA() const {
	return m_ParticipantA;
}

LWOOBJID Trade::GetParticipantB() const {
	return m_ParticipantB;
}

Entity* Trade::GetParticipantAEntity() const {
	return Game::entityManager->GetEntity(m_ParticipantA);
}

Entity* Trade::GetParticipantBEntity() const {
	return Game::entityManager->GetEntity(m_ParticipantB);
}

void Trade::SetCoins(LWOOBJID participant, uint64_t coins) {
	if (participant == m_ParticipantA) {
		m_CoinsA = coins;
	} else if (participant == m_ParticipantB) {
		m_CoinsB = coins;
	}
}

void Trade::SetItems(LWOOBJID participant, std::vector<TradeItem> items) {
	if (participant == m_ParticipantA) {
		m_ItemsA = items;
	} else if (participant == m_ParticipantB) {
		m_ItemsB = items;
	}
}

void Trade::SetAccepted(LWOOBJID participant, bool value) {
	if (participant == m_ParticipantA) {
		m_AcceptedA = !value;

		LOG("Accepted from A (%d), B: (%d)", value, m_AcceptedB);

		auto* entityB = GetParticipantBEntity();

		if (entityB != nullptr) {
			GameMessages::ServerTradeAccept accept;
			accept.target = m_ParticipantB;
			accept.bFirst = value;
			accept.Send(entityB->GetSystemAddress());
		}
	} else if (participant == m_ParticipantB) {
		m_AcceptedB = !value;

		LOG("Accepted from B (%d), A: (%d)", value, m_AcceptedA);

		auto* entityA = GetParticipantAEntity();

		if (entityA != nullptr) {
			GameMessages::ServerTradeAccept accept;
			accept.target = m_ParticipantA;
			accept.bFirst = value;
			accept.Send(entityA->GetSystemAddress());
		}
	}

	if (m_AcceptedA && m_AcceptedB) {
		auto* entityB = GetParticipantBEntity();

		if (entityB != nullptr) {
			GameMessages::ServerTradeAccept accept;
			accept.target = m_ParticipantB;
			accept.bFirst = false;
			accept.Send(entityB->GetSystemAddress());
		} else {
			return;
		}

		auto* entityA = GetParticipantAEntity();

		if (entityA != nullptr) {
			GameMessages::ServerTradeAccept accept;
			accept.target = m_ParticipantA;
			accept.bFirst = false;
			accept.Send(entityA->GetSystemAddress());
		} else {
			return;
		}

		Complete();
		TradingManager::Instance()->CancelTrade(LWOOBJID_EMPTY, m_TradeId, false);
	}
}

void Trade::Complete() {
	auto* entityA = GetParticipantAEntity();
	auto* entityB = GetParticipantBEntity();

	if (entityA == nullptr || entityB == nullptr) return;

	auto* inventoryA = entityA->GetComponent<InventoryComponent>();
	auto* inventoryB = entityB->GetComponent<InventoryComponent>();
	auto* missionsA = entityA->GetComponent<MissionComponent>();
	auto* missionsB = entityB->GetComponent<MissionComponent>();
	auto* characterA = entityA->GetCharacter();
	auto* characterB = entityB->GetCharacter();

	if (inventoryA == nullptr || inventoryB == nullptr || characterA == nullptr || characterB == nullptr || missionsA == nullptr || missionsB == nullptr) return;

	// First verify both players have the coins and items requested for the trade.
	if (characterA->GetCoins() < m_CoinsA || characterB->GetCoins() < m_CoinsB) {
		LOG("Possible coin trade cheating attempt! Aborting trade.");
		return;
	}

	for (const auto& tradeItem : m_ItemsA) {
		auto* itemToRemove = inventoryA->FindItemById(tradeItem.itemId);
		if (itemToRemove) {
			if (itemToRemove->GetCount() < tradeItem.itemCount) {
				LOG("Possible cheating attempt from %s in trading!!! Aborting trade", characterA->GetName().c_str());
				return;
			}
		} else {
			LOG("Possible cheating attempt from %s in trading due to item not being available!!!", characterA->GetName().c_str());
			return;
		}
	}

	for (const auto& tradeItem : m_ItemsB) {
		auto* itemToRemove = inventoryB->FindItemById(tradeItem.itemId);
		if (itemToRemove) {
			if (itemToRemove->GetCount() < tradeItem.itemCount) {
				LOG("Possible cheating attempt from %s in trading!!! Aborting trade", characterB->GetName().c_str());
				return;
			}
		} else {
			LOG("Possible cheating attempt from %s in trading due to item not being available!!!  Aborting trade", characterB->GetName().c_str());
			return;
		}
	}

	// Now actually do the trade. Nothing is created or destroyed, so the economy ledger records transfers instead.
	EconomyLedger::ScopedItemTransfer transfer;
	characterA->SetCoins(characterA->GetCoins() - m_CoinsA + m_CoinsB, eLootSourceType::TRADE);
	characterB->SetCoins(characterB->GetCoins() - m_CoinsB + m_CoinsA, eLootSourceType::TRADE);

	const auto zone = Game::server ? Game::server->GetZoneID() : 0;
	const auto recordCoins = [zone](const uint64_t coins, const LWOOBJID from, const LWOOBJID to) {
		if (coins == 0) return;
		EconomyLedger::RecordTransfer({ .method = IEconomyLedger::eTransferMethod::TRADE, .coins = static_cast<int64_t>(coins),
			.fromCharacter = from, .toCharacter = to, .zone = zone });
	};
	recordCoins(m_CoinsA, characterA->GetID(), characterB->GetID());
	recordCoins(m_CoinsB, characterB->GetID(), characterA->GetID());

	const auto giveItems = [zone](const std::vector<TradeItem>& items, InventoryComponent* from, MissionComponent* fromMissions,
		InventoryComponent* to, const LWOOBJID fromCharacter, const LWOOBJID toCharacter) {
		for (const auto& tradeItem : items) {
			auto* itemToRemove = from->FindItemById(tradeItem.itemId);
			if (!itemToRemove) continue;

			// A whole item keeps its data (subkey, config) but gets a new object id, as upstream: reusing the id would
			// leave two objects with one id if the server stopped between the two players' saves. A stack merges into
			// the other player's stack. new_item_id records where the items ended up, so the chain can be followed.
			const bool whole = itemToRemove->GetCount() == tradeItem.itemCount;
			const auto config = itemToRemove->GetConfig();
			const auto subKey = itemToRemove->GetSubKey();
			const auto bound = itemToRemove->GetBound();
			itemToRemove->SetCount(itemToRemove->GetCount() - tradeItem.itemCount);
			fromMissions->Progress(eMissionTaskType::GATHER, tradeItem.itemLot, LWOOBJID_EMPTY, "", -static_cast<int32_t>(tradeItem.itemCount));
			const auto received = whole
				? to->ReceiveItem(LWOOBJID_EMPTY, tradeItem.itemLot, tradeItem.itemCount, eLootSourceType::TRADE, config, subKey, bound)
				: to->ReceiveItem(LWOOBJID_EMPTY, tradeItem.itemLot, tradeItem.itemCount, eLootSourceType::TRADE);

			EconomyLedger::RecordTransfer({ .method = IEconomyLedger::eTransferMethod::TRADE, .itemId = tradeItem.itemId, .newItemId = received.id,
				.lot = tradeItem.itemLot, .count = tradeItem.itemCount, .fromCharacter = fromCharacter, .toCharacter = toCharacter, .zone = zone,
				.merged = received.merged });
		}
	};
	giveItems(m_ItemsA, inventoryA, missionsA, inventoryB, characterA->GetID(), characterB->GetID());
	giveItems(m_ItemsB, inventoryB, missionsB, inventoryA, characterB->GetID(), characterA->GetID());

	characterA->SaveXMLToDatabase();
	characterB->SaveXMLToDatabase();
	return;
}

void Trade::Cancel(const LWOOBJID canceller) {
	auto* entityA = GetParticipantAEntity();
	auto* entityB = GetParticipantBEntity();

	if (entityA == nullptr || entityB == nullptr) return;

	if (entityA->GetObjectID() != canceller || canceller == LWOOBJID_EMPTY) {
		GameMessages::ServerTradeCancel cancel;
		cancel.target = entityA->GetObjectID();
		cancel.Send(entityA->GetSystemAddress());
	}
	if (entityB->GetObjectID() != canceller || canceller == LWOOBJID_EMPTY) {
		GameMessages::ServerTradeCancel cancel;
		cancel.target = entityB->GetObjectID();
		cancel.Send(entityB->GetSystemAddress());
	}
}

void Trade::SendUpdateToOther(LWOOBJID participant) {
	Entity* other = nullptr;
	Entity* self = nullptr;
	uint64_t coins;
	std::vector<TradeItem> itemIds;

	LOG("Attempting to send trade update");

	if (participant == m_ParticipantA) {
		other = GetParticipantBEntity();
		self = GetParticipantAEntity();
		coins = m_CoinsA;
		itemIds = m_ItemsA;
	} else if (participant == m_ParticipantB) {
		other = GetParticipantAEntity();
		self = GetParticipantBEntity();
		coins = m_CoinsB;
		itemIds = m_ItemsB;
	} else {
		return;
	}

	if (other == nullptr || self == nullptr) return;

	std::vector<TradeItem> items{};

	auto* inventoryComponent = self->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) return;

	for (const auto tradeItem : itemIds) {
		auto* item = inventoryComponent->FindItemById(tradeItem.itemId);

		if (item == nullptr) return;

		if (tradeItem.itemCount > item->GetCount()) return;

		items.push_back(tradeItem);
	}

	LOG("Sending trade update");

	GameMessages::ServerTradeUpdate update;
	update.target = other->GetObjectID();
	update.i64Currency = coins;
	for (const auto& item : items) {
		GameMessages::TradeItemEntry entry;
		entry.key = item.itemId;
		entry.itemID = item.itemId;
		entry.templateID = item.itemLot;
		entry.count = item.itemCount; // the count is always written, even when it is 1
		update.inventoryMap.push_back(entry);
	}
	update.Send(other->GetSystemAddress());
}

const std::unique_ptr<Trade>& TradingManager::GetTrade(LWOOBJID tradeId) const {
	const auto& pair = trades.find(tradeId);

	if (pair == trades.end()) return g_EmptyTrade;

	return pair->second;
}

const std::unique_ptr<Trade>& TradingManager::GetPlayerTrade(LWOOBJID playerId) const {
	for (const auto& trade : trades | std::views::values) {
		if (trade->IsParticipant(playerId)) {
			return trade;
		}
	}

	return g_EmptyTrade;
}

void TradingManager::CancelTrade(const LWOOBJID canceller, LWOOBJID tradeId, const bool sendCancelMessage) {
	const auto& trade = GetTrade(tradeId);

	if (trade == nullptr) return;

	if (sendCancelMessage) trade->Cancel(canceller);

	trades.erase(tradeId);
}

void TradingManager::NewTrade(LWOOBJID participantA, LWOOBJID participantB) {
	const LWOOBJID tradeId = ObjectIDManager::GenerateObjectID();

	trades.insert_or_assign(tradeId, std::make_unique<Trade>(tradeId, participantA, participantB));

	LOG("Created new trade between (%llu) <-> (%llu)", participantA, participantB);
}
