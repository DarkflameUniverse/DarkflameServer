#include "ItemSetPassiveAbility.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "DestroyableComponent.h"
#include "SkillComponent.h"
#include "ItemSet.h"
#include "EntityManager.h"
#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDScriptComponentTable.h"
#include "CDSkillBehaviorTable.h"
#include "Behavior.h"
#include "eReplicaComponentType.h"

namespace {
	struct ScriptStatTrigger {
		std::string_view script;
		ItemSetStatTrigger trigger;
	};

	constexpr auto LowImagination = PassiveAbilityTrigger::AssemblyImagination;
	constexpr auto LowArmor = PassiveAbilityTrigger::SentinelArmor;

	// The CDClient links set items to these scripts through their ScriptComponent, but what each
	// script does only exists in the scripts themselves (res/scripts/equipmenttriggers/<name>.lua,
	// which all run skillSetTriggerTemplate.lua with these vars), so they are mirrored here like dScripts.
	// Paradox and Venture have scripts of the same kind (paradox1-3, venture*) but no item uses them.
	constexpr ScriptStatTrigger StatTriggerScripts[] = {
		// Low Imagination, IMAGINATION LESS 1
		{ "assembly1.lua", { LowImagination, 394, 4, 2, 0.0f } },
		{ "assembly2.lua", { LowImagination, 581, 4, 3, 0.0f } },
		{ "assembly3.lua", { LowImagination, 582, 4, 4, 0.0f } },
		{ "assemblyengineer1.lua", { LowImagination, 394, 4, 2, 11.0f } },
		{ "assemblyengineer2.lua", { LowImagination, 581, 4, 3, 11.0f } },
		{ "assemblyengineer3.lua", { LowImagination, 582, 4, 4, 11.0f } },
		{ "assemblyinventor1.lua", { LowImagination, 394, 4, 25, 11.0f } },
		{ "assemblyinventor2.lua", { LowImagination, 581, 4, 26, 11.0f } },
		{ "assemblyinventor3.lua", { LowImagination, 582, 4, 27, 11.0f } },
		{ "assemblysummoner1.lua", { LowImagination, 394, 4, 28, 11.0f } },
		{ "assemblysummoner2.lua", { LowImagination, 581, 4, 29, 11.0f } },
		{ "assemblysummoner3.lua", { LowImagination, 582, 4, 30, 11.0f } },
		// Low Armor, ARMOR LESS 1
		{ "sentinelknight1.lua", { LowArmor, 559, 4, 7, 0.0f } },
		{ "sentinelknight2.lua", { LowArmor, 560, 4, 8, 0.0f } },
		{ "sentinelknight3.lua", { LowArmor, 561, 4, 9, 0.0f } },
		{ "sentinelspaceranger1.lua", { LowArmor, 1101, 4, 10, 0.0f } },
		{ "sentinelspaceranger2.lua", { LowArmor, 1102, 4, 11, 0.0f } },
		{ "sentinelspaceranger3.lua", { LowArmor, 1103, 4, 12, 0.0f } },
		{ "sentinelsamurai1.lua", { LowArmor, 562, 4, 13, 0.0f } },
		{ "sentinelsamurai2.lua", { LowArmor, 563, 4, 14, 0.0f } },
		{ "sentinelsamurai3.lua", { LowArmor, 564, 4, 15, 0.0f } },
	};

	std::string ScriptFileName(std::string_view scriptName) {
		const auto slash = scriptName.find_last_of("/\\");
		if (slash != std::string_view::npos) scriptName.remove_prefix(slash + 1);

		std::string fileName{ scriptName };
		std::transform(fileName.begin(), fileName.end(), fileName.begin(), [](unsigned char c) { return std::tolower(c); });
		return fileName;
	}
}

ItemSetPassiveAbility::ItemSetPassiveAbility(PassiveAbilityTrigger trigger, Entity* parent, ItemSet* itemSet) {
	m_Trigger = trigger;
	m_Parent = parent;
	m_ItemSet = itemSet;

	m_Cooldown = 0.0f;
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
		OnEnemySmashed(target);

		return;
	}

	auto* destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();
	auto* skillComponent = m_Parent->GetComponent<SkillComponent>();

	if (destroyableComponent == nullptr || skillComponent == nullptr) {
		return;
	}

	Game::entityManager->SerializeEntity(m_Parent);

	if (m_ItemSet->GetEquippedCount() < m_StatTrigger.itemsRequired) return;

	m_Cooldown = m_StatTrigger.cooldown;
	skillComponent->CastSkill(m_StatTrigger.skillID, m_Parent->GetObjectID());
}

