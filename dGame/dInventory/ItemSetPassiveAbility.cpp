#include "ItemSetPassiveAbility.h"

#include "DestroyableComponent.h"
#include "SkillComponent.h"
#include "ItemSet.h"
#include "eItemSetPassiveAbilityID.h"

ItemSetPassiveAbility::ItemSetPassiveAbility(PassiveAbilityTrigger trigger, Entity* parent, ItemSet* itemSet) {
	m_Trigger = trigger;
	m_Parent = parent;
	m_ItemSet = itemSet;

	m_Cooldown = 0.0f;
}

ItemSetPassiveAbility::~ItemSetPassiveAbility() {
}

void ItemSetPassiveAbility::Trigger(PassiveAbilityTrigger trigger, Entity* target) {
	if (m_Trigger != trigger || m_Cooldown > 0.0f) {
		return;
	}

	Activate(target);
}

void ItemSetPassiveAbility::Update(float deltaTime) {
	if (m_Cooldown > 0.0f) {
		m_Cooldown -= deltaTime;
	}
}

void ItemSetPassiveAbility::Activate(Entity* target) {
	if (m_Trigger == PassiveAbilityTrigger::EnemySmashed) {
		OnEnemySmshed(target);

		return;
	}

	auto* destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();
	auto* skillComponent = m_Parent->GetComponent<SkillComponent>();

	if (destroyableComponent == nullptr || skillComponent == nullptr) {
		return;
	}

	Game::entityManager->SerializeEntity(m_Parent);

	const auto id = static_cast<eItemSetPassiveAbilityID>(m_ItemSet->GetID());
	const auto parentID = m_Parent->GetObjectID();
	const auto equippedCount = m_ItemSet->GetEquippedCount();

	switch (id) {
		// Assembly
	case eItemSetPassiveAbilityID::InventorRank1:
	case eItemSetPassiveAbilityID::SummonerRank1:
	case eItemSetPassiveAbilityID::EngineerRank1: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(394, 4401, parentID);
		break;
	}
	case eItemSetPassiveAbilityID::InventorRank2:
	case eItemSetPassiveAbilityID::SummonerRank2:
	case eItemSetPassiveAbilityID::EngineerRank2: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(581, 9433, parentID);
		break;
	}
	case eItemSetPassiveAbilityID::InventorRank3:
	case eItemSetPassiveAbilityID::SummonerRank3:
	case eItemSetPassiveAbilityID::EngineerRank3: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(582, 9435, parentID);
		break;
	}

											   // Sentinel
	case eItemSetPassiveAbilityID::KnightRank1: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(559, 8884, parentID);
		break;
	}
	case eItemSetPassiveAbilityID::KnightRank2: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(560, 8885, parentID);
		break;
	}
	case eItemSetPassiveAbilityID::KnightRank3: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(561, 8890, parentID);
		break;
	}

	case eItemSetPassiveAbilityID::SpaceRangerRank1: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(1101, 24612, parentID);
		break;
	}
	case eItemSetPassiveAbilityID::SpaceRangerRank2: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(1102, 24617, parentID);
		break;
	}
	case eItemSetPassiveAbilityID::SpaceRangerRank3: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(1103, 24622, parentID);
		break;
	}

	case eItemSetPassiveAbilityID::SamuraiRank1: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(562, 8899, parentID);
		break;
	}
	case eItemSetPassiveAbilityID::SamuraiRank2: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(563, 8904, parentID);
		break;
	}
	case eItemSetPassiveAbilityID::SamuraiRank3: {
		if (equippedCount < 4) return;
		m_Cooldown = 11.0f;
		skillComponent->CalculateBehavior(564, 8909, parentID);
		break;
	}

	default:
		break;
	}
}

std::vector<ItemSetPassiveAbility> ItemSetPassiveAbility::FindAbilities(uint32_t itemSetID, Entity* parent, ItemSet* itemSet) {
	std::vector<ItemSetPassiveAbility> abilities;

	switch (static_cast<eItemSetPassiveAbilityID>(itemSetID)) {
		// Assembly
	case eItemSetPassiveAbilityID::SummonerRank1:
	case eItemSetPassiveAbilityID::SummonerRank2:
	case eItemSetPassiveAbilityID::SummonerRank3:
	case eItemSetPassiveAbilityID::InventorRank1:
	case eItemSetPassiveAbilityID::InventorRank2:
	case eItemSetPassiveAbilityID::InventorRank3:
	case eItemSetPassiveAbilityID::EngineerRank1:
	case eItemSetPassiveAbilityID::EngineerRank2:
	case eItemSetPassiveAbilityID::EngineerRank3: {
		abilities.emplace_back(PassiveAbilityTrigger::AssemblyImagination, parent, itemSet);

		break;
	}
											   // Sentinel
	case eItemSetPassiveAbilityID::KnightRank1:
	case eItemSetPassiveAbilityID::KnightRank2:
	case eItemSetPassiveAbilityID::KnightRank3:
	case eItemSetPassiveAbilityID::SpaceRangerRank1:
	case eItemSetPassiveAbilityID::SpaceRangerRank2:
	case eItemSetPassiveAbilityID::SpaceRangerRank3:
	case eItemSetPassiveAbilityID::SamuraiRank1:
	case eItemSetPassiveAbilityID::SamuraiRank2:
	case eItemSetPassiveAbilityID::SamuraiRank3: {
		abilities.emplace_back(PassiveAbilityTrigger::SentinelArmor, parent, itemSet);
		abilities.emplace_back(PassiveAbilityTrigger::EnemySmashed, parent, itemSet);

		break;
	}
											  // Paradox
	case eItemSetPassiveAbilityID::BatLord:
	case eItemSetPassiveAbilityID::SpaceMarauderRank1:
	case eItemSetPassiveAbilityID::SpaceMarauderRank2:
	case eItemSetPassiveAbilityID::SpaceMarauderRank3:
	case eItemSetPassiveAbilityID::SorcererRank1:
	case eItemSetPassiveAbilityID::SorcererRank2:
	case eItemSetPassiveAbilityID::SorcererRank3:
	case eItemSetPassiveAbilityID::ShinobiRank1:
	case eItemSetPassiveAbilityID::ShinobiRank2:
	case eItemSetPassiveAbilityID::ShinobiRank3: {
		abilities.emplace_back(PassiveAbilityTrigger::EnemySmashed, parent, itemSet);

		break;
	}
	default:
		break;
	}

	return abilities;
}

