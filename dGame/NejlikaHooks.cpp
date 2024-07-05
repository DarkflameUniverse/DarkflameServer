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

#include "NejlikaData.h"

using namespace nejlika;
using namespace nejlika::NejlikaData;

void nejlika::NejlikaHooks::InstallHooks() {
	Command itemDescriptionCommand{
		.help = "Special UI command, does nothing when used in chat.",
		.info = "Special UI command, does nothing when used in chat.",
		.aliases = {"d"},
		.handle = ItemDescription,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	SlashCommandHandler::RegisterCommand(itemDescriptionCommand);

	LoadNejlikaData();

	InventoryComponent::OnItemCreated += [](InventoryComponent* component, Item* item) {
		const auto& itemType = static_cast<eItemType>(item->GetInfo().itemType);

		/*
		static const std::unordered_set<eItemType> listOfHandledItems {
			eItemType::HAT,
			eItemType::CHEST,
			eItemType::LEGS,
			eItemType::NECK,
			eItemType::LEFT_HAND,
			eItemType::RIGHT_HAND
		};

		if (listOfHandledItems.find(itemType) == listOfHandledItems.end()) {
			return;
		}
		*/

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

		additionalData.ApplyToEntity();

		auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

		if (!inventoryComponent) {
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

		additionalData.InitializeSkills();
	};

	Entity::OnReadyForUpdates += [](Entity* entity) {
		if (!entity->IsPlayer()) {
			return;
		}

		//GameMessages::SendAddSkill(entity, NejlikaData::GetLookup().GetValue("intro:skills:proxy:main"), BehaviorSlot::Head);
		//GameMessages::SendAddSkill(entity, NejlikaData::GetLookup().GetValue("intro:skills:proxy:secondary"), BehaviorSlot::Offhand);
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

	SkillComponent::OnSkillCast += [](SkillComponent* skillComponent, uint32_t skillID, bool success) {
		std::cout << "Skill cast: " << skillID << " - " << success << std::endl;

		auto* inventoryComponent = skillComponent->GetParent()->GetComponent<InventoryComponent>();

		if (!inventoryComponent) {
			return;
		}

		auto* entity = skillComponent->GetParent();

		auto skills = inventoryComponent->GetSkills();

		const auto primaryTrigger = NejlikaData::GetLookup().GetValue("intro:skills:proxy:main");
		const auto secondaryTrigger = NejlikaData::GetLookup().GetValue("intro:skills:proxy:secondary");
		const auto tertiaryTrigger = NejlikaData::GetLookup().GetValue("intro:skills:proxy:tertiary");

		if (skillID == primaryTrigger || skillID == secondaryTrigger || skillID == tertiaryTrigger) {
		} else {
			return;
		}

		inventoryComponent->RotateSkills();
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

		const auto& skillTemplates = GetModifierNameTemplates(ModifierNameType::Skill);

		const auto& skillTemplateIt = std::find_if(skillTemplates.begin(), skillTemplates.end(), [skillID](const auto& it) {
			return it.GetLOT() == skillID;
			});

		std::vector<ModifierInstance> modifiers;

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

		uint32_t totalDamage = 0;

		std::unordered_map<ModifierType, std::pair<float, float>> durationTypes;
		std::unordered_map<ModifierType, float> tmpDamageValues;

		for (const auto& type : damageTypes) {
			if (nejlika::IsOverTimeType(type)) {
				float damageValue = offenderEntity.CalculateModifier(type, modifiers, level);

				// Calculate resistance, can't go below 20% of the original damage
				const auto resistance = std::max(1 - (damagedEntity.CalculateResistance(type) / 100), 0.2f);

				float reductedDamage = damageValue * resistance;

				const auto durationType = nejlika::GetDurationType(type);

				const auto duration = offenderEntity.CalculateModifier(durationType, modifiers, level);

				const auto durationResistance = std::max(1 - (damagedEntity.CalculateResistance(durationType) / 100), 0.2f);

				float reductedDuration = duration * durationResistance;

				durationTypes[type] = std::make_pair(reductedDamage, reductedDuration);

				continue;
			}

			if (!nejlika::IsNormalDamageType(type)) {
				continue;
			}

			float damageValue = offenderEntity.CalculateModifier(type, modifiers, level);

			tmpDamageValues[type] = damageValue;
		}

		// Type A -> Type B -> (0-100) how much of type A is converted to type B
		const auto converationMap = offenderEntity.CalculateDamageConversion(modifiers);

		std::unordered_map<ModifierType, float> finalDamageValues;

		for (const auto& [typeA, typeBMap] : converationMap) {
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
		}

		for (const auto& [type, damage] : finalDamageValues) {
			// Calculate resistance, can't go below 20% of the original damage
			const auto resistance = std::max(1 - (damagedEntity.CalculateResistance(type) / 100), 0.2f);

			float reductedDamage = damage * resistance;

			totalDamage += static_cast<uint32_t>(reductedDamage);

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

				damageMultiplier *= offenderEntity.CalculateMultiplier(ModifierType::CriticalDamage, modifiers);
			}
		}

		if (isHit) {
			// Add a random +5% to the damage
			totalDamage += static_cast<uint32_t>(totalDamage * (GeneralUtils::GenerateRandomNumber<int32_t>(0, 5) / 100.0f));

			damage = totalDamage;
		} else {
			damage = totalDamage = 0;
		}

		if (isCritical) {
			totalDamage = static_cast<uint32_t>(totalDamage * damageMultiplier);

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

		auto attackSpeed = offenderEntity.CalculateModifier(ModifierType::AttackSpeed, modifiers, level);

		if (offfendEntity->IsPlayer()) {
			offfendEntity->AddCallbackTimer(0.0f, [offfendEntity, skillID, attackSpeed]() {
				CBITSTREAM;
				CMSGHEADER;

				const auto entity = offfendEntity->GetObjectID();

				bitStream.Write(entity);
				bitStream.Write(eGameMessageType::MODIFY_SKILL_COOLDOWN);

				bitStream.Write1();
				bitStream.Write<float>(attackSpeed);
				bitStream.Write<int32_t>(static_cast<int32_t>(skillID));

				LOG("Sending cooldown reduction for skill: %d", skillID);

				SEND_PACKET_BROADCAST;
			});
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

			const auto damagePerTick = static_cast<int32_t>(damageDuration.first / duration);

			auto* destroyable = damaged->GetComponent<DestroyableComponent>();

			if (!destroyable) {
				continue;
			}
			
			for (size_t i = 0; i < duration; i++)
			{
				damaged->AddCallbackTimer(i * 0.5f, [offender, damaged, damagePerTick]() {
					auto* destroyable = damaged->GetComponent<DestroyableComponent>();

					if (!destroyable) {
						return;
					}

					destroyable->Damage(offender, damagePerTick, 0, true, true);
				});
			}
		}

		/* Moved to DestroyableComponent
		std::stringstream damageUIMessage;

		auto damagedPosition = damaged->GetPosition();

		// Add a slight random offset to the damage position
		damagedPosition.x += (rand() % 10 - 5) / 5.0f;
		damagedPosition.y += (rand() % 10 - 5) / 5.0f;
		damagedPosition.z += (rand() % 10 - 5) / 5.0f;

		int colorR = 255;
		int colorG = 255;
		int colorB = 255;
		int colorA = 0;

		if (damaged->IsPlayer()) {
			// Make the damage red
			colorR = 0;
			colorG = 255;
			colorB = 0;
			colorA = 0;
		}

		const auto damageText = isHit ? std::to_string(totalDamage) : "Miss";

		damageUIMessage << 0.0825 << ";" << 0.12 << ";" << damagedPosition.x << ";" << damagedPosition.y + 4.5f << ";" << damagedPosition.z << ";" << 0.1 << ";";
		damageUIMessage << 200 << ";" << 200 << ";" << 0.5 << ";" << 1.0 << ";" << damageText << ";" << 4 << ";" << 4 << ";" << colorR << ";" << colorG << ";" << colorB << ";";
		damageUIMessage << colorA;

		const auto damageUIStr = damageUIMessage.str();

		if (damaged->IsPlayer()) {
			damaged->SetNetworkVar<std::string>(u"renderText", damageUIStr, UNASSIGNED_SYSTEM_ADDRESS);
		} else if (offfendEntity->IsPlayer()) {
			offfendEntity->SetNetworkVar<std::string>(u"renderText", damageUIStr, UNASSIGNED_SYSTEM_ADDRESS);
		}*/
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

	if (itemId == LWOOBJID_EMPTY) {
		ChatPackets::SendSystemMessage(sysAddr, u"Invalid item ID.");

		const auto& itemTemplateVec = NejlikaData::GetModifierNameTemplates(ModifierNameType::Object);

		const auto itemTemplateIt = std::find_if(itemTemplateVec.begin(), itemTemplateVec.end(), [lot](const auto& it) {
			return it.GetLOT() == static_cast<int32_t>(lot);
			});

		if (itemTemplateIt == itemTemplateVec.end()) {
			name << "<font color=\"#D0AB62\">NAME</font>";
			desc << "DESC";
		} else {
			const auto& itemTemplate = *itemTemplateIt;

			const auto& modifiers = itemTemplate.GetModifiers();

			// Get the entity level
			auto* levelProgressionComponent = entity->GetComponent<LevelProgressionComponent>();

			if (!levelProgressionComponent) {
				return;
			}

			auto level = levelProgressionComponent->GetLevel();

			name << "<font color=\"#D0AB62\">NAME</font>";
			desc << ModifierTemplate::GenerateHtmlString(modifiers, level);
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
