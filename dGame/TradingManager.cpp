#include "TradingManager.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "InventoryComponent.h"
#include "../dWorldServer/ObjectIDManager.h"
#include "Game.h"
#include "dLogger.h"
#include "Item.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"

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

		Game::logger->Log("Trade", "Accepted from A (%d), B: (%d)", value, m_AcceptedB);

		auto* entityB = GetParticipantBEntity();

		if (entityB != nullptr) {
			GameMessages::SendServerTradeAccept(m_ParticipantB, value, entityB->GetSystemAddress());
		}
	} else if (participant == m_ParticipantB) {
		m_AcceptedB = !value;

		Game::logger->Log("Trade", "Accepted from B (%d), A: (%d)", value, m_AcceptedA);

		auto* entityA = GetParticipantAEntity();

		if (entityA != nullptr) {
			GameMessages::SendServerTradeAccept(m_ParticipantA, value, entityA->GetSystemAddress());
		}
	}

	if (m_AcceptedA && m_AcceptedB) {
		auto* entityB = GetParticipantBEntity();

		if (entityB != nullptr) {
			GameMessages::SendServerTradeAccept(m_ParticipantB, false, entityB->GetSystemAddress());
		} else {
			return;
		}

		auto* entityA = GetParticipantAEntity();

		if (entityA != nullptr) {
			GameMessages::SendServerTradeAccept(m_ParticipantA, false, entityA->GetSystemAddress());
		} else {
			return;
		}

		Complete();
		TradingManager::Instance()->CancelTrade(m_TradeId);
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
		Game::logger->Log("TradingManager", "Possible coin trade cheating attempt! Aborting trade.");
		return;
	}

	for (const auto& tradeItem : m_ItemsA) {
		auto* itemToRemove = inventoryA->FindItemById(tradeItem.itemId);
		if (itemToRemove) {
			if (itemToRemove->GetCount() < tradeItem.itemCount) {
				Game::logger->Log("TradingManager", "Possible cheating attempt from %s in trading!!! Aborting trade", characterA->GetName().c_str());
				return;
			}
		} else {
			Game::logger->Log("TradingManager", "Possible cheating attempt from %s in trading due to item not being available!!!", characterA->GetName().c_str());
			return;
		}
	}

	for (const auto& tradeItem : m_ItemsB) {
		auto* itemToRemove = inventoryB->FindItemById(tradeItem.itemId);
		if (itemToRemove) {
			if (itemToRemove->GetCount() < tradeItem.itemCount) {
				Game::logger->Log("TradingManager", "Possible cheating attempt from %s in trading!!! Aborting trade", characterB->GetName().c_str());
				return;
			}
		} else {
			Game::logger->Log("TradingManager", "Possible cheating attempt from %s in trading due to item not being available!!!  Aborting trade", characterB->GetName().c_str());
			return;
		}
	}

	// Now actually do the trade.
	characterA->SetCoins(characterA->GetCoins() - m_CoinsA + m_CoinsB, eLootSourceType::TRADE);
	characterB->SetCoins(characterB->GetCoins() - m_CoinsB + m_CoinsA, eLootSourceType::TRADE);

	for (const auto& tradeItem : m_ItemsA) {
		auto* itemToRemove = inventoryA->FindItemById(tradeItem.itemId);
		if (itemToRemove) itemToRemove->SetCount(itemToRemove->GetCount() - tradeItem.itemCount);
		missionsA->Progress(eMissionTaskType::GATHER, tradeItem.itemLot, LWOOBJID_EMPTY, "", -tradeItem.itemCount);
		inventoryB->AddItem(tradeItem.itemLot, tradeItem.itemCount, eLootSourceType::TRADE);
	}

	for (const auto& tradeItem : m_ItemsB) {
		auto* itemToRemove = inventoryB->FindItemById(tradeItem.itemId);
		if (itemToRemove) itemToRemove->SetCount(itemToRemove->GetCount() - tradeItem.itemCount);
		missionsB->Progress(eMissionTaskType::GATHER, tradeItem.itemLot, LWOOBJID_EMPTY, "", -tradeItem.itemCount);
		inventoryA->AddItem(tradeItem.itemLot, tradeItem.itemCount, eLootSourceType::TRADE);
	}

	characterA->SaveXMLToDatabase();
	characterB->SaveXMLToDatabase();
	return;
}

void Trade::Cancel() {
	auto* entityA = GetParticipantAEntity();
	auto* entityB = GetParticipantBEntity();

	if (entityA == nullptr || entityB == nullptr) return;

	GameMessages::SendServerTradeCancel(entityA->GetObjectID(), entityA->GetSystemAddress());
	GameMessages::SendServerTradeCancel(entityB->GetObjectID(), entityB->GetSystemAddress());
}

void Trade::SendUpdateToOther(LWOOBJID participant) {
	Entity* other = nullptr;
	Entity* self = nullptr;
	uint64_t coins;
	std::vector<TradeItem> itemIds;

	Game::logger->Log("Trade", "Attempting to send trade update");

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

	Game::logger->Log("Trade", "Sending trade update");

	GameMessages::SendServerTradeUpdate(other->GetObjectID(), coins, items, other->GetSystemAddress());
}

TradingManager::TradingManager() {
}

TradingManager::~TradingManager() {
	for (const auto& pair : trades) {
		delete pair.second;
	}

	trades.clear();
}

Trade* TradingManager::GetTrade(LWOOBJID tradeId) const {
	const auto& pair = trades.find(tradeId);

	if (pair == trades.end()) return nullptr;

	return pair->second;
}

Trade* TradingManager::GetPlayerTrade(LWOOBJID playerId) const {
	for (const auto& pair : trades) {
		if (pair.second->IsParticipant(playerId)) {
			return pair.second;
		}
	}

	return nullptr;
}

void TradingManager::CancelTrade(LWOOBJID tradeId) {
	auto* trade = GetTrade(tradeId);

	if (trade == nullptr) return;

	delete trade;

	trades.erase(tradeId);
}

Trade* TradingManager::NewTrade(LWOOBJID participantA, LWOOBJID participantB) {
	const LWOOBJID tradeId = ObjectIDManager::Instance()->GenerateObjectID();

	auto* trade = new Trade(tradeId, participantA, participantB);

	trades[tradeId] = trade;

	Game::logger->Log("TradingManager", "Created new trade between (%llu) <-> (%llu)", participantA, participantB);

	return trade;
}
