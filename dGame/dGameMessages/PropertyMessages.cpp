#include "PropertyMessages.h"

#include "Amf3.h"
#include "AMFDeserialize.h"
#include "AmfSerialize.h"
#include "BitStreamUtils.h"
#include "CDClientManager.h"
#include "CDPropertyEntranceComponentTable.h"
#include "CDPropertyTemplateTable.h"
#include "Character.h"
#include "ControlBehaviors.h"
#include "Database.h"
#include "dZoneManager.h"
#include "eReplicaComponentType.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "HotPropertySlots.h"
#include "Logger.h"
#include "MultiZoneEntranceComponent.h"
#include "PlayerManager.h"
#include "PlayerReports.h"
#include "PropertyEntranceComponent.h"
#include "PropertyManagementComponent.h"
#include "PropertyVendorComponent.h"
#include "User.h"
#include "UserManager.h"

#include <chrono>

namespace {
	using HotPropertyInfo = GameMessages::NewsSendHotPropertiesInfoToClient::HotPropertyInfo;

	// The news screen's slots and the property worlds they can show properties of, from the CDClient (see HotPropertySlots.h)
	struct NewsWorlds {
		std::vector<HotPropertySlots::Slot> slots;
		std::vector<uint32_t> worlds;
	};

	const NewsWorlds& GetNewsWorlds() {
		static const auto news = [] {
			std::vector<HotPropertySlots::TemplateRow> templates;
			for (const auto& row : CDClientManager::GetTable<CDPropertyTemplateTable>()->GetEntries()) templates.push_back({ row.id, row.mapID, row.spawnName });
			std::vector<HotPropertySlots::EntranceRow> entrances;
			for (const auto& row : CDClientManager::GetTable<CDPropertyEntranceComponentTable>()->GetEntries()) entrances.push_back({ row.mapID, row.propertyName });
			return NewsWorlds{ HotPropertySlots::ResolveSlots(templates, entrances), HotPropertySlots::PropertyWorlds(templates, entrances) };
		}();
		return news;
	}

	HotPropertyInfo ToHotProperty(const IProperty::Info& info, const std::string& ownerName, uint32_t templateId) {
		HotPropertyInfo hot;
		hot.propertyId = info.id;
		hot.ownerId = info.ownerId;
		hot.ownerName = GeneralUtils::UTF8ToUTF16(ownerName);
		hot.reputation = info.reputation;
		hot.templateId = static_cast<int32_t>(templateId);
		hot.name = GeneralUtils::UTF8ToUTF16(info.name); // empty: the client shows the template's name
		hot.description = GeneralUtils::UTF8ToUTF16(info.description);
		hot.performanceCost = info.performanceCost;
		hot.lastPublished = info.lastUpdatedTime;
		hot.cloneId = info.cloneId;
		return hot;
	}

