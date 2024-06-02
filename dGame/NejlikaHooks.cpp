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

#include "NejlikaData.h"

using namespace nejlika;
using namespace nejlika::NejlikaData;

void nejlika::NejlikaHooks::InstallHooks()
{
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
	};

	EntityManager::OnEntityDestroyed += [](Entity* entity) {
		UnsetAdditionalEntityData(entity->GetObjectID());
	};

	InventoryComponent::OnItemLoaded += [](InventoryComponent* component, Item* item) {
		SetAdditionalItemData(item->GetId(), AdditionalItemData(item));
	};

	InventoryComponent::OnItemDestroyed += [](InventoryComponent* component, Item* item) {
		UnsetAdditionalItemData(item->GetId());
	};

	InventoryComponent::OnItemEquipped += [](InventoryComponent* component, Item* item) {
		std::cout << "Item equipped: " << item->GetId() << std::endl;

		const auto entityDataOpt = GetAdditionalEntityData(component->GetParent()->GetObjectID());

		if (!entityDataOpt.has_value()) {
			std::cout << "No entity data found for entity." << std::endl;
			return;
		}

		auto& entityData = *entityDataOpt.value();

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

		entityData.ApplyToEntity();
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

		auto* inventoryComponent = offfendEntity->GetComponent<InventoryComponent>();

		if (inventoryComponent) {
			const auto& skills = inventoryComponent->GetSkills();

			std::cout << "Found " << skills.size() << " skills." << std::endl;

			// omg...
			for (const auto& [slot, skill] : skills) {
				std::cout << "Found skill: " << skill << std::endl;
				
				if (skill != skillID) {
					continue;
				}

				const auto& equipped = inventoryComponent->GetEquippedItems();

				for (const auto& [equippedSlot, itemDetails] : equipped) {
					std::cout << "Found equipped item: " << itemDetails.lot << std::endl;

					const auto info = Inventory::FindItemComponent(itemDetails.lot);

					const auto itemBehaviorSlot = InventoryComponent::FindBehaviorSlot(static_cast<eItemType>(info.itemType));

					std::cout << "Comparing slots: " << static_cast<int32_t>(itemBehaviorSlot) << " - " << static_cast<int32_t>(slot) << std::endl;

					if (itemBehaviorSlot == slot) {
						itemLot = itemDetails.lot;
						itemId = itemDetails.id;

						std::cout << "Found item: " << itemLot << std::endl;

						break;
					}
				}
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

		// Remove the following: Offensive, Defensive, Health, Armor, Imagination
		damageTypes.erase(ModifierType::Offensive);
		damageTypes.erase(ModifierType::Defensive);
		damageTypes.erase(ModifierType::Health);
		damageTypes.erase(ModifierType::Armor);
		damageTypes.erase(ModifierType::Imagination);

		uint32_t totalDamage = 0;

		for (const auto& type : damageTypes) {
			float damageValue = offenderEntity.CalculateModifier(type, modifiers, level);

			// Calculate resistance, can't go below 20% of the original damage
			const auto resistance = std::max(1 - (damagedEntity.CalculateResistance(type) / 100), 0.2f);

			damageValue *= resistance;

			totalDamage += static_cast<uint32_t>(damageValue);

			std::cout << "Damage type: " << magic_enum::enum_name(type) << " - " << damageValue << std::endl << " Resistance: " << resistance << std::endl;
			std::cout << "Heath left: " << damaged->GetComponent<DestroyableComponent>()->GetHealth() << std::endl;
		}

		// Get the offenders Offensive modifier
		auto offenderModifiers = offenderEntity.CalculateModifier(ModifierType::Offensive, level);

		// Get the defenders Defensive modifier
		auto defensiveModifiers = damagedEntity.CalculateModifier(ModifierType::Defensive, level);

		if (offenderModifiers == 0) offenderModifiers = 1;
		if (defensiveModifiers == 0) defensiveModifiers = 1;

		auto ratio = offenderModifiers / defensiveModifiers;

		// Ratio can not ge below 1.05
		ratio = std::max(ratio, 1.05f);

		// Roll a number between 0 and ratio
		float roll = GeneralUtils::GenerateRandomNumber<size_t>() / static_cast<float>(std::numeric_limits<size_t>::max());

		roll *= ratio;

		std::cout << "Offensive: " << offenderModifiers << " Defensive: " << defensiveModifiers << " Ratio: " << ratio << " Roll: " << roll << std::endl;

		// If the roll is above 1, the damage is increased by 1+roll, to a maximum of 5x the damage
		if (roll > 1) {
			roll = std::min(roll, 5.0f);
			totalDamage += static_cast<uint32_t>(totalDamage * roll);

			GameMessages::SendPlayFXEffect(
				damaged->GetObjectID(),
				20041,
				u"onhit",
				std::to_string(GeneralUtils::GenerateRandomNumber<uint32_t>())
			);
		}

		// Add a random +10% to the damage
		totalDamage += static_cast<uint32_t>(totalDamage * (GeneralUtils::GenerateRandomNumber<int32_t>(0, 10) / 100.0f));

		damage = totalDamage;

		if (offfendEntity->IsPlayer()) {
			offfendEntity->AddCallbackTimer(0.0f, [offfendEntity, skillID]() {
				CBITSTREAM;
				CMSGHEADER;

				const auto entity = offfendEntity->GetObjectID();

				bitStream.Write(entity);
				bitStream.Write(eGameMessageType::MODIFY_SKILL_COOLDOWN);

				bitStream.Write1();
				bitStream.Write<float>(-10.0f);
				bitStream.Write<int32_t>(static_cast<int32_t>(skillID));

				LOG("Sending cooldown reduction for skill: %d", skillID);

				SEND_PACKET_BROADCAST;
			});
		}
	};
}


void nejlika::NejlikaHooks::ItemDescription(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	auto splitArgs = GeneralUtils::SplitString(args, ' ');
	if (splitArgs.size() < 2) {
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

	if (itemId == LWOOBJID_EMPTY) {
		ChatPackets::SendSystemMessage(sysAddr, u"Invalid item ID.");
		return;
	}
	
	const auto itemDataOpt = GetAdditionalItemData(itemId);

	if (!itemDataOpt.has_value()) {
		return;
	}

	auto& itemDetails = *itemDataOpt.value();

	const auto& modifiers = itemDetails.GetModifierInstances();
	const auto& names = itemDetails.GetModifierNames();

	if (modifiers.empty() && names.empty()) {
		return;
	}

	std::stringstream name;
	std::stringstream desc;

	name << "NAME";

	desc << ModifierName::GenerateHtmlString(names) << "\n";
	
	desc << ModifierInstance::GenerateHtmlString(modifiers);

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