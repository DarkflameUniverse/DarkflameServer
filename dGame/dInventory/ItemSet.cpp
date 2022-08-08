#include "ItemSet.h"

#include "InventoryComponent.h"
#include "Entity.h"
#include "SkillComponent.h"
#include "CDClientDatabase.h"
#include "Game.h"
#include "MissionComponent.h"
#include <algorithm>

ItemSet::ItemSet(const uint32_t id, InventoryComponent* inventoryComponent) {
	this->m_ID = id;
	this->m_InventoryComponent = inventoryComponent;

	this->m_PassiveAbilities = ItemSetPassiveAbility::FindAbilities(id, m_InventoryComponent->GetParent(), this);

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT skillSetWith2, skillSetWith3, skillSetWith4, skillSetWith5, skillSetWith6, itemIDs FROM ItemSets WHERE setID = ?;");
	query.bind(1, (int)id);

	auto result = query.execQuery();

	if (result.eof()) {
		return;
	}

	for (auto i = 0; i < 5; ++i) {
		if (result.fieldIsNull(i)) {
			continue;
		}

		auto skillQuery = CDClientDatabase::CreatePreppedStmt(
			"SELECT SkillID FROM ItemSetSkills WHERE SkillSetID = ?;");
		skillQuery.bind(1, result.getIntField(i));

		auto skillResult = skillQuery.execQuery();

		if (skillResult.eof()) {
			return;
		}

		while (!skillResult.eof()) {
			if (skillResult.fieldIsNull(0)) {
				skillResult.nextRow();

				continue;
			}

			const auto skillId = skillResult.getIntField(0);

			switch (i) {
			case 0:
				m_SkillsWith2.push_back(skillId);
				break;
			case 1:
				m_SkillsWith3.push_back(skillId);
				break;
			case 2:
				m_SkillsWith4.push_back(skillId);
				break;
			case 3:
				m_SkillsWith5.push_back(skillId);
				break;
			case 4:
				m_SkillsWith6.push_back(skillId);
				break;
			default:
				break;
			}

			skillResult.nextRow();
		}
	}

	std::string ids = result.getStringField(5);

	ids.erase(std::remove_if(ids.begin(), ids.end(), ::isspace), ids.end());

	std::istringstream stream(ids);
	std::string token;

	result.finalize();

	m_Items = {};

	while (std::getline(stream, token, ',')) {
		int32_t value;
		if (GeneralUtils::TryParse(token, value)) {
			m_Items.push_back(value);
		}
	}

	m_Equipped = {};

	for (const auto item : m_Items) {
		if (inventoryComponent->IsEquipped(item)) {
			m_Equipped.push_back(item);
		}
	}
}

bool ItemSet::Contains(const LOT lot) {
	return std::find(m_Items.begin(), m_Items.end(), lot) != m_Items.end();
}

void ItemSet::OnEquip(const LOT lot) {
	if (!Contains(lot)) {
		return;
	}

	const auto& index = std::find(m_Equipped.begin(), m_Equipped.end(), lot);

	if (index != m_Equipped.end()) {
		return;
	}

	m_Equipped.push_back(lot);

	const auto& skillSet = GetSkillSet(m_Equipped.size());

	if (skillSet.empty()) {
		return;
	}

	auto* skillComponent = m_InventoryComponent->GetParent()->GetComponent<SkillComponent>();
	auto* missionComponent = m_InventoryComponent->GetParent()->GetComponent<MissionComponent>();

	for (const auto skill : skillSet) {
		auto* skillTable = CDClientManager::Instance()->GetTable<CDSkillBehaviorTable>("SkillBehavior");

		const auto behaviorId = skillTable->GetSkillByID(skill).behaviorID;

		missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_SKILL, skill);

		skillComponent->HandleUnmanaged(behaviorId, m_InventoryComponent->GetParent()->GetObjectID());
	}
}

void ItemSet::OnUnEquip(const LOT lot) {
	if (!Contains(lot)) {
		return;
	}

	const auto& index = std::find(m_Equipped.begin(), m_Equipped.end(), lot);

	if (index == m_Equipped.end()) {
		return;
	}

	const auto& skillSet = GetSkillSet(m_Equipped.size());

	m_Equipped.erase(index);

	if (skillSet.empty()) {
		return;
	}

	const auto& skillComponent = m_InventoryComponent->GetParent()->GetComponent<SkillComponent>();

	for (const auto skill : skillSet) {
		auto* skillTable = CDClientManager::Instance()->GetTable<CDSkillBehaviorTable>("SkillBehavior");

		const auto behaviorId = skillTable->GetSkillByID(skill).behaviorID;

		skillComponent->HandleUnCast(behaviorId, m_InventoryComponent->GetParent()->GetObjectID());
	}
}

uint32_t ItemSet::GetEquippedCount() const {
	return m_Equipped.size();
}

uint32_t ItemSet::GetID() const {
	return m_ID;
}

void ItemSet::Update(float deltaTime) {
	for (auto& passiveAbility : m_PassiveAbilities) {
		passiveAbility.Update(deltaTime);
	}
}

void ItemSet::TriggerPassiveAbility(PassiveAbilityTrigger trigger) {
	for (auto& passiveAbility : m_PassiveAbilities) {
		passiveAbility.Trigger(trigger);
	}
}

std::vector<uint32_t> ItemSet::GetSkillSet(const uint32_t itemCount) const {
	switch (itemCount) {
	case 2:
		return m_SkillsWith2;
	case 3:
		return m_SkillsWith3;
	case 4:
		return m_SkillsWith4;
	case 5:
		return m_SkillsWith5;
	case 6:
		return m_SkillsWith6;
	default:
		return {};
	}
}
