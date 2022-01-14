#include "ItemSetPassiveAbility.h"

#include "DestroyableComponent.h"
#include "SkillComponent.h"
#include "ItemSet.h"
#include "ItemSetPassiveAbilityID.h"

ItemSetPassiveAbility::ItemSetPassiveAbility(PassiveAbilityTrigger trigger, Entity* parent, ItemSet* itemSet) 
{
    m_Trigger = trigger;
    m_Parent = parent;
    m_ItemSet = itemSet;

    m_Cooldown = 0.0f;
}

ItemSetPassiveAbility::~ItemSetPassiveAbility() 
{
}

void ItemSetPassiveAbility::Trigger(PassiveAbilityTrigger trigger) 
{
    if (m_Trigger != trigger || m_Cooldown > 0.0f)
    {
        return;
    }

    Activate();
}

void ItemSetPassiveAbility::Update(float deltaTime) 
{
    if (m_Cooldown > 0.0f)
    {
        m_Cooldown -= deltaTime;
    }
}

void ItemSetPassiveAbility::Activate() 
{
    if (m_Trigger == PassiveAbilityTrigger::EnemySmashed)
    {
        OnEnemySmshed();

        return;
    }

    auto* destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();
    auto* skillComponent = m_Parent->GetComponent<SkillComponent>();

    if (destroyableComponent == nullptr || skillComponent == nullptr)
    {
        return;
    }

    EntityManager::Instance()->SerializeEntity(m_Parent);

    const auto id = static_cast<ItemSetPassiveAbilityID>(m_ItemSet->GetID());
    const auto parentID = m_Parent->GetObjectID();
    const auto equippedCount = m_ItemSet->GetEquippedCount();

    switch (id)
    {
    // Assembly
    case ItemSetPassiveAbilityID::InventorRank1:
    case ItemSetPassiveAbilityID::SummonerRank1:
    case ItemSetPassiveAbilityID::EngineerRank1: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(394, 4401, parentID);
        break;
    }
    case ItemSetPassiveAbilityID::InventorRank2:
    case ItemSetPassiveAbilityID::SummonerRank2:
    case ItemSetPassiveAbilityID::EngineerRank2: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(581, 9433, parentID);
        break;
    }
    case ItemSetPassiveAbilityID::InventorRank3:
    case ItemSetPassiveAbilityID::SummonerRank3:
    case ItemSetPassiveAbilityID::EngineerRank3: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(582, 9435, parentID);
        break;
    }

    // Sentinel
    case ItemSetPassiveAbilityID::KnightRank1: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(559, 8884, parentID);
        break;
    }
    case ItemSetPassiveAbilityID::KnightRank2: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(560, 8885, parentID);
        break;
    }
    case ItemSetPassiveAbilityID::KnightRank3: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(561, 8890, parentID);
        break;
    }

    case ItemSetPassiveAbilityID::SpaceRangerRank1: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(1101, 24612, parentID);
        break;
    }
    case ItemSetPassiveAbilityID::SpaceRangerRank2: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(1102, 24617, parentID);
        break;
    }
    case ItemSetPassiveAbilityID::SpaceRangerRank3: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(1103, 24622, parentID);
        break;
    }

    case ItemSetPassiveAbilityID::SamuraiRank1: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(562, 8899, parentID);
        break;
    }
    case ItemSetPassiveAbilityID::SamuraiRank2: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(563, 8904, parentID);
        break;
    }
    case ItemSetPassiveAbilityID::SamuraiRank3: {
        if (equippedCount < 4) return;
        m_Cooldown = 11.0f;
        skillComponent->CalculateBehavior(564, 8909, parentID);
        break;
    }

    default:
        break;
    }
}

std::vector<ItemSetPassiveAbility> ItemSetPassiveAbility::FindAbilities(uint32_t itemSetID, Entity* parent, ItemSet* itemSet) 
{
    std::vector<ItemSetPassiveAbility> abilities;

    switch (static_cast<ItemSetPassiveAbilityID>(itemSetID)) {
    // Assembly
    case ItemSetPassiveAbilityID::SummonerRank1:
    case ItemSetPassiveAbilityID::SummonerRank2:
    case ItemSetPassiveAbilityID::SummonerRank3:
    case ItemSetPassiveAbilityID::InventorRank1:
    case ItemSetPassiveAbilityID::InventorRank2:
    case ItemSetPassiveAbilityID::InventorRank3:
    case ItemSetPassiveAbilityID::EngineerRank1:
    case ItemSetPassiveAbilityID::EngineerRank2:
    case ItemSetPassiveAbilityID::EngineerRank3: {
        abilities.emplace_back(PassiveAbilityTrigger::AssemblyImagination, parent, itemSet);

        break;
    }
    // Sentinel
    case ItemSetPassiveAbilityID::KnightRank1:
    case ItemSetPassiveAbilityID::KnightRank2:
    case ItemSetPassiveAbilityID::KnightRank3:
    case ItemSetPassiveAbilityID::SpaceRangerRank1:
    case ItemSetPassiveAbilityID::SpaceRangerRank2:
    case ItemSetPassiveAbilityID::SpaceRangerRank3:
    case ItemSetPassiveAbilityID::SamuraiRank1:
    case ItemSetPassiveAbilityID::SamuraiRank2:
    case ItemSetPassiveAbilityID::SamuraiRank3: {
        abilities.emplace_back(PassiveAbilityTrigger::SentinelArmor, parent, itemSet);
        abilities.emplace_back(PassiveAbilityTrigger::EnemySmashed, parent, itemSet);

        break;
    }
    // Paradox
    case ItemSetPassiveAbilityID::BatLord:
    case ItemSetPassiveAbilityID::SpaceMarauderRank1:
    case ItemSetPassiveAbilityID::SpaceMarauderRank2:
    case ItemSetPassiveAbilityID::SpaceMarauderRank3:
    case ItemSetPassiveAbilityID::SorcererRank1:
    case ItemSetPassiveAbilityID::SorcererRank2:
    case ItemSetPassiveAbilityID::SorcererRank3:
    case ItemSetPassiveAbilityID::ShinobiRank1:
    case ItemSetPassiveAbilityID::ShinobiRank2:
    case ItemSetPassiveAbilityID::ShinobiRank3: {
        abilities.emplace_back(PassiveAbilityTrigger::EnemySmashed, parent, itemSet);

        break;
    }
    default:
        break;
    }
    
    return abilities;
}

