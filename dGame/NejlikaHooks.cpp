#include "NejlikaHooks.h"

#include "SlashCommandHandler.h"

#include <InventoryComponent.h>
#include <Item.h>
#include <ChatPackets.h>
#include <Amf3.h>
#include <iomanip>
#include <dConfig.h>
#include <magic_enum.hpp>
#include <GeneralUtils.h>
#include <LevelProgressionComponent.h>
#include <DestroyableComponent.h>
#include <unordered_set>
#include <BaseCombatAIComponent.h>
#include <PlayerManager.h>
#include <eGameMessageType.h>
#include <dServer.h>
#include <Item.h>
#include <SkillComponent.h>
#include <MissionComponent.h>
#include <eMissionState.h>

#include "NejlikaHelpers.h"
#include "NejlikaData.h"

using namespace nejlika;
using namespace nejlika::NejlikaData;

namespace {
	std::unordered_map<LWOOBJID, std::unordered_map<uint32_t, std::vector<ModifierInstance>>> uniqueSkillModifiers;
}

void nejlika::NejlikaHooks::InstallHooks() {
	Command itemDescriptionCommand{
		.help = "Special UI command, does nothing when used in chat.",
		.info = "Special UI command, does nothing when used in chat.",
		.aliases = {"d"},
		.handle = ItemDescription,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	SlashCommandHandler::RegisterCommand(itemDescriptionCommand);

	Command modificationssCommand{
		.help = "Displays active modifications.",
		.info = "Displays active modifications.",
		.aliases = {"stats"},
		.handle = [](Entity* entity, const SystemAddress& sysAddr, const std::string args) {
			auto entityDataOpt = GetAdditionalEntityData(entity->GetObjectID());

			if (!entityDataOpt.has_value()) {
				return;
			}

			auto& entityData = *entityDataOpt.value();

			std::vector<ModifierInstance> modifiers = entityData.GetActiveModifiers();

			std::stringstream ss;

			ss << "Active modifications: " << std::endl;

			ss << ModifierInstance::GenerateHtmlString(modifiers);

			ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::ASCIIToUTF16(ss.str()));
		},
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	SlashCommandHandler::RegisterCommand(modificationssCommand);

	LoadNejlikaData();

	InventoryComponent::OnItemCreated += [](InventoryComponent* component, Item* item) {
		const auto& itemType = static_cast<eItemType>(item->GetInfo().itemType);

		// No to the Thinking Hat
		if (item->GetLot() == 6086) {
			return;
		}

		auto* levelProgressionComponent = component->GetParent()->GetComponent<LevelProgressionComponent>();

		if (!levelProgressionComponent) {
			return;
		}

		auto additionalData = AdditionalItemData(item);

		LOG("Rolling modifiers for item: %d", item->GetLot());

		additionalData.RollModifiers(item, levelProgressionComponent->GetLevel());

		SetAdditionalItemData(item->GetId(), additionalData);

		auto entityDataOpt = GetAdditionalEntityData(component->GetParent()->GetObjectID());

		if (!entityDataOpt.has_value()) {
			return;
		}

		auto& entityData = *entityDataOpt.value();

		auto upgradeTemplateOpt = GetUpgradeTemplate(item->GetLot());

		if (!upgradeTemplateOpt.has_value()) {
			return;
		}

		auto& upgradeTemplate = *upgradeTemplateOpt.value();

		entityData.AddUpgradeItem(item->GetId());
		entityData.AddSkills(item->GetId());

		entityData.ApplyToEntity();
	};

	InventoryComponent::OnCountChanged += [](InventoryComponent* component, Item* item) {
		auto entityDataOpt = GetAdditionalEntityData(component->GetParent()->GetObjectID());

		if (!entityDataOpt.has_value()) {
			return;
		}

		auto& entityData = *entityDataOpt.value();

		auto upgradeTemplateOpt = GetUpgradeTemplate(item->GetLot());

		if (!upgradeTemplateOpt.has_value()) {
			return;
		}

		entityData.ApplyToEntity();
	};

	EntityManager::OnEntityCreated += [](Entity* entity) {
		auto* destroyable = entity->GetComponent<DestroyableComponent>();

		if (!destroyable) {
			return;
		}

		SetAdditionalEntityData(entity->GetObjectID(), AdditionalEntityData(entity->GetObjectID(), entity->GetLOT()));

		auto additionalDataOpt = GetAdditionalEntityData(entity->GetObjectID());

		if (!additionalDataOpt.has_value()) {
			return;
		}

		auto& additionalData = *additionalDataOpt.value();

		auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

		if (!inventoryComponent) {
			additionalData.ApplyToEntity();
			return;
		}

		// Loop through all items and check if they are upgrade items
		const auto& inventories = inventoryComponent->GetInventories();

		for (const auto& [type, inventory] : inventories) {
			for (const auto& [id, item] : inventory->GetItems()) {
				const auto upgradeTemplateOpt = GetUpgradeTemplate(item->GetLot());

				if (!upgradeTemplateOpt.has_value()) {
					continue;
				}

				const auto& upgradeTemplate = *upgradeTemplateOpt.value();

				additionalData.AddUpgradeItem(id);
			}
		}

		additionalData.ApplyToEntity();

		additionalData.InitializeSkills();
	};

	Entity::OnReadyForUpdates += [](Entity* entity) {
		if (!entity->IsPlayer()) {
			return;
		}

		GameMessages::SendAddSkill(entity, NejlikaData::GetLookup().GetValue("intro:skills:proxy:tertiary"), BehaviorSlot::Neck);

		auto* missionComponent = entity->GetComponent<MissionComponent>();

		if (missionComponent) {
			if (missionComponent->GetMissionState(1732) != eMissionState::COMPLETE) {
				missionComponent->CompleteMission(1732, true, false);
			}

			if (missionComponent->GetMissionState(173) != eMissionState::COMPLETE) {
				missionComponent->CompleteMission(173, true, false);

				auto* destroyable = entity->GetComponent<DestroyableComponent>();

				destroyable->SetMaxImagination(6);
				destroyable->SetImagination(6);
			}
		}

		auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

		if (!inventoryComponent) {
			return;
		}

		inventoryComponent->UpdateSkills();
	};

	EntityManager::OnEntityDestroyed += [](Entity* entity) {
		UnsetAdditionalEntityData(entity->GetObjectID());
	};

	InventoryComponent::OnItemLoaded += [](InventoryComponent* component, Item* item) {
		SetAdditionalItemData(item->GetId(), AdditionalItemData(item));
	};

	InventoryComponent::OnItemDestroyed += [](InventoryComponent* component, Item* item) {
		auto entityDataOpt = GetAdditionalEntityData(component->GetParent()->GetObjectID());

		if (!entityDataOpt.has_value()) {
			UnsetAdditionalItemData(item->GetId());
			return;
		}

		auto& entityData = *entityDataOpt.value();

		entityData.RemoveUpgradeItem(item->GetId());
		entityData.RemoveSkills(item->GetLot());
		UnsetAdditionalItemData(item->GetId());
	};

	LevelProgressionComponent::OnLevelUp += [](LevelProgressionComponent* component) {
		auto* parent = component->GetParent();

		auto entityDataOpt = GetAdditionalEntityData(parent->GetObjectID());

		if (!entityDataOpt.has_value()) {
			return;
		}

		auto& entityData = *entityDataOpt.value();

		entityData.ApplyToEntity();

		auto* inventoryComponent = parent->GetComponent<InventoryComponent>();

		if (!inventoryComponent) {
			return;
		}

		inventoryComponent->AddItem(NejlikaData::GetLookup().GetValue("intro:upgrades:level-token"), 3, eLootSourceType::MODERATION);
	};

	InventoryComponent::OnItemEquipped += [](InventoryComponent* component, Item* item) {
		std::cout << "Item equipped: " << item->GetId() << std::endl;

		const auto entityDataOpt = GetAdditionalEntityData(component->GetParent()->GetObjectID());

		if (!entityDataOpt.has_value()) {
			std::cout << "No entity data found for entity." << std::endl;
			return;
		}

		auto& entityData = *entityDataOpt.value();

		entityData.TriggerUpgradeItems(UpgradeTriggerType::Equip);

		entityData.ApplyToEntity();

		const auto itemDataOpt = GetAdditionalItemData(item->GetId());

		if (!itemDataOpt.has_value()) {
			std::cout << "No item data found for item." << std::endl;
			return;
		}

		const auto& itemData = *itemDataOpt.value();

		const auto itemId = item->GetId();

		std::cout << "Sending effects for item: " << itemId << " with " << itemData.GetModifierInstances().size() << " modifiers." << std::endl;

		for (const auto& modifier : itemData.GetModifierInstances()) {
			const auto effectID = modifier.GetEffectID();
			const auto effectType = modifier.GetEffectType();

			component->GetParent()->AddCallbackTimer(0.1f, [itemId, effectID, effectType]() {
				std::cout << "Sending effect: " << effectID << " - " << effectType << std::endl;
				GameMessages::SendPlayFXEffect(
					itemId,
					static_cast<int32_t>(effectID),
					GeneralUtils::UTF8ToUTF16(effectType),
					std::to_string(GeneralUtils::GenerateRandomNumber<uint32_t>())
				);
				});
		}
		};

	InventoryComponent::OnItemUnequipped += [](InventoryComponent* component, Item* item) {
		const auto entityDataOpt = GetAdditionalEntityData(component->GetParent()->GetObjectID());

		if (!entityDataOpt.has_value()) {
			return;
		}

		auto& entityData = *entityDataOpt.value();

		entityData.TriggerUpgradeItems(UpgradeTriggerType::UnEquip);

		entityData.ApplyToEntity();
		};

	SkillComponent::OnSkillCast += [](SkillComponent* skillComponent, uint32_t skillID, bool& success, uint32_t skillUID) {
		std::cout << "Skill cast: " << skillID << " - " << success << std::endl;

		auto* inventoryComponent = skillComponent->GetParent()->GetComponent<InventoryComponent>();

		if (!inventoryComponent) {
			return;
		}

		auto* entity = skillComponent->GetParent();

		auto skills = inventoryComponent->GetSkills();
		const auto tertiaryTrigger = NejlikaData::GetLookup().GetValue("intro:skills:proxy:tertiary");

		if (skillID == tertiaryTrigger) {
			inventoryComponent->RotateSkills();
		}

		const auto entityDataOpt = GetAdditionalEntityData(entity->GetObjectID());

		if (!entityDataOpt.has_value()) {
			return;
		}

		auto& entityData = *entityDataOpt.value();

		const auto& skillTemplates = GetModifierNameTemplates(ModifierNameType::Skill);

		const auto& skillTemplateIt = std::find_if(skillTemplates.begin(), skillTemplates.end(), [skillID](const auto& it) {
			return it.GetLOT() == skillID;
			});

		std::vector<ModifierInstance> modifiers;

		if (skillTemplateIt != skillTemplates.end()) {
			const auto& skillTemplate = *skillTemplateIt;

			const auto skillModifiers = skillTemplate.GenerateModifiers(entityData.GetLevel());

			modifiers.insert(modifiers.end(), skillModifiers.begin(), skillModifiers.end());
		}

		LOT itemLot = 0;
		LWOOBJID itemId = 0;
		BehaviorSlot itemSlot = BehaviorSlot::Invalid;

		if (inventoryComponent) {
			const auto& equipped = inventoryComponent->GetEquippedItems();

			// omg...
			for (const auto& [equippedSlot, itemDetails] : equipped) {
				std::cout << "Found equipped item: " << itemDetails.lot << std::endl;

				const auto info = Inventory::FindItemComponent(itemDetails.lot);

				const auto skill = InventoryComponent::FindSkill(itemDetails.lot);

				if (skill != skillID) {
					continue;
				}

				const auto itemBehaviorSlot = InventoryComponent::FindBehaviorSlot(static_cast<eItemType>(info.itemType));

				itemLot = itemDetails.lot;
				itemId = itemDetails.id;
				itemSlot = itemBehaviorSlot;

				std::cout << "Found item: " << itemLot << std::endl;

				break;
			}
		}

		TriggerParameters params;
		params.SkillID = skillID;
		params.SelectedBehaviorSlot = itemSlot;

		// Upgrades
		const auto upgradeModifiers = entityData.TriggerUpgradeItems(UpgradeTriggerType::OnCast, params);

		auto unionModifiersUpgradeModifiers = modifiers;
		unionModifiersUpgradeModifiers.insert(unionModifiersUpgradeModifiers.end(), upgradeModifiers.begin(), upgradeModifiers.end());

		const auto& imaginationCost = entityData.CalculateFinalModifier(ModifierType::ImaginationCost, unionModifiersUpgradeModifiers, entityData.GetLevel());

		auto* destroyable = entity->GetComponent<DestroyableComponent>();

		if (destroyable != nullptr) {
			if (destroyable->GetImagination() < imaginationCost) {
				NejlikaHelpers::RenderDamageText("Insufficient imagination!", entity, entity, 4.0f);
			}
			else {
				destroyable->SetImagination(destroyable->GetImagination() - imaginationCost);

				Game::entityManager->SerializeEntity(entity);

				// Insert into the unique skill modifiers
				const auto& uniqueSkillModifiersIt = uniqueSkillModifiers.find(entity->GetObjectID());

				if (uniqueSkillModifiersIt != uniqueSkillModifiers.end()) {
					auto& uniqueSkillModifiersMap = uniqueSkillModifiersIt->second;

					uniqueSkillModifiersMap[skillID] = upgradeModifiers;
				}
				else {
					uniqueSkillModifiers[entity->GetObjectID()] = { { skillID, upgradeModifiers } };
				}

				entity->AddCallbackTimer(10.0f, [entity, skillID]() {
					const auto& uniqueSkillModifiersIt = uniqueSkillModifiers.find(entity->GetObjectID());

					if (uniqueSkillModifiersIt != uniqueSkillModifiers.end()) {
						auto& uniqueSkillModifiersMap = uniqueSkillModifiersIt->second;

						const auto& uniqueSkillModifiersSkillIt = uniqueSkillModifiersMap.find(skillID);

						if (uniqueSkillModifiersSkillIt != uniqueSkillModifiersMap.end()) {
							uniqueSkillModifiersMap.erase(uniqueSkillModifiersSkillIt);
						}
					}
				});

				modifiers.insert(modifiers.end(), upgradeModifiers.begin(), upgradeModifiers.end());
			}
		}

		float attackSpeed = 0;

		if (itemSlot == BehaviorSlot::Primary) {
			auto attacksPerSecond = entityData.CalculateFinalModifier(ModifierType::AttacksPerSecond, modifiers, entityData.GetLevel());
			attacksPerSecond = std::max(attacksPerSecond, 1.0f);
			const auto attackSpeedMod = entityData.CalculateMultiplier(ModifierType::AttackSpeed, modifiers);
			attackSpeed = (1.0f / attacksPerSecond) * ((attackSpeedMod / 100.0f)) / 2.0f; // 2.0f to account for animation times
			LOG("Attack speed: %f, attacks per second: %f, attack speed mod: %f", attackSpeed, attacksPerSecond, attackSpeedMod);
		}
		else {
			const auto skillRecharge = entityData.CalculateFinalModifier(ModifierType::SkillRecharge, modifiers, entityData.GetLevel());
			const auto skillCooldownMod = entityData.CalculateMultiplier(ModifierType::SkillCooldownReduction, modifiers);
			attackSpeed = skillRecharge * (skillCooldownMod / 100.0f);
			LOG("Skill recharge: %f, skill cooldown mod: %f", skillRecharge, skillCooldownMod);
		}

		if (entity->IsPlayer()) {
			entity->AddCallbackTimer(0.0f, [entity, skillID, attackSpeed]() {
				CBITSTREAM;
				CMSGHEADER;

				const auto objectID = entity->GetObjectID();

				bitStream.Write(objectID);
				bitStream.Write(eGameMessageType::MODIFY_SKILL_COOLDOWN);

				bitStream.Write1();
				bitStream.Write<float>(attackSpeed);
				bitStream.Write<int32_t>(static_cast<int32_t>(skillID));

				LOG("Sending cooldown reduction for skill: %d: %f", skillID, attackSpeed);

				SEND_PACKET_BROADCAST;
			});
		}
	};

	DestroyableComponent::OnDamageCalculation += [](Entity* damaged, LWOOBJID offender, uint32_t skillID, uint32_t& damage) {
		std::cout << "Calculating damage with skill: " << skillID << std::endl;

		const auto damagedEntityOpt = GetAdditionalEntityData(damaged->GetObjectID());

		if (!damagedEntityOpt.has_value()) {
			std::cout << "No entity data found for damaged entity." << std::endl;
			return;
		}

		auto& damagedEntity = *damagedEntityOpt.value();

		const auto offenderEntityOpt = GetAdditionalEntityData(offender);

		if (!offenderEntityOpt.has_value()) {
			std::cout << "No entity data found for offender entity." << std::endl;
			return;
		}

		auto& offenderEntity = *offenderEntityOpt.value();

		auto* baseCombatAIComponent = damaged->GetComponent<BaseCombatAIComponent>();

		if (baseCombatAIComponent) {
			baseCombatAIComponent->SetThreat(offender, 1);
		}

		damagedEntity.CheckForRescale(&offenderEntity);
		offenderEntity.CheckForRescale(&damagedEntity);

		int32_t level = offenderEntity.GetLevel();

		auto* offfendEntity = Game::entityManager->GetEntity(offender);

		if (offfendEntity == nullptr) {
			std::cout << "Offender entity not found." << std::endl;
			return;
		}

		auto* levelProgressionComponent = offfendEntity->GetComponent<LevelProgressionComponent>();

		if (levelProgressionComponent) {
			level = levelProgressionComponent->GetLevel();
		}

		LOT itemLot = 0;
		LWOOBJID itemId = 0;
		BehaviorSlot itemSlot = BehaviorSlot::Invalid;

		auto* inventoryComponent = offfendEntity->GetComponent<InventoryComponent>();

		if (inventoryComponent) {
			const auto& equipped = inventoryComponent->GetEquippedItems();

			// omg...
			for (const auto& [equippedSlot, itemDetails] : equipped) {
				std::cout << "Found equipped item: " << itemDetails.lot << std::endl;

				const auto info = Inventory::FindItemComponent(itemDetails.lot);

				const auto skill = InventoryComponent::FindSkill(itemDetails.lot);

				if (skill != skillID) {
					continue;
				}

				const auto itemBehaviorSlot = InventoryComponent::FindBehaviorSlot(static_cast<eItemType>(info.itemType));

				itemLot = itemDetails.lot;
				itemId = itemDetails.id;
				itemSlot = itemBehaviorSlot;

				std::cout << "Found item: " << itemLot << std::endl;

				break;
			}
		}

		std::vector<ModifierInstance> modifiers;

		if (itemSlot == BehaviorSlot::Primary) {
			const auto mainWeaponDamage = offenderEntity.CalculateMainWeaponDamage();

			for (const auto& modifier : mainWeaponDamage) {
				std::cout << "Main weapon damage: " << magic_enum::enum_name(modifier.GetType()) << " - " << modifier.GetValue() << std::endl;
			}

			modifiers.insert(modifiers.end(), mainWeaponDamage.begin(), mainWeaponDamage.end());
		}

		const auto& skillTemplates = GetModifierNameTemplates(ModifierNameType::Skill);

		const auto& skillTemplateIt = std::find_if(skillTemplates.begin(), skillTemplates.end(), [skillID](const auto& it) {
			return it.GetLOT() == skillID;
			});

		if (skillTemplateIt != skillTemplates.end()) {
			const auto& skillTemplate = *skillTemplateIt;

			const auto skillModifiers = skillTemplate.GenerateModifiers(level);

			modifiers.insert(modifiers.end(), skillModifiers.begin(), skillModifiers.end());
		}

		TriggerParameters params;
		params.SkillID = skillID;
		params.SelectedBehaviorSlot = itemSlot;

		// Upgrades
		const auto upgradeModifiers = offenderEntity.TriggerUpgradeItems(UpgradeTriggerType::OnHit, params);

		modifiers.insert(modifiers.end(), upgradeModifiers.begin(), upgradeModifiers.end());

		const auto& uniqueSkillModifiersIt = uniqueSkillModifiers.find(offender);

		if (uniqueSkillModifiersIt != uniqueSkillModifiers.end()) {
			const auto& uniqueSkillModifiersMap = uniqueSkillModifiersIt->second;

			const auto& uniqueSkillModifiersSkillIt = uniqueSkillModifiersMap.find(skillID);

			if (uniqueSkillModifiersSkillIt != uniqueSkillModifiersMap.end()) {
				const auto& uniqueSkillModifiersVec = uniqueSkillModifiersSkillIt->second;

				modifiers.insert(modifiers.end(), uniqueSkillModifiersVec.begin(), uniqueSkillModifiersVec.end());
			}
		}

		std::unordered_set<ModifierType> damageTypes;

		for (const auto& modifier : modifiers) {
			damageTypes.insert(modifier.GetType());
		}

		const auto itemDataOpt = GetAdditionalItemData(itemId);

		if (itemDataOpt.has_value()) {
			const auto& itemData = *itemDataOpt.value();

			for (const auto& modifier : itemData.GetModifierInstances()) {
				damageTypes.insert(modifier.GetType());
			}
		}

		int32_t totalDamage = 0;

		std::unordered_map<ModifierType, std::pair<float, float>> durationTypes;
		std::unordered_map<ModifierType, float> tmpDamageValues;
		
		const auto mainWeaponDamageModifier = (offenderEntity.CalculateMultiplier(ModifierType::MainWeaponDamage, modifiers) - 100.0f) / 100.0f;

		if (mainWeaponDamageModifier > 0.0f) {
			const auto mainWeaponDamage = offenderEntity.CalculateMainWeaponDamage();

			for (auto modifier : mainWeaponDamage) {
				const auto damageValue = modifier.GetValue();

				modifier.SetValue(damageValue * mainWeaponDamageModifier);

				modifiers.push_back(modifier);	
			}
		}
		
		for (const auto& type : damageTypes) {
			if (nejlika::IsOverTimeType(type)) {
				float damageValue = offenderEntity.CalculateFinalModifier(type, modifiers, level);

				// Calculate resistance, can't go below 20% of the original damage
				const auto resistance = std::max(1 - (damagedEntity.CalculateResistance(type) / 100), 0.2f);

				float reductedDamage = damageValue * resistance;

				const auto durationType = nejlika::GetDurationType(type);

				const auto duration = offenderEntity.CalculateFinalModifier(durationType, modifiers, level);

				const auto durationResistance = std::max(1 - (damagedEntity.CalculateResistance(durationType) / 100), 0.2f);

				float reductedDuration = duration * durationResistance;

				durationTypes[type] = std::make_pair(reductedDamage, reductedDuration);

				continue;
			}

			if (!nejlika::IsNormalDamageType(type)) {
				continue;
			}

			float damageValue = offenderEntity.CalculateFinalModifier(type, modifiers, level);

			tmpDamageValues[type] = damageValue;
		}

		// Type A -> Type B -> (0-100) how much of type A is converted to type B
		const auto converationMap = offenderEntity.CalculateDamageConversion(modifiers);

		std::unordered_map<ModifierType, float> finalDamageValues;

		/*for (const auto& [typeA, typeBMap] : converationMap) {
			const auto& typeAValue = tmpDamageValues.find(typeA);

			if (typeAValue == tmpDamageValues.end()) {
				continue;
			}

			const auto& typeAValueFloat = typeAValue->second;

			for (const auto& [typeB, conversion] : typeBMap) {
				const auto& typeBValue = tmpDamageValues.find(typeB);

				if (typeBValue == tmpDamageValues.end()) {
					continue;
				}

				const auto& typeBValueFloat = typeBValue->second;

				const auto convertedValue = typeAValueFloat * conversion;

				finalDamageValues[typeA] += typeAValueFloat - convertedValue;
				finalDamageValues[typeB] += typeBValueFloat + convertedValue;
			}
		}*/

		// Add the remaining values
		for (const auto& [type, value] : tmpDamageValues) {
			if (finalDamageValues.find(type) == finalDamageValues.end()) {
				finalDamageValues[type] = value;
			}
		}

		for (const auto& [type, damage] : finalDamageValues) {
			// Calculate resistance, can't go below 20% of the original damage
			const auto resistance = std::max(1 - (damagedEntity.CalculateResistance(type) / 100), 0.2f);

			float reductedDamage = damage * resistance;

			totalDamage += static_cast<int32_t>(reductedDamage);

			std::cout << "Damage type: " << magic_enum::enum_name(type) << " - " << damage << std::endl;
			std::cout << "Resistance: " << resistance << " - " << reductedDamage << std::endl;
			std::cout << "Heath left: " << damaged->GetComponent<DestroyableComponent>()->GetHealth() << std::endl;
		}

		// Get the offenders Offensive modifier
		auto offenderModifiers = offenderEntity.CalculateModifier(ModifierType::Offensive, level);

		// Get the defenders Defensive modifier
		auto defensiveModifiers = damagedEntity.CalculateModifier(ModifierType::Defensive, level);

		if (offenderModifiers == 0) offenderModifiers = 1;
		if (defensiveModifiers == 0) defensiveModifiers = 1;

		// https://www.grimdawn.com/guide/gameplay/combat/#q20
		auto pth = ((
			((offenderModifiers / ((defensiveModifiers / 3.5) + offenderModifiers)) * 300) * 0.3
		) + (
			((((offenderModifiers * 3.25) + 10000) - (defensiveModifiers * 3.25)) / 100) * 0.7)
		) - 50;

		if (pth < 60) pth = 60;

		float roll = GeneralUtils::GenerateRandomNumber<size_t>(0, std::max(static_cast<int32_t>(pth), 100));

		bool isCritical = false;
		bool isHit = false;
		float damageMultiplier = 1.0f;

		if (roll > pth) {
			// Miss
			isHit = false;
		} else {
			// Hit
			isHit = true;

			if (pth >= 135) {
				if (roll <= 134) damageMultiplier = 1.5f;
				else if (roll <= 129) damageMultiplier = 1.4f;
				else if (roll <= 124) damageMultiplier = 1.3f;
				else if (roll <= 119) damageMultiplier = 1.2f;
				else if (roll <= 104) damageMultiplier = 1.1f;
			} else if (pth >= 130) {
				if (roll <= 129) damageMultiplier = 1.4f;
				else if (roll <= 124) damageMultiplier = 1.3f;
				else if (roll <= 119) damageMultiplier = 1.2f;
				else if (roll <= 104) damageMultiplier = 1.1f;
			} else if (pth >= 120) {
				if (roll <= 119) damageMultiplier = 1.3f;
				else if (roll <= 104) damageMultiplier = 1.2f;
				else if (roll <= 89) damageMultiplier = 1.1f;
			} else if (pth >= 105) {
				if (roll <= 104) damageMultiplier = 1.2f;
				else if (roll <= 89) damageMultiplier = 1.1f;
			} else if (pth >= 90) {
				if (roll <= 89) damageMultiplier = 1.1f;
			} else if (pth < 70) {
				damageMultiplier = pth / 70.0f;
			}

			if (damageMultiplier > 1.0f) {
				isCritical = true;

				damageMultiplier *= (offenderEntity.CalculateMultiplier(ModifierType::CriticalDamage, modifiers) / 100.0f);
			}
		}

		if (isHit) {
			// Add a random +5% to the damage
			totalDamage += static_cast<int32_t>(totalDamage * (GeneralUtils::GenerateRandomNumber<int32_t>(0, 5) / 100.0f));

			damage = totalDamage;
		} else {
			damage = totalDamage = 0;
		}

		if (totalDamage < 0 || damage < 0) {
			totalDamage = damage = 0;
		}

		if (isCritical) {
			totalDamage = static_cast<int32_t>(totalDamage * damageMultiplier);

			const auto effectName = std::to_string(GeneralUtils::GenerateRandomNumber<uint32_t>());
			const auto damagedID = damaged->GetObjectID();

			GameMessages::SendPlayFXEffect(
				damagedID,
				1531,
				u"create",
				effectName
			);

			damaged->AddCallbackTimer(0.5f, [damaged, effectName]() {
				GameMessages::SendStopFXEffect(
					damaged,
					true,
					effectName
				);
			});
		}

		if (damage == 0) {
			return;
		}

		const auto stunDuration = offenderEntity.CalculateFinalModifier(ModifierType::Stun, modifiers, level);

		if (baseCombatAIComponent && stunDuration > 0) {
			LOG("Stunning for %f seconds", stunDuration);
			baseCombatAIComponent->Stun(stunDuration, true);
		}

		// Apply over time damage.
		// Times are rounded to the nearest 0.5s
		for (const auto& [type, damageDuration] : durationTypes) {
			if (damageDuration.first == 0) {
				continue;
			}

			const auto duration = static_cast<int32_t>(damageDuration.second * 2);

			if (duration == 0) {
				continue;
			}

			const auto damagePerTick = static_cast<int32_t>(damageDuration.first);

			auto* destroyable = damaged->GetComponent<DestroyableComponent>();

			if (!destroyable) {
				continue;
			}
			
			for (size_t i = 0; i < duration; i++)
			{
				damaged->AddCallbackTimer(i * 0.5f, [type, offender, damaged, damagePerTick]() {

					auto* offenderEntity = Game::entityManager->GetEntity(offender);

					auto* destroyable = damaged->GetComponent<DestroyableComponent>();

					if (!destroyable) {
						return;
					}

					destroyable->Damage(damagePerTick, offender, 0, true, true);
				});
			}
		}
	};
}


void nejlika::NejlikaHooks::ItemDescription(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	auto splitArgs = GeneralUtils::SplitString(args, ' ');
	if (splitArgs.size() < 3) {
		ChatPackets::SendSystemMessage(sysAddr, u"Invalid arguments.");
		return;
	}

	auto requestId = GeneralUtils::TryParse<int32_t>(splitArgs[0]).value_or(-1);

	if (requestId == -1) {
		ChatPackets::SendSystemMessage(sysAddr, u"Invalid item ID.");
		return;
	}

	std::cout << "Request ID: " << requestId << std::endl;

	auto itemId = GeneralUtils::TryParse<LWOOBJID>(splitArgs[1]).value_or(LWOOBJID_EMPTY);

	auto lot = GeneralUtils::TryParse<LOT>(splitArgs[2]).value_or(0);

	if (lot == 0) {
		ChatPackets::SendSystemMessage(sysAddr, u"Invalid item LOT.");
		return;
	}

	std::cout << "Item ID: " << itemId << std::endl;
	std::cout << "Item LOT: " << lot << std::endl;

	std::stringstream name;
	std::stringstream desc;

	const auto& upgradeItemOpt = NejlikaData::GetUpgradeTemplate(lot);

	if (upgradeItemOpt.has_value()) {
		const auto& upgradeItem = *upgradeItemOpt.value();

		auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

		if (!inventoryComponent) {
			return;
		}

		auto amount = inventoryComponent->GetLotCount(lot);

		if (itemId == LWOOBJID_EMPTY) {
			amount++;
		}

		const auto& modifiers = upgradeItem.GenerateModifiers(amount);

		name << "<font color=\"#D0AB62\">NAME</font>";

		desc << "DESC" << "\n\n";

		desc << "<font color=\"#D0AB62\">Level " << amount << "/" << upgradeItem.GetMaxLevel() << "</font>\n";

		desc << ModifierInstance::GenerateHtmlString(modifiers);

		const auto& passives = upgradeItem.GetPassives();

		desc << UpgradeEffect::GenerateHtmlString(passives, amount);
	} else if (itemId == LWOOBJID_EMPTY) {
		const auto& itemTemplateVec = NejlikaData::GetModifierNameTemplates(ModifierNameType::Object);
		
		auto* levelProgressionComponent = entity->GetComponent<LevelProgressionComponent>();

		if (!levelProgressionComponent) {
			return;
		}

		std::vector<const nejlika::ModifierNameTemplate*> availableTemplates;
		const nejlika::ModifierNameTemplate* lowestLevelTemplate = nullptr;
		std::vector<const nejlika::ModifierNameTemplate*> lowestLevelTemplates;

		for (const auto& itemTemplate : itemTemplateVec) {
			auto level = static_cast<int32_t>(levelProgressionComponent->GetLevel());

			if (itemTemplate.GetLOT() != static_cast<int32_t>(lot)) {
				continue;
			}

			if (lowestLevelTemplate == nullptr) {
				lowestLevelTemplate = &itemTemplate;
			}
			else if (lowestLevelTemplate->GetMinLevel() > level) {
				lowestLevelTemplate = &itemTemplate;
				lowestLevelTemplates.clear();
			}
			else if (lowestLevelTemplate->GetMinLevel() == level) {
				lowestLevelTemplates.push_back(&itemTemplate);
			}
			
			if (itemTemplate.GetMinLevel() > level || itemTemplate.GetMaxLevel() < level) {
				continue;
			}

			availableTemplates.push_back(&itemTemplate);
		}

		// Add the lowest level template if no other template was found
		if (availableTemplates.empty() && lowestLevelTemplate != nullptr) {
			availableTemplates.push_back(lowestLevelTemplate);

			// Add all templates with the same level
			availableTemplates.insert(availableTemplates.end(), lowestLevelTemplates.begin(), lowestLevelTemplates.end());
		}
		
		name << "<font color=\"#D0AB62\">NAME</font>";

		if (availableTemplates.empty()) {
			desc << "DESC";
		} else {
			if (availableTemplates.size() > 1) {
				desc << "<font color=\"#D0AB62\">One of:</font>\n";
			}
			for (size_t i = 0; i < availableTemplates.size(); i++) {
				const auto& itemTemplate = *availableTemplates[i];

				const auto& modifiers = itemTemplate.GetModifiers();

				// Get the entity level
				auto* levelProgressionComponent = entity->GetComponent<LevelProgressionComponent>();

				if (!levelProgressionComponent) {
					return;
				}

				auto level = std::max(static_cast<int32_t>(levelProgressionComponent->GetLevel()), itemTemplate.GetMinLevel());

				desc << ModifierTemplate::GenerateHtmlString(modifiers, level);

				if (i < availableTemplates.size() - 1) {
					desc << "\n<font color=\"#D0AB62\">or</font>\n";
				}
			}
		}
	} else {
		const auto itemDataOpt = GetAdditionalItemData(itemId);

		if (!itemDataOpt.has_value()) {
			name << "<font color=\"#D0AB62\">NAME</font>";
			desc << "DESC";
		} else {
			auto& itemDetails = *itemDataOpt.value();

			const auto& modifiers = itemDetails.GetModifierInstances();
			const auto& names = itemDetails.GetModifierNames();

			if (modifiers.empty() && names.empty()) {
				name << "<font color=\"#D0AB62\">NAME</font>";
				desc << "DESC";
			} else {
				name << ModifierName::GenerateHtmlString(names);

				desc << ModifierInstance::GenerateHtmlString(modifiers);
			}
		}
	}

	std::cout << "Sending item name: " << name.str() << std::endl;
	std::cout << "Sending item desc: " << desc.str() << std::endl;

	std::stringstream messageName;
	messageName << "desc" << requestId;

	AMFArrayValue amfArgs;

	amfArgs.Insert("t", true);
	amfArgs.Insert("d", desc.str());
	amfArgs.Insert("n", name.str());

	GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, messageName.str(), amfArgs);

	std::cout << "Sent item description." << std::endl;
}
