#include "AdditionalEntityData.h"

#include "NejlikaData.h"

#include <DestroyableComponent.h>
#include <LevelProgressionComponent.h>
#include <InventoryComponent.h>
#include <BaseCombatAIComponent.h>
#include <TeamManager.h>
#include <ControllablePhysicsComponent.h>
#include <Item.h>

#include <queue>

using namespace nejlika;

float nejlika::AdditionalEntityData::CalculateModifier(ModifierType type, ModifierOperator op, bool resistance) const
{
	float total = 0;

	for (const auto& modifier : activeModifiers) {
		if (modifier.GetType() != type || modifier.GetOperator() != op || modifier.IsResistance() != resistance) {
			continue;
		}

		total += modifier.GetValue();
	}

	return total;
}

float nejlika::AdditionalEntityData::CalculateModifier(ModifierType type, std::vector<ModifierInstance>& additionalModifiers, ModifierOperator op, bool resistance) const {
	float total = 0;

	for (const auto& modifier : additionalModifiers) {
		if (modifier.GetType() != type || modifier.GetOperator() != op || modifier.IsResistance() != resistance) {
			continue;
		}

		total += modifier.GetValue();
	}

	return total + CalculateModifier(type, op, resistance);
}

float nejlika::AdditionalEntityData::CalculateModifier(ModifierType type, int32_t level) const
{
	const auto templateDataOpt = NejlikaData::GetEntityTemplate(lot);

	if (!templateDataOpt.has_value()) {
		return 0;
	}

	const auto& templateData = *templateDataOpt.value();

	const auto scaler = templateData.GetScaler(type, false, level);

	float additive = CalculateModifier(type, ModifierOperator::Additive, false);

	if (scaler != 0 && additive >= scaler) {
		additive -= scaler;
	}

	float multiplicative = CalculateModifier(type, ModifierOperator::Multiplicative, false);

	std::cout << "Scaler: " << scaler << " Additive: " << additive << " Multiplicative: " << multiplicative << std::endl;

	return (scaler + additive) * (1 + multiplicative / 100);
}

float nejlika::AdditionalEntityData::CalculateModifier(ModifierType type) const {
	return CalculateModifier(type, level);
}

float nejlika::AdditionalEntityData::CalculateModifier(ModifierType type, std::vector<ModifierInstance>& additionalModifiers, int32_t level) const
{
	const auto templateDataOpt = NejlikaData::GetEntityTemplate(lot);

	if (!templateDataOpt.has_value()) {
		return 0;
	}

	const auto& templateData = *templateDataOpt.value();

	const auto scaler = templateData.GetScaler(type, false, level);

	float additive = CalculateModifier(type, additionalModifiers, ModifierOperator::Additive, false);
	
	if (scaler != 0 && additive >= scaler) {
		additive -= scaler;
	}
	
	float multiplicative = CalculateModifier(type, additionalModifiers, ModifierOperator::Multiplicative, false);

	static const std::unordered_set<ModifierType> damageTypes = {
		ModifierType::Slashing,
		ModifierType::Piercing,
		ModifierType::Bludgeoning,
		ModifierType::Fire,
		ModifierType::Cold,
		ModifierType::Lightning,
		ModifierType::Corruption,
		ModifierType::Psychic
	};

	if (damageTypes.contains(type)) {
		additive += CalculateModifier(ModifierType::Damage, additionalModifiers, ModifierOperator::Additive, false);
		multiplicative += CalculateModifier(ModifierType::Damage, additionalModifiers, ModifierOperator::Multiplicative, false);
	}

	float total = (scaler + additive) * (1 + multiplicative / 100);

	std::cout << "Scaler: " << scaler << " Additive: " << additive << " Multiplicative: " << multiplicative << " Total: " << total << std::endl;

	return total;
}

float nejlika::AdditionalEntityData::CalculateResistance(ModifierType type) const
{
	return CalculateModifier(type, ModifierOperator::Multiplicative, true);
}

float nejlika::AdditionalEntityData::CalculateMultiplier(ModifierType type) const
{
	return 1 + CalculateModifier(type, ModifierOperator::Multiplicative, false);
}

