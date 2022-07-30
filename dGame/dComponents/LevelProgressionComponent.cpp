#include "LevelProgressionComponent.h"
#include "ControllablePhysicsComponent.h"
#include "InventoryComponent.h"
#include "CharacterComponent.h"
#include "tinyxml2.h"

LevelProgressionComponent::LevelProgressionComponent(Entity* parent) : Component(parent) {
	m_Parent = parent;
	m_Level = 1;
}

void LevelProgressionComponent::UpdateXml(tinyxml2::XMLDocument* doc) {
	tinyxml2::XMLElement* level = doc->FirstChildElement("obj")->FirstChildElement("lvl");
	if (!level) {
		Game::logger->Log("LevelProgressionComponent", "Failed to find lvl tag while updating XML!");
		return;
	}
	level->SetAttribute("l", m_Level);

}

void LevelProgressionComponent::LoadFromXml(tinyxml2::XMLDocument* doc) {
	tinyxml2::XMLElement* level = doc->FirstChildElement("obj")->FirstChildElement("lvl");
	if (!level) {
		Game::logger->Log("LevelProgressionComponent", "Failed to find lvl tag while loading XML!");
		return;
	}
	level->QueryAttribute("l", &m_Level);

}

void LevelProgressionComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(bIsInitialUpdate || m_DirtyLevelInfo);
	if (bIsInitialUpdate || m_DirtyLevelInfo) outBitStream->Write(m_Level);
	m_DirtyLevelInfo = false;
}

void LevelProgressionComponent::HandleLevelUp() {
	auto* rewardsTable = CDClientManager::Instance()->GetTable<CDRewardsTable>("Rewards");

	const auto& rewards = rewardsTable->GetByLevelID(m_Level);
	bool rewardingItem = rewards.size() > 0;

	auto* inventoryComponent = m_Parent->GetComponent<InventoryComponent>();
	auto* controllablePhysicsComponent = m_Parent->GetComponent<ControllablePhysicsComponent>();

	if (!inventoryComponent || !controllablePhysicsComponent) return;
	// Tell the client we beginning to send level rewards.
	if (rewardingItem) GameMessages::NotifyLevelRewards(m_Parent->GetObjectID(), m_Parent->GetSystemAddress(), m_Level, rewardingItem);

	for (auto* reward : rewards) {
		switch (reward->rewardType) {
		case 0:
			inventoryComponent->AddItem(reward->value, reward->count, eLootSourceType::LOOT_SOURCE_LEVEL_REWARD);
			break;
		case 4:
		{
			auto* items = inventoryComponent->GetInventory(eInventoryType::ITEMS);
			items->SetSize(items->GetSize() + reward->value);
		}
		break;
		case 9:
			controllablePhysicsComponent->SetSpeedMultiplier(static_cast<float>(reward->value) / 500.0f);
			break;
		case 11:
		case 12:
			break;
		default:
			break;
		}
	}
	// Tell the client we have finished sending level rewards.
	if (rewardingItem) GameMessages::NotifyLevelRewards(m_Parent->GetObjectID(), m_Parent->GetSystemAddress(), m_Level, !rewardingItem);
}