void ItemSetPassiveAbility::OnEnemySmshed(Entity* target) {
	auto* destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();
	auto* skillComponent = m_Parent->GetComponent<SkillComponent>();

	if (destroyableComponent == nullptr || skillComponent == nullptr) {
		return;
	}

	Game::entityManager->SerializeEntity(m_Parent);

	const auto id = static_cast<eItemSetPassiveAbilityID>(m_ItemSet->GetID());
	const auto parentID = m_Parent->GetObjectID();
	const auto equippedCount = m_ItemSet->GetEquippedCount();

	switch (id) {
		// Bat Lord
	case eItemSetPassiveAbilityID::BatLord: {
		if (equippedCount < 5) return;
		destroyableComponent->Heal(3);
		break;
	}
										 // Sentinel
	case eItemSetPassiveAbilityID::KnightRank1: {
		if (equippedCount < 5) return;
		destroyableComponent->Repair(1);
		break;
	}
	case eItemSetPassiveAbilityID::KnightRank2: {
		if (equippedCount < 5) return;
		destroyableComponent->Repair(1);
		break;
	}
	case eItemSetPassiveAbilityID::KnightRank3: {
		if (equippedCount < 5) return;
		destroyableComponent->Repair(1);
		break;
	}

	case eItemSetPassiveAbilityID::SpaceRangerRank1: {
		if (equippedCount < 5) return;
		destroyableComponent->Repair(1);
		break;
	}
	case eItemSetPassiveAbilityID::SpaceRangerRank2: {
		if (equippedCount < 5) return;
		destroyableComponent->Repair(1);
		break;
	}
	case eItemSetPassiveAbilityID::SpaceRangerRank3: {
		if (equippedCount < 5) return;
		destroyableComponent->Repair(1);
		break;
	}

	case eItemSetPassiveAbilityID::SamuraiRank1: {
		if (equippedCount < 5) return;
		destroyableComponent->Repair(1);
		break;
	}
	case eItemSetPassiveAbilityID::SamuraiRank2: {
		if (equippedCount < 5) return;
		destroyableComponent->Repair(1);
		break;
	}
	case eItemSetPassiveAbilityID::SamuraiRank3: {
		if (equippedCount < 5) return;
		destroyableComponent->Repair(1);
		break;
	}

											  // Paradox
	case eItemSetPassiveAbilityID::SpaceMarauderRank1: {
		if (equippedCount < 4) return;
		destroyableComponent->Imagine(1);
		break;
	}
	case eItemSetPassiveAbilityID::SpaceMarauderRank2: {
		if (equippedCount < 4) return;
		destroyableComponent->Imagine(2);
		break;
	}
	case eItemSetPassiveAbilityID::SpaceMarauderRank3: {
		if (equippedCount < 4) return;
		destroyableComponent->Imagine(3);
		break;
	}

	case eItemSetPassiveAbilityID::ShinobiRank1: {
		if (equippedCount < 4) return;
		destroyableComponent->Imagine(1);
		break;
	}
	case eItemSetPassiveAbilityID::ShinobiRank2: {
		if (equippedCount < 4) return;
		destroyableComponent->Imagine(2);
		break;
	}
	case eItemSetPassiveAbilityID::ShinobiRank3: {
		if (equippedCount < 4) return;
		destroyableComponent->Imagine(3);
		break;
	}

	case eItemSetPassiveAbilityID::SorcererRank1: {
		if (equippedCount < 4) return;
		destroyableComponent->Imagine(1);
		break;
	}
	case eItemSetPassiveAbilityID::SorcererRank2: {
		if (equippedCount < 4) return;
		destroyableComponent->Imagine(2);
		break;
	}
	case eItemSetPassiveAbilityID::SorcererRank3: {
		if (equippedCount < 4) return;
		destroyableComponent->Imagine(3);
		break;
	}

	default:
		break;
	}
}