std::vector<ModifierInstance> nejlika::AdditionalEntityData::TriggerUpgradeItems(UpgradeTriggerType triggerType) {
	auto* entity = Game::entityManager->GetEntity(id);

	if (entity == nullptr) {
		return {};
	}

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return {};
	}

	std::vector<ModifierInstance> result;

	for (const auto& itemID : upgradeItems) {
		auto* item = inventoryComponent->FindItemById(itemID);

		if (item == nullptr) {
			continue;
		}

		const auto upgradeDataOpt = NejlikaData::GetUpgradeTemplate(item->GetLot());

		if (!upgradeDataOpt.has_value()) {
			continue;
		}

		const auto& upgradeData = *upgradeDataOpt.value();

		const auto modifiers = upgradeData.Trigger(item->GetCount(), triggerType, id);

		result.insert(result.end(), modifiers.begin(), modifiers.end());
	}

	return result;
}

void nejlika::AdditionalEntityData::InitializeSkills() {
	auto* entity = Game::entityManager->GetEntity(id);

	if (entity == nullptr) {
		return;
	}

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}
	
	struct entry {
		LWOOBJID id;
		int32_t priority;

		entry(LWOOBJID id, int32_t priority) : id(id), priority(priority) {}
	};

	std::vector<entry> items;

	for (const auto& itemID : upgradeItems) {
		auto* item = inventoryComponent->FindItemById(itemID);

		if (item == nullptr) {
			continue;
		}
		
		const auto priority = item->GetSlot();

		items.push_back(entry(itemID, priority));
	}

	std::sort(items.begin(), items.end(), [](const entry& a, const entry& b) {
		return a.priority < b.priority;
	});

	for (const auto& item : items) {
		AddSkills(item.id);
	}
}

void nejlika::AdditionalEntityData::AddSkills(LWOOBJID item) {
	if (!upgradeItems.contains(item)) {
		return;
	}

	auto* entity = Game::entityManager->GetEntity(id);

	if (entity == nullptr) {
		return;
	}

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	auto* itemData = inventoryComponent->FindItemById(item);

	if (itemData == nullptr) {
		return;
	}

	const auto upgradeDataOpt = NejlikaData::GetUpgradeTemplate(itemData->GetLot());

	if (!upgradeDataOpt.has_value()) {
		return;
	}

	const auto& upgradeData = *upgradeDataOpt.value();

	LOG("Adding skills for item %i", id);

	upgradeData.AddSkills(id);
}

void nejlika::AdditionalEntityData::RemoveSkills(LOT lot) {
	const auto upgradeDataOpt = NejlikaData::GetUpgradeTemplate(lot);

	if (!upgradeDataOpt.has_value()) {
		return;
	}

	const auto& upgradeData = *upgradeDataOpt.value();

	LOG("Removing skills for item %i", id);

	upgradeData.RemoveSkills(id);
}

void nejlika::AdditionalEntityData::RollStandardModifiers(int32_t level) {
	standardModifiers.clear();

	const auto templateDataOpt = NejlikaData::GetEntityTemplate(lot);

	if (templateDataOpt.has_value()) {
		const auto& templateData = *templateDataOpt.value();

		const auto modifiers = templateData.GenerateModifiers(level);

		standardModifiers.insert(standardModifiers.end(), modifiers.begin(), modifiers.end());
	}

	const auto objectDataVec = NejlikaData::GetModifierNameTemplates(ModifierNameType::Object);

	for (const auto& objectData : objectDataVec) {
		if (objectData.GetLOT() != lot) {
			continue;
		}

		const auto modifiers = objectData.GenerateModifiers(level);

		standardModifiers.insert(standardModifiers.end(), modifiers.begin(), modifiers.end());
	}
}