const ItemSetStatTrigger* ItemSetPassiveAbility::GetStatTriggerForScript(std::string_view scriptName) {
	const auto fileName = ScriptFileName(scriptName);
	for (const auto& entry : StatTriggerScripts) {
		if (entry.script == fileName) return &entry.trigger;
	}
	return nullptr;
}

std::vector<ItemSetStatTrigger> ItemSetPassiveAbility::FindStatTriggers(uint32_t itemSetID, const std::vector<LOT>& items) {
	std::vector<ItemSetStatTrigger> triggers;

	auto* registryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();
	auto* scriptTable = CDClientManager::GetTable<CDScriptComponentTable>();

	for (const auto lot : items) {
		const auto scriptComponentID = registryTable->GetByIDAndType(lot, eReplicaComponentType::SCRIPT, -1);
		if (scriptComponentID < 0) continue;

		const auto* trigger = GetStatTriggerForScript(scriptTable->GetByID(scriptComponentID).script_name);

		// Like the script, only fire for the set it was written for
		if (!trigger || trigger->setID != itemSetID) continue;

		// Every item of a set carries the same script, but only one of them casts the skill
		const auto alreadyFound = std::any_of(triggers.begin(), triggers.end(), [trigger](const ItemSetStatTrigger& found) {
			return found.trigger == trigger->trigger && found.skillID == trigger->skillID;
		});
		if (!alreadyFound) triggers.push_back(*trigger);
	}

	return triggers;
}

std::vector<ItemSetOnKillSkill> ItemSetPassiveAbility::FindOnKillSkills(const std::array<std::vector<uint32_t>, 5>& skillSets) {
	std::vector<ItemSetOnKillSkill> onKillSkills;

	auto* skillTable = CDClientManager::GetTable<CDSkillBehaviorTable>();

	for (size_t tier = 0; tier < skillSets.size(); ++tier) {
		for (const auto skillID : skillSets[tier]) {
			const auto behaviorID = skillTable->GetSkillByID(skillID).behaviorID;
			if (behaviorID == 0) continue;

			if (Behavior::GetBehaviorTemplate(behaviorID) != BehaviorTemplate::DARK_INSPIRATION) continue;

			// The client keys the status effect by behavior ID, so a behavior repeated in a higher tier
			// (e.g. the Sentinel rank 3 armor repair at 5 and 6 items) does not stack. Keep the lowest tier.
			const auto alreadyFound = std::any_of(onKillSkills.begin(), onKillSkills.end(), [behaviorID](const ItemSetOnKillSkill& found) {
				return found.behaviorID == behaviorID;
			});
			if (alreadyFound) continue;

			onKillSkills.push_back({ static_cast<uint32_t>(tier + 2), skillID, behaviorID });
		}
	}

	return onKillSkills;
}

std::vector<ItemSetPassiveAbility> ItemSetPassiveAbility::FindAbilities(uint32_t itemSetID, const std::vector<LOT>& items, const std::array<std::vector<uint32_t>, 5>& skillSets, Entity* parent, ItemSet* itemSet) {
	std::vector<ItemSetPassiveAbility> abilities;

	for (const auto& statTrigger : FindStatTriggers(itemSetID, items)) {
		auto& ability = abilities.emplace_back(statTrigger.trigger, parent, itemSet);
		ability.m_StatTrigger = statTrigger;
	}

	auto onKillSkills = FindOnKillSkills(skillSets);
	if (!onKillSkills.empty()) {
		auto& ability = abilities.emplace_back(PassiveAbilityTrigger::EnemySmashed, parent, itemSet);
		ability.m_OnKillSkills = std::move(onKillSkills);
	}

	return abilities;
}

void ItemSetPassiveAbility::OnEnemySmashed(Entity* target) {
	if (!target) return;

	auto* destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();
	auto* skillComponent = m_Parent->GetComponent<SkillComponent>();

	if (destroyableComponent == nullptr || skillComponent == nullptr) {
		return;
	}

	const auto equippedCount = m_ItemSet->GetEquippedCount();

	// The client adds a status effect for each DarkInspiration set skill when it is cast on equip,
	// which casts the behavior's action when the wearer kills something of a faction in its faction_list.
	// A tier's skills stay cast while at least that many items are equipped.
	for (const auto& onKillSkill : m_OnKillSkills) {
		if (equippedCount < onKillSkill.itemsRequired) continue;

		SkillComponent::CalculateUnmanaged(onKillSkill.behaviorID, m_Parent->GetObjectID(), target->GetObjectID());
	}

	Game::entityManager->SerializeEntity(m_Parent);
}