	// What each slot shows, as chosen on the dashboard (featured_properties, featured_properties_settings), resolved by
	// HotPropertySlots::Resolve so no property is shown twice. Every entry carries its slot's template id, whatever
	// world the property is on: that is what puts it in the slot. Empty slots are left out.
	std::vector<HotPropertyInfo> LoadHotProperties() {
		const auto& news = GetNewsWorlds();
		const bool fullAuto = Database::Get()->GetFeaturedPropertiesSettings().fullAuto;
		std::map<uint32_t, IFeaturedProperties::FeaturedSlot> chosen;
		for (const auto& row : Database::Get()->GetFeaturedPropertySlots()) chosen[row.templateId] = row;

		std::vector<HotPropertySlots::Choice> choices;
		for (const auto& slot : news.slots) {
			const auto it = chosen.find(slot.templateId);
			HotPropertySlots::Choice choice{ HotPropertySlots::eMode::AUTO, slot.mapId };
			if (it != chosen.end()) choice = { HotPropertySlots::ModeFromInt(it->second.mode), HotPropertySlots::Location(it->second.zoneId, slot, news.worlds), it->second.propertyId };
			choices.push_back(choice);
		}

		// The candidates, with what to send for each
		std::vector<HotPropertySlots::Candidate> candidates;
		std::map<LWOOBJID, std::pair<IProperty::Info, std::string>> properties;
		for (const auto world : HotPropertySlots::CandidateWorlds(choices, fullAuto)) {
			IProperty::ShowcaseQuery query;
			query.zoneId = world;
			query.sort = IProperty::ShowcaseSort::REPUTATION;
			query.length = HotPropertySlots::CANDIDATES_PER_WORLD;
			for (const auto& entry : Database::Get()->GetShowcaseProperties(query).entries) {
				candidates.push_back({ entry.info.id, entry.info.zoneId, entry.info.reputation });
				properties.emplace(entry.info.id, std::make_pair(entry.info, entry.ownerName));
			}
		}
		for (const auto& choice : choices) {
			if (fullAuto || choice.mode != HotPropertySlots::eMode::PICKED || properties.contains(choice.propertyId)) continue;
			const auto info = Database::Get()->GetPropertyInfo(choice.propertyId);
			const auto owner = info ? Database::Get()->GetCharacterInfo(info->ownerId) : std::nullopt;
			if (!info || !owner || !HotPropertySlots::Featurable(info->modApproved, info->privacyOption, info->zoneId, choice.mapId)) continue;
			candidates.push_back({ info->id, info->zoneId, info->reputation });
			properties.emplace(info->id, std::make_pair(*info, owner->name));
		}

		std::vector<HotPropertyInfo> hot;
		const auto showing = HotPropertySlots::Resolve(choices, fullAuto, candidates);
		for (size_t i = 0; i < showing.size(); i++) {
			if (!showing[i].propertyId) continue;
			const auto& [info, ownerName] = properties.at(*showing[i].propertyId);
			hot.push_back(ToHotProperty(info, ownerName, news.slots[i].templateId));
		}
		return hot;
	}
}