void nejlika::AdditionalEntityData::ApplyToEntity() {
	const auto templateDataOpt = NejlikaData::GetEntityTemplate(lot);

	if (!templateDataOpt.has_value()) {
		return;
	}

	const auto& templateData = *templateDataOpt.value();

	auto* entity = Game::entityManager->GetEntity(id);

	if (entity == nullptr) {
		return;
	}

	auto* destroyable = entity->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		return;
	}

	auto* levelProgression = entity->GetComponent<LevelProgressionComponent>();

	if (levelProgression != nullptr) {
		this->level = levelProgression->GetLevel();
	}
	else {
		this->level = templateData.GetMinLevel();
	}
	
	if (!initialized) {
		RollStandardModifiers(level);
	}

	activeModifiers = standardModifiers;

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

	if (inventoryComponent) {
		for (const auto& [location, item] : inventoryComponent->GetEquippedItems()) {
			const auto itemDataOpt = NejlikaData::GetAdditionalItemData(item.id);

			if (!itemDataOpt.has_value()) {
				continue;
			}

			const auto& itemData = *itemDataOpt.value();

			const auto& itemModifiers = itemData.GetModifierInstances();

			activeModifiers.insert(activeModifiers.end(), itemModifiers.begin(), itemModifiers.end());
		}
	}

	destroyable->SetMaxHealth(static_cast<int32_t>(CalculateModifier(ModifierType::Health, level)));
	destroyable->SetMaxArmor(static_cast<int32_t>(CalculateModifier(ModifierType::Armor, level)));
	if (!entity->IsPlayer()) {
		destroyable->SetMaxImagination(static_cast<int32_t>(CalculateModifier(ModifierType::Imagination, level)));
	}

	if (initialized) {
		return;
	}

	destroyable->SetHealth(destroyable->GetMaxHealth());
	destroyable->SetArmor(destroyable->GetMaxArmor());

	if (!entity->IsPlayer()) {
		destroyable->SetImagination(destroyable->GetMaxImagination());
	}

	if (entity->IsPlayer()) {
		auto* controllablePhysicsComponent = entity->GetComponent<ControllablePhysicsComponent>();
		if (controllablePhysicsComponent) controllablePhysicsComponent->SetSpeedMultiplier(CalculateMultiplier(ModifierType::Speed));
	}

	initialized = true;
}

void nejlika::AdditionalEntityData::CheckForRescale(AdditionalEntityData* other) {
	auto* entity = Game::entityManager->GetEntity(id);

	if (entity == nullptr) {
		return;
	}

	if (entity->IsPlayer()) {
		return;
	}

	auto* baseCombat = entity->GetComponent<BaseCombatAIComponent>();

	if (baseCombat == nullptr) {
		return;
	}

	const auto& threats = baseCombat->GetThreats();

	int32_t totalThreats = 0;
	int32_t totalLevel = 0;

	for (const auto& [threat, _] : threats) {
		const auto threatEntityOpt = NejlikaData::GetAdditionalEntityData(threat);

		if (!threatEntityOpt.has_value()) {
			continue;
		}

		const auto& threatEntity = *threatEntityOpt.value();

		if (other->id == threatEntity.id) {
			continue;
		}

		totalLevel += threatEntity.level;
		totalThreats++;
	}

	if (other != nullptr) {
		totalLevel += other->level;
		totalThreats++;

		auto* team = TeamManager::Instance()->GetTeam(other->id);

		if (team != nullptr) {
			for (const auto& member : team->members) {
				const auto memberEntityOpt = NejlikaData::GetAdditionalEntityData(member);

				if (!memberEntityOpt.has_value()) {
					continue;
				}

				const auto& memberEntity = *memberEntityOpt.value();

				if (other->id == memberEntity.id) {
					continue;
				}

				totalLevel += memberEntity.level;
				totalThreats++;
			}
		}
	}

	if (totalThreats == 0) {
		return;
	}

	const auto averageLevel = totalLevel / totalThreats;

	// Can't rescale to a lower level
	if (averageLevel <= level) {
		return;
	}

	level = averageLevel;

	auto* destroyable = entity->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		return;
	}

	float healthPercentage = destroyable->GetMaxHealth() == 0 ? 1 : static_cast<float>(destroyable->GetHealth()) / destroyable->GetMaxHealth();
	float armorPercentage = destroyable->GetMaxArmor() == 0 ? 1 : static_cast<float>(destroyable->GetArmor()) / destroyable->GetMaxArmor();
	float imaginationPercentage = destroyable->GetMaxImagination() == 0 ? 1 : static_cast<float>(destroyable->GetImagination()) / destroyable->GetMaxImagination();

	RollStandardModifiers(level);

	ApplyToEntity();

	destroyable->SetHealth(static_cast<int32_t>(destroyable->GetMaxHealth() * healthPercentage));
	destroyable->SetArmor(static_cast<int32_t>(destroyable->GetMaxArmor() * armorPercentage));
	destroyable->SetImagination(static_cast<int32_t>(destroyable->GetMaxImagination() * imaginationPercentage));

	LOG("Rescaled entity %i to level %d", entity->GetLOT(), level);
}