void ItemSetPassiveAbility::OnEnemySmshed() 
{
    auto* destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();
    auto* skillComponent = m_Parent->GetComponent<SkillComponent>();

    if (destroyableComponent == nullptr || skillComponent == nullptr)
    {
        return;
    }

    EntityManager::Instance()->SerializeEntity(m_Parent);

    const auto id = static_cast<ItemSetPassiveAbilityID>(m_ItemSet->GetID());
    const auto parentID = m_Parent->GetObjectID();
    const auto equippedCount = m_ItemSet->GetEquippedCount();

    switch (id)
    {
    // Bat Lord
    case ItemSetPassiveAbilityID::BatLord: {
        if(equippedCount < 5) return;
        destroyableComponent->Heal(3);
        break;
    }
    // Sentinel
    case ItemSetPassiveAbilityID::KnightRank1: {
        if (equippedCount < 5) return;
        destroyableComponent->Repair(1);
        break;
    }
    case ItemSetPassiveAbilityID::KnightRank2: {
        if (equippedCount < 5) return;
        destroyableComponent->Repair(1);
        break;
    }
    case ItemSetPassiveAbilityID::KnightRank3: {
        if (equippedCount < 5) return;
        destroyableComponent->Repair(1);
        break;
    }

    case ItemSetPassiveAbilityID::SpaceRangerRank1: {
        if (equippedCount < 5) return;
        destroyableComponent->Repair(1);
        break;
    }
    case ItemSetPassiveAbilityID::SpaceRangerRank2: {
        if (equippedCount < 5) return;
        destroyableComponent->Repair(1);
        break;
    }
    case ItemSetPassiveAbilityID::SpaceRangerRank3: {
        if (equippedCount < 5) return;
        destroyableComponent->Repair(1);
        break;
    }

    case ItemSetPassiveAbilityID::SamuraiRank1: {
        if (equippedCount < 5) return;
        destroyableComponent->Repair(1);
        break;
    }
    case ItemSetPassiveAbilityID::SamuraiRank2: {
        if (equippedCount < 5) return;
        destroyableComponent->Repair(1);
        break;
    }
    case ItemSetPassiveAbilityID::SamuraiRank3: {
        if (equippedCount < 5) return;
        destroyableComponent->Repair(1);
        break;
    }

    // Paradox
    case ItemSetPassiveAbilityID::SpaceMarauderRank1: {
        if (equippedCount < 4) return;
        destroyableComponent->Imagine(1);
        break;
    }
    case ItemSetPassiveAbilityID::SpaceMarauderRank2: {
        if (equippedCount < 4) return;
        destroyableComponent->Imagine(2);
        break;
    }
    case ItemSetPassiveAbilityID::SpaceMarauderRank3: {
        if (equippedCount < 4) return;
        destroyableComponent->Imagine(3);
        break;
    }
    
    case ItemSetPassiveAbilityID::ShinobiRank1: {
        if (equippedCount < 4) return;
        destroyableComponent->Imagine(1);
        break;
    }
    case ItemSetPassiveAbilityID::ShinobiRank2: {
        if (equippedCount < 4) return;
        destroyableComponent->Imagine(2);
        break;
    }
    case ItemSetPassiveAbilityID::ShinobiRank3: {
        if (equippedCount < 4) return;
        destroyableComponent->Imagine(3);
        break;
    }

    case ItemSetPassiveAbilityID::SorcererRank1: {
        if (equippedCount < 4) return;
        destroyableComponent->Imagine(1);
        break;
    }
    case ItemSetPassiveAbilityID::SorcererRank2: {
        if (equippedCount < 4) return;
        destroyableComponent->Imagine(2);
        break;
    }
    case ItemSetPassiveAbilityID::SorcererRank3: {
        if (equippedCount < 4) return;
        destroyableComponent->Imagine(3);
        break;
    }

    default:
        break;
    }
}