namespace GameMessages {
	void DownloadPropertyData::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(propertyId);
		bitStream.Write(templateId);
		bitStream.Write(mapId);
		bitStream.Write(vendorMapId);
		bitStream.Write(cloneId);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, name);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, description);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, ownerName);
		bitStream.Write(ownerId);
		bitStream.Write(propertyType);
		bitStream.Write(zoneCode);
		bitStream.Write(rent);
		bitStream.Write(rentalPeriod);
		bitStream.Write(expirationDate);
		bitStream.Write(rentAmount);
		bitStream.Write(reputation);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, spawnName);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, templateName);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, templateDescription);
		bitStream.Write(rentDuration);
		bitStream.Write(votes);
		bitStream.Write(durationType);
		bitStream.Write(renew);
		bitStream.Write(ownerAccountID);
		bitStream.Write(moderationStatus);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, rejectionReason);
		bitStream.Write(lastLogoutTime);
		bitStream.Write(dayOfMonthPlaqueWasBought);
		bitStream.Write(repAchievementReq);
		bitStream.Write(zonePosition.x);
		bitStream.Write(zonePosition.y);
		bitStream.Write(zonePosition.z);
		bitStream.Write(maxBuildHeight);
		bitStream.Write(rentalDate);
		bitStream.Write(accessType);
		bitStream.Write<uint32_t>(pathPositions.size());
		for (const auto& path : pathPositions) {
			bitStream.Write(path.x);
			bitStream.Write(path.y);
			bitStream.Write(path.z);
		}
	}

	bool DownloadPropertyData::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(propertyId));
		VALIDATE_READ(bitStream.Read(templateId));
		VALIDATE_READ(bitStream.Read(mapId));
		VALIDATE_READ(bitStream.Read(vendorMapId));
		VALIDATE_READ(bitStream.Read(cloneId));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, name));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, description));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, ownerName));
		VALIDATE_READ(bitStream.Read(ownerId));
		VALIDATE_READ(bitStream.Read(propertyType));
		VALIDATE_READ(bitStream.Read(zoneCode));
		VALIDATE_READ(bitStream.Read(rent));
		VALIDATE_READ(bitStream.Read(rentalPeriod));
		VALIDATE_READ(bitStream.Read(expirationDate));
		VALIDATE_READ(bitStream.Read(rentAmount));
		VALIDATE_READ(bitStream.Read(reputation));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, spawnName));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, templateName));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, templateDescription));
		VALIDATE_READ(bitStream.Read(rentDuration));
		VALIDATE_READ(bitStream.Read(votes));
		VALIDATE_READ(bitStream.Read(durationType));
		VALIDATE_READ(bitStream.Read(renew));
		VALIDATE_READ(bitStream.Read(ownerAccountID));
		VALIDATE_READ(bitStream.Read(moderationStatus));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, rejectionReason));
		VALIDATE_READ(bitStream.Read(lastLogoutTime));
		VALIDATE_READ(bitStream.Read(dayOfMonthPlaqueWasBought));
		VALIDATE_READ(bitStream.Read(repAchievementReq));
		VALIDATE_READ(bitStream.Read(zonePosition.x));
		VALIDATE_READ(bitStream.Read(zonePosition.y));
		VALIDATE_READ(bitStream.Read(zonePosition.z));
		VALIDATE_READ(bitStream.Read(maxBuildHeight));
		VALIDATE_READ(bitStream.Read(rentalDate));
		VALIDATE_READ(bitStream.Read(accessType));
		uint32_t pathCount{};
		VALIDATE_READ(bitStream.Read(pathCount));
		if (static_cast<uint64_t>(pathCount) * 96 > bitStream.GetNumberOfUnreadBits()) return false;
		pathPositions.resize(pathCount);
		for (auto& path : pathPositions) {
			VALIDATE_READ(bitStream.Read(path.x));
			VALIDATE_READ(bitStream.Read(path.y));
			VALIDATE_READ(bitStream.Read(path.z));
		}
		return true;
	}

	void PropertyRentalResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(cloneid);
		bitStream.Write(code);
		bitStream.Write(propertyID);
		bitStream.Write(rentdue);
	}

	bool PropertyRentalResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(cloneid));
		VALIDATE_READ(bitStream.Read(code));
		VALIDATE_READ(bitStream.Read(propertyID));
		VALIDATE_READ(bitStream.Read(rentdue));
		return true;
	}

	void PropertySelectQuery::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(navOffset);
		bitStream.Write(thereAreMore);
		bitStream.Write(cloneId);
		bitStream.Write(hasFeaturedProperty);
		bitStream.Write(wasFriends);
		bitStream.Write<uint32_t>(properties.size());
		for (const auto& property : properties) {
			bitStream.Write(property.cloneId);
			BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, property.ownerName);
			BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, property.name);
			BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, property.description);
			bitStream.Write(property.reputation);
			bitStream.Write(property.isBff);
			bitStream.Write(property.isFriend);
			bitStream.Write(property.isModApproved);
			bitStream.Write(property.isAlt);
			bitStream.Write(property.isOwned);
			bitStream.Write(property.accessType);
			bitStream.Write(property.dateLastPublished);
			bitStream.Write(property.performanceCost);
		}
	}

	bool PropertySelectQuery::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(navOffset));
		VALIDATE_READ(bitStream.Read(thereAreMore));
		VALIDATE_READ(bitStream.Read(cloneId));
		VALIDATE_READ(bitStream.Read(hasFeaturedProperty));
		VALIDATE_READ(bitStream.Read(wasFriends));
		uint32_t count{};
		VALIDATE_READ(bitStream.Read(count));
		properties.clear();
		for (uint32_t i = 0; i < count; i++) {
			auto& property = properties.emplace_back();
			VALIDATE_READ(bitStream.Read(property.cloneId));
			VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, property.ownerName));
			VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, property.name));
			VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, property.description));
			VALIDATE_READ(bitStream.Read(property.reputation));
			VALIDATE_READ(bitStream.Read(property.isBff));
			VALIDATE_READ(bitStream.Read(property.isFriend));
			VALIDATE_READ(bitStream.Read(property.isModApproved));
			VALIDATE_READ(bitStream.Read(property.isAlt));
			VALIDATE_READ(bitStream.Read(property.isOwned));
			VALIDATE_READ(bitStream.Read(property.accessType));
			VALIDATE_READ(bitStream.Read(property.dateLastPublished));
			VALIDATE_READ(bitStream.Read(property.performanceCost));
		}
		return true;
	}

	void GetModelsOnProperty::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint32_t>(models.size());
		for (const auto& [model, item] : models) {
			bitStream.Write(model);
			bitStream.Write(item);
		}
	}

	bool GetModelsOnProperty::Deserialize(RakNet::BitStream& bitStream) {
		uint32_t count{};
		VALIDATE_READ(bitStream.Read(count));
		if (static_cast<uint64_t>(count) * 128 > bitStream.GetNumberOfUnreadBits()) return false;
		models.resize(count);
		for (auto& [model, item] : models) {
			VALIDATE_READ(bitStream.Read(model));
			VALIDATE_READ(bitStream.Read(item));
		}
		return true;
	}

	void PlaceModelResponse::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, position, NiPoint3Constant::ZERO);
		BitStreamUtils::WriteOptional(bitStream, propertyPlaqueID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, response, 0);
		BitStreamUtils::WriteOptional(bitStream, rotation, QuatUtils::IDENTITY);
	}

	bool PlaceModelResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, position, NiPoint3Constant::ZERO));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, propertyPlaqueID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, response, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, rotation, QuatUtils::IDENTITY));
		return true;
	}

	void HandleUGCEquipPreCreateBasedOnEditMode::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(modelCount);
		bitStream.Write(modelID);
	}

	bool HandleUGCEquipPreCreateBasedOnEditMode::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(modelCount));
		VALIDATE_READ(bitStream.Read(modelID));
		return true;
	}

	void HandleUGCEquipPostDeleteBasedOnEditMode::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(invItem);
		BitStreamUtils::WriteOptional(bitStream, itemsTotal, 0);
	}

	bool HandleUGCEquipPostDeleteBasedOnEditMode::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(invItem));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, itemsTotal, 0));
		return true;
	}

	void SetPropertyAccess::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional<uint8_t>(bitStream, accessType, 0);
		BitStreamUtils::WriteOptional(bitStream, renew, 0);
	}

	bool SetPropertyAccess::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional<uint8_t>(bitStream, accessType, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, renew, 0));
		return true;
	}

	void SetPropertyAccess::Handle(Entity& entity, const SystemAddress& sysAddr) {
		LOG("Set privacy option to: %i", accessType);

		if (PropertyManagementComponent::Instance() == nullptr) return;

		PropertyManagementComponent::Instance()->SetPrivacyOption(static_cast<PropertyPrivacyOption>(accessType));
	}

	void UpdatePropertyOrModelForFilterCheck::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(isProperty);
		bitStream.Write(ugcId);
		bitStream.Write(playerId);
		bitStream.Write(worldId);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, newDescription);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, newName);
	}

	bool UpdatePropertyOrModelForFilterCheck::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(isProperty));
		VALIDATE_READ(bitStream.Read(ugcId));
		VALIDATE_READ(bitStream.Read(playerId));
		VALIDATE_READ(bitStream.Read(worldId));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, newDescription));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, newName));
		return true;
	}

	void UpdatePropertyOrModelForFilterCheck::Handle(Entity& entity, const SystemAddress& sysAddr) {
		PropertyManagementComponent::Instance()->UpdatePropertyDetails(GeneralUtils::UTF16ToWTF8(newName), GeneralUtils::UTF16ToWTF8(newDescription));
	}

	void QueryPropertyData::Handle(Entity& entity, const SystemAddress& sysAddr) {
		LOG("Entity (%i) requesting data", entity.GetLOT());

		auto* propertyVendorComponent = static_cast<PropertyVendorComponent*>(entity.GetComponent(eReplicaComponentType::PROPERTY_VENDOR));

		if (propertyVendorComponent != nullptr) {
			propertyVendorComponent->OnQueryPropertyData(&entity, sysAddr);
		}

		auto* propertyManagerComponent = static_cast<PropertyManagementComponent*>(entity.GetComponent(eReplicaComponentType::PROPERTY_MANAGEMENT));

		if (propertyManagerComponent != nullptr) {
			propertyManagerComponent->OnQueryPropertyData(&entity, sysAddr);
		}
	}

	void PropertyEditorBegin::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, distanceType, 0);
		BitStreamUtils::WriteOptional(bitStream, propertyObjectID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, startMode, 1);
		bitStream.Write(startPaused);
	}

	bool PropertyEditorBegin::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, distanceType, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, propertyObjectID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, startMode, 1));
		VALIDATE_READ(bitStream.Read(startPaused));
		return true;
	}

	void PropertyEditorBegin::Handle(Entity& entity, const SystemAddress& sysAddr) {
		PropertyManagementComponent::Instance()->OnStartBuilding();

		Game::zoneManager->GetZoneControlObject()->OnZonePropertyEditBegin();
	}

	void PropertyEditorEnd::Handle(Entity& entity, const SystemAddress& sysAddr) {
		PropertyManagementComponent::Instance()->OnFinishBuilding();

		Game::zoneManager->GetZoneControlObject()->OnZonePropertyEditEnd();
	}

	void PropertyContentsFromClient::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(queryDB);
	}

	bool PropertyContentsFromClient::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(queryDB));
		return true;
	}

	void PropertyContentsFromClient::Handle(Entity& entity, const SystemAddress& sysAddr) {
		User* user = UserManager::Instance()->GetUser(sysAddr);

		Entity* player = Game::entityManager->GetEntity(user->GetLoggedInChar());

		const auto& models = PropertyManagementComponent::Instance()->GetModels();
		GetModelsOnProperty modelsOnProperty;
		modelsOnProperty.target = player->GetObjectID();
		modelsOnProperty.models = { models.begin(), models.end() };
		LOG("Sending property models to (%llu) (%d)", modelsOnProperty.target, true);
		modelsOnProperty.Send(UNASSIGNED_SYSTEM_ADDRESS);
	}

	void ZonePropertyModelEquipped::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, playerID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, propertyID, LWOOBJID_EMPTY);
	}

	bool ZonePropertyModelEquipped::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, playerID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, propertyID, LWOOBJID_EMPTY));
		return true;
	}

	void ZonePropertyModelEquipped::Handle(Entity& entity, const SystemAddress& sysAddr) {
		Game::zoneManager->GetZoneControlObject()->OnZonePropertyModelEquipped();
	}

	void ZonePropertyModelRotated::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, playerID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, propertyID, LWOOBJID_EMPTY);
	}

	bool ZonePropertyModelRotated::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, playerID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, propertyID, LWOOBJID_EMPTY));
		return true;
	}

	void ZonePropertyModelRotated::Handle(Entity& entity, const SystemAddress& sysAddr) {
		User* usr = UserManager::Instance()->GetUser(sysAddr);
		Game::entityManager->GetZoneControlEntity()->OnZonePropertyModelRotated(usr->GetLastUsedChar()->GetEntity());
	}

	void PlacePropertyModel::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(modelID);
	}

	bool PlacePropertyModel::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(modelID));
		return true;
	}

	void PlacePropertyModel::Handle(Entity& entity, const SystemAddress& sysAddr) {
		PropertyManagementComponent::Instance()->UpdateModelPosition(modelID, NiPoint3Constant::ZERO, QuatUtils::IDENTITY);
	}

	void UpdateModelFromClient::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(modelID);
		bitStream.Write(position);
		BitStreamUtils::WriteOptional(bitStream, rotation, QuatUtils::IDENTITY);
	}

	bool UpdateModelFromClient::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(modelID));
		VALIDATE_READ(bitStream.Read(position));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, rotation, QuatUtils::IDENTITY));
		return true;
	}

	void UpdateModelFromClient::Handle(Entity& entity, const SystemAddress& sysAddr) {
		PropertyManagementComponent::Instance()->UpdateModelPosition(modelID, position, rotation);
	}

	void DeleteModelFromClient::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, modelID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, reason, 0);
	}

	bool DeleteModelFromClient::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, modelID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, reason, 0));
		return true;
	}

	void DeleteModelFromClient::Handle(Entity& entity, const SystemAddress& sysAddr) {
		PropertyManagementComponent::Instance()->DeleteModel(modelID, reason);
	}

	void PropertyEntranceSync::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(includeNullAddress);
		bitStream.Write(includeNullDescription);
		bitStream.Write(playersOwn);
		bitStream.Write(updateUI);
		bitStream.Write(numResults);
		bitStream.Write(reputationTime);
		bitStream.Write(sortMethod);
		bitStream.Write(startIndex);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, filterText);
	}

	bool PropertyEntranceSync::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(includeNullAddress));
		VALIDATE_READ(bitStream.Read(includeNullDescription));
		VALIDATE_READ(bitStream.Read(playersOwn));
		VALIDATE_READ(bitStream.Read(updateUI));
		VALIDATE_READ(bitStream.Read(numResults));
		VALIDATE_READ(bitStream.Read(reputationTime));
		VALIDATE_READ(bitStream.Read(sortMethod));
		VALIDATE_READ(bitStream.Read(startIndex));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, filterText));
		return true;
	}

	void PropertyEntranceSync::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* player = PlayerManager::GetPlayer(sysAddr);

		auto* entranceComponent = entity.GetComponent<PropertyEntranceComponent>();

		if (entranceComponent == nullptr) return;

		entranceComponent->OnPropertyEntranceSync(player,
			includeNullAddress,
			includeNullDescription,
			playersOwn,
			updateUI,
			numResults,
			reputationTime,
			sortMethod,
			startIndex,
			filterText,
			sysAddr
		);
	}

	void EnterProperty1::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(index);
		bitStream.Write(returnToZone);
	}

	bool EnterProperty1::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(index));
		VALIDATE_READ(bitStream.Read(returnToZone));
		return true;
	}

	void EnterProperty1::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* player = PlayerManager::GetPlayer(sysAddr);

		auto* entranceComponent = entity.GetComponent<PropertyEntranceComponent>();
		if (entranceComponent != nullptr) {
			entranceComponent->OnEnterProperty(player, static_cast<uint32_t>(index), returnToZone, sysAddr);
			return;
		}

		auto multiZoneEntranceComponent = entity.GetComponent<MultiZoneEntranceComponent>();
		if (multiZoneEntranceComponent != nullptr) {
			multiZoneEntranceComponent->OnSelectWorld(player, static_cast<uint32_t>(index));
		}
	}

	void UpdatePropertyPerformanceCost::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, performanceCost, 0.0f);
	}

	bool UpdatePropertyPerformanceCost::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, performanceCost, 0.0f));
		return true;
	}

	void UpdatePropertyPerformanceCost::Handle(Entity& entity, const SystemAddress& sysAddr) {
		if (performanceCost == 0.0f) return;

		auto zone = Game::zoneManager->GetZone();
		if (!zone) {
			LOG("If you see this message, something is very wrong.");
			return;
		}

		const auto* const propertyManagementComponent = entity.GetComponent<PropertyManagementComponent>();
		const auto* const ownerEntity = propertyManagementComponent ? propertyManagementComponent->GetOwner() : nullptr;
		const auto* const character = ownerEntity ? ownerEntity->GetCharacter() : nullptr;
		const auto& zoneID = zone->GetZoneID();
		if (character && character->GetPropertyCloneID() == zoneID.GetCloneID()) {
			Database::Get()->UpdatePerformanceCost(zoneID, performanceCost);
		}
	}

	void ReportOffensiveModel::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, description);
		bitStream.Write(offensiveObjectID);
	}

	bool ReportOffensiveModel::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, description));
		VALIDATE_READ(bitStream.Read(offensiveObjectID));
		return true;
	}

	void ReportOffensiveModel::Handle(Entity& entity, const SystemAddress& sysAddr) {
		PlayerReports::ReportOffensiveModel(&entity, description, offensiveObjectID);
	}

	void ReportOffensiveProperty::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, description);
		bitStream.Write(propertyPlaqueObjectID);
	}

	bool ReportOffensiveProperty::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, description));
		VALIDATE_READ(bitStream.Read(propertyPlaqueObjectID));
		return true;
	}

	void ReportOffensiveProperty::Handle(Entity& entity, const SystemAddress& sysAddr) {
		PlayerReports::ReportOffensiveProperty(&entity, description, propertyPlaqueObjectID);
	}

	ControlBehaviors::ControlBehaviors() : NetGameMsg(MessageType::Game::CONTROL_BEHAVIORS) {}
	ControlBehaviors::~ControlBehaviors() = default;

	void ControlBehaviors::Serialize(RakNet::BitStream& bitStream) const {
		AMFArrayValue empty;
		bitStream.Write<AMFBaseValue&>(args ? *args : empty);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, command);
	}

	bool ControlBehaviors::Deserialize(RakNet::BitStream& bitStream) {
		AMFDeserialize reader;
		try {
			auto deserializedData = reader.Read(bitStream);
			if (!deserializedData || deserializedData->GetValueType() != eAmf::Array) {
				LOG("Failed to deserialize AMF data for control behaviors command: not an array");
				return false;
			}

			args.reset(static_cast<AMFArrayValue*>(deserializedData.release()));
		} catch (...) {
			LOG("Failed to deserialize AMF data for control behaviors command");
			return false;
		}

		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, command)); // Prevent DoS via unbounded command buffer
		return true;
	}

	void ControlBehaviors::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* const owner = PropertyManagementComponent::Instance()->GetOwner();
		if (!owner) return;

		::ControlBehaviors::Instance().ProcessCommand(&entity, *args, command, owner);
	}

	void GetHotPropertyData::Handle(Entity& entity, const SystemAddress& sysAddr) {
		// The screen asks every time it opens; the answer is the same for everyone, so it is kept for a little while
		static std::vector<HotPropertyInfo> cached;
		static std::chrono::steady_clock::time_point loadedAt{};
		static bool loaded = false;
		const auto now = std::chrono::steady_clock::now();
		if (!loaded || now - loadedAt > std::chrono::seconds(30)) {
			try {
				cached = LoadHotProperties();
			} catch (const std::exception& ex) {
				LOG("Failed to load the news screen's top properties: %s", ex.what());
				cached.clear();
			}
			loadedAt = now;
			loaded = true;
		}

		NewsSendHotPropertiesInfoToClient message;
		message.target = entity.GetObjectID();
		for (const auto index : HotPropertySlots::NewsOrder(cached.size())) message.properties.push_back(cached[index]);
		message.Send(sysAddr);
	}

	void NewsSendHotPropertiesInfoToClient::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint32_t>(properties.size());
		for (const auto& info : properties) {
			bitStream.Write(info.propertyId);
			bitStream.Write(info.ownerId);
			BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, info.ownerName);
			bitStream.Write(info.reputation);
			bitStream.Write(info.templateId);
			BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, info.name);
			BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, info.description);
			bitStream.Write(info.performanceCost);
			bitStream.Write(info.lastPublished);
			bitStream.Write(info.cloneId);
		}
	}

	bool NewsSendHotPropertiesInfoToClient::Deserialize(RakNet::BitStream& bitStream) {
		uint32_t count{};
		VALIDATE_READ(bitStream.Read(count));
		properties.clear();
		for (uint32_t i = 0; i < count; i++) {
			auto& info = properties.emplace_back();
			VALIDATE_READ(bitStream.Read(info.propertyId));
			VALIDATE_READ(bitStream.Read(info.ownerId));
			VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, info.ownerName));
			VALIDATE_READ(bitStream.Read(info.reputation));
			VALIDATE_READ(bitStream.Read(info.templateId));
			VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, info.name));
			VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, info.description));
			VALIDATE_READ(bitStream.Read(info.performanceCost));
			VALIDATE_READ(bitStream.Read(info.lastPublished));
			VALIDATE_READ(bitStream.Read(info.cloneId));
		}
		return true;
	}

	void PlayBehaviorSound::Serialize(RakNet::BitStream& stream) const {
		stream.Write(soundID != -1);
		if (soundID != -1) stream.Write(soundID);
	}

	bool PlayBehaviorSound::Deserialize(RakNet::BitStream& stream) {
		return BitStreamUtils::ReadOptional(stream, soundID, -1);
	}
}
