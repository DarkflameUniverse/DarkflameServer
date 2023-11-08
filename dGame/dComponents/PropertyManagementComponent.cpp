#include "PropertyManagementComponent.h"

#include <sstream>

#include "MissionComponent.h"
#include "EntityManager.h"
#include "PropertyDataMessage.h"
#include "UserManager.h"
#include "GameMessages.h"
#include "Character.h"
#include "CDClientDatabase.h"
#include "dZoneManager.h"
#include "Game.h"
#include "Item.h"
#include "Database.h"
#include "../dWorldServer/ObjectIDManager.h"
#include "Player.h"
#include "RocketLaunchpadControlComponent.h"
#include "PropertyEntranceComponent.h"
#include "InventoryComponent.h"
#include "eMissionTaskType.h"
#include "eObjectBits.h"

#include <vector>
#include "CppScripts.h"

PropertyManagementComponent* PropertyManagementComponent::instance = nullptr;

PropertyManagementComponent::PropertyManagementComponent(Entity* parent) : Component(parent) {
	this->owner = LWOOBJID_EMPTY;
	this->templateId = 0;
	this->propertyId = LWOOBJID_EMPTY;
	this->models = {};
	this->propertyName = "";
	this->propertyDescription = "";
	this->privacyOption = PropertyPrivacyOption::Private;
	this->originalPrivacyOption = PropertyPrivacyOption::Private;

	instance = this;

	const auto& worldId = Game::zoneManager->GetZone()->GetZoneID();
	const auto zoneId = worldId.GetMapID();
	const auto cloneId = worldId.GetCloneID();

	auto query = CDClientDatabase::CreatePreppedStmt("SELECT id FROM PropertyTemplate WHERE mapID = ?;");

	query.bind(1, static_cast<int32_t>(zoneId));

	auto result = query.execQuery();

	if (result.eof() || result.fieldIsNull(0)) {
		return;
	}

	templateId = result.getIntField(0);

	auto propertyInfo = Database::Get()->GetPropertyInfo(zoneId, cloneId);

	if (propertyInfo) {
		this->propertyId = propertyInfo->id;
		this->owner = propertyInfo->ownerId;
		GeneralUtils::SetBit(this->owner, eObjectBits::CHARACTER);
		GeneralUtils::SetBit(this->owner, eObjectBits::PERSISTENT);
		this->clone_Id = propertyInfo->cloneId;
		this->propertyName = propertyInfo->name;
		this->propertyDescription = propertyInfo->description;
		this->privacyOption = static_cast<PropertyPrivacyOption>(propertyInfo->privacyOption);
		this->rejectionReason = propertyInfo->rejectionReason;
		this->moderatorRequested = propertyInfo->modApproved == 0 && rejectionReason == "" && privacyOption == PropertyPrivacyOption::Public;
		this->LastUpdatedTime = propertyInfo->lastUpdatedTime;
		this->claimedTime = propertyInfo->claimedTime;
		this->reputation = propertyInfo->reputation;

		Load();
	}
}

LWOOBJID PropertyManagementComponent::GetOwnerId() const {
	return owner;
}

Entity* PropertyManagementComponent::GetOwner() const {
	return Game::entityManager->GetEntity(owner);
}

void PropertyManagementComponent::SetOwner(Entity* value) {
	owner = value->GetObjectID();
}

std::vector<NiPoint3> PropertyManagementComponent::GetPaths() const {
	const auto zoneId = Game::zoneManager->GetZone()->GetWorldID();

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT path FROM PropertyTemplate WHERE mapID = ?;");
	query.bind(1, (int)zoneId);

	auto result = query.execQuery();

	std::vector<NiPoint3> paths{};

	if (result.eof()) {
		return paths;
	}

	std::vector<float> points;

	std::istringstream stream(result.getStringField(0));
	std::string token;

	while (std::getline(stream, token, ' ')) {
		try {
			auto value = std::stof(token);

			points.push_back(value);
		} catch (std::invalid_argument& exception) {
			LOG("Failed to parse value (%s): (%s)!", token.c_str(), exception.what());
		}
	}

	for (auto i = 0u; i < points.size(); i += 3) {
		paths.emplace_back(points[i], points[i + 1], points[i + 2]);
	}

	return paths;
}

PropertyPrivacyOption PropertyManagementComponent::GetPrivacyOption() const {
	return privacyOption;
}

void PropertyManagementComponent::SetPrivacyOption(PropertyPrivacyOption value) {
	if (owner == LWOOBJID_EMPTY) return;

	if (value == static_cast<PropertyPrivacyOption>(3)) // Client sends 3 for private for some reason, but expects 0 in return?
	{
		value = PropertyPrivacyOption::Private;
	}

	if (value == PropertyPrivacyOption::Public && privacyOption != PropertyPrivacyOption::Public) {
		rejectionReason = "";
		moderatorRequested = true;
	}
	privacyOption = value;

	IProperty::Info info;
	info.id = propertyId;
	info.privacyOption = static_cast<uint32_t>(privacyOption);
	info.rejectionReason = rejectionReason;
	info.modApproved = 0;

	Database::Get()->UpdatePropertyModerationInfo(info);
}

void PropertyManagementComponent::UpdatePropertyDetails(std::string name, std::string description) {
	if (owner == LWOOBJID_EMPTY) return;

	propertyName = name;

	propertyDescription = description;

	IProperty::Info info;
	info.id = propertyId;
	info.name = propertyName;
	info.description = propertyDescription;

	Database::Get()->UpdatePropertyDetails(info);

	OnQueryPropertyData(GetOwner(), UNASSIGNED_SYSTEM_ADDRESS);
}

bool PropertyManagementComponent::Claim(const LWOOBJID playerId) {
	if (owner != LWOOBJID_EMPTY) {
		return false;
	}

	auto* entity = Game::entityManager->GetEntity(playerId);

	auto* user = entity->GetParentUser();

	auto character = entity->GetCharacter();
	if (!character) return false;

	auto* zone = Game::zoneManager->GetZone();

	const auto& worldId = zone->GetZoneID();
	const auto propertyZoneId = worldId.GetMapID();
	const auto propertyCloneId = worldId.GetCloneID();

	const auto playerCloneId = character->GetPropertyCloneID();

	// If we are not on our clone do not allow us to claim the property
	if (propertyCloneId != playerCloneId) return false;

	std::string name = zone->GetZoneName();
	std::string description = "";

	auto prop_path = zone->GetPath(m_Parent->GetVarAsString(u"propertyName"));

	if (prop_path){
		if (!prop_path->property.displayName.empty()) name = prop_path->property.displayName;
		description = prop_path->property.displayDesc;
	}

	SetOwnerId(playerId);

	propertyId = ObjectIDManager::GenerateRandomObjectID();

	IProperty::Info info;
	info.id = propertyId;
	info.ownerId = playerId;
	info.cloneId = playerCloneId;
	info.name = name;
	info.description = description;

	Database::Get()->InsertNewProperty(info, templateId, worldId);

	auto* zoneControlObject = Game::zoneManager->GetZoneControlObject();
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(zoneControlObject)) {
		script->OnZonePropertyRented(zoneControlObject, entity);
	}
	return true;
}

void PropertyManagementComponent::OnStartBuilding() {
	auto* ownerEntity = GetOwner();

	if (ownerEntity == nullptr) return;

	const auto players = Player::GetAllPlayers();

	LWOMAPID zoneId = 1100;

	const auto entrance = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::PROPERTY_ENTRANCE);

	originalPrivacyOption = privacyOption;

	SetPrivacyOption(PropertyPrivacyOption::Private); // Cant visit player which is building

	if (!entrance.empty()) {
		auto* rocketPad = entrance[0]->GetComponent<RocketLaunchpadControlComponent>();

		if (rocketPad != nullptr) {
			zoneId = rocketPad->GetDefaultZone();
		}
	}

	for (auto* player : players) {
		if (player == ownerEntity) continue;

		player->SendToZone(zoneId);
	}
	auto inventoryComponent = ownerEntity->GetComponent<InventoryComponent>();

	// Push equipped items
	if (inventoryComponent) inventoryComponent->PushEquippedItems();
}

void PropertyManagementComponent::OnFinishBuilding() {
	auto* ownerEntity = GetOwner();

	if (ownerEntity == nullptr) return;

	SetPrivacyOption(originalPrivacyOption);

	UpdateApprovedStatus(false);

	Save();
}

void PropertyManagementComponent::UpdateModelPosition(const LWOOBJID id, const NiPoint3 position, NiQuaternion rotation) {
	LOG("Placing model <%f, %f, %f>", position.x, position.y, position.z);

	auto* entity = GetOwner();

	if (entity == nullptr) {
		return;
	}

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	auto* item = inventoryComponent->FindItemById(id);

	if (item == nullptr) {
		LOG("Failed to find item with id %d", id);

		return;
	}

	NiQuaternion originalRotation = rotation;

	const auto modelLOT = item->GetLot();

	if (rotation != NiQuaternion::IDENTITY) {
		rotation = { rotation.w, rotation.z, rotation.y, rotation.x };
	}

	if (item->GetLot() == 6662) {
		LWOOBJID spawnerID = item->GetSubKey();

		EntityInfo info;
		info.lot = 14;
		info.pos = {};
		info.rot = {};
		info.spawner = nullptr;
		info.spawnerID = spawnerID;
		info.spawnerNodeID = 0;

		for (auto* setting : item->GetConfig()) {
			info.settings.push_back(setting->Copy());
		}

		Entity* newEntity = Game::entityManager->CreateEntity(info);
		if (newEntity != nullptr) {
			Game::entityManager->ConstructEntity(newEntity);

			// Make sure the propMgmt doesn't delete our model after the server dies
			// Trying to do this after the entity is constructed. Shouldn't really change anything but
			// There was an issue with builds not appearing since it was placed above ConstructEntity.
			PropertyManagementComponent::Instance()->AddModel(newEntity->GetObjectID(), spawnerID);
		}

		item->SetCount(item->GetCount() - 1);
		return;
	}

	item->SetCount(item->GetCount() - 1);

	auto* node = new SpawnerNode();

	node->position = position;
	node->rotation = rotation;

	ObjectIDManager::Instance()->RequestPersistentID([this, node, modelLOT, entity, position, rotation, originalRotation](uint32_t persistentId) {
		SpawnerInfo info{};

		info.templateID = modelLOT;
		info.nodes = { node };
		info.templateScale = 1.0f;
		info.activeOnLoad = true;
		info.amountMaintained = 1;
		info.respawnTime = 10;

		info.emulated = true;
		info.emulator = Game::entityManager->GetZoneControlEntity()->GetObjectID();

		info.spawnerID = persistentId;
		GeneralUtils::SetBit(info.spawnerID, eObjectBits::CLIENT);

		const auto spawnerId = Game::zoneManager->MakeSpawner(info);

		auto* spawner = Game::zoneManager->GetSpawner(spawnerId);

		auto ldfModelBehavior = new LDFData<LWOOBJID>(u"modelBehaviors", 0);
		auto userModelID = new LDFData<LWOOBJID>(u"userModelID", info.spawnerID);
		auto modelType = new LDFData<int>(u"modelType", 2);
		auto propertyObjectID = new LDFData<bool>(u"propertyObjectID", true);
		auto componentWhitelist = new LDFData<int>(u"componentWhitelist", 1);
		info.nodes[0]->config.push_back(componentWhitelist);
		info.nodes[0]->config.push_back(ldfModelBehavior);
		info.nodes[0]->config.push_back(modelType);
		info.nodes[0]->config.push_back(propertyObjectID);
		info.nodes[0]->config.push_back(userModelID);

		auto* model = spawner->Spawn();

		models.insert_or_assign(model->GetObjectID(), spawnerId);

		GameMessages::SendPlaceModelResponse(entity->GetObjectID(), entity->GetSystemAddress(), position, m_Parent->GetObjectID(), 14, originalRotation);

		GameMessages::SendUGCEquipPreCreateBasedOnEditMode(entity->GetObjectID(), entity->GetSystemAddress(), 0, spawnerId);

		GameMessages::SendGetModelsOnProperty(entity->GetObjectID(), GetModels(), UNASSIGNED_SYSTEM_ADDRESS);

		Game::entityManager->GetZoneControlEntity()->OnZonePropertyModelPlaced(entity);
		});
	// Progress place model missions
	auto missionComponent = entity->GetComponent<MissionComponent>();
	if (missionComponent != nullptr) missionComponent->Progress(eMissionTaskType::PLACE_MODEL, 0);
}

void PropertyManagementComponent::DeleteModel(const LWOOBJID id, const int deleteReason) {
	LOG("Delete model: (%llu) (%i)", id, deleteReason);

	auto* entity = GetOwner();

	if (entity == nullptr) {
		return;
	}

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	const auto index = models.find(id);

	if (index == models.end()) {
		LOG("Failed to find model");

		return;
	}

	const auto spawnerId = index->second;

	auto* spawner = Game::zoneManager->GetSpawner(spawnerId);

	models.erase(id);

	if (spawner == nullptr) {
		LOG("Failed to find spawner");
	}

	auto* model = Game::entityManager->GetEntity(id);

	if (model == nullptr) {
		LOG("Failed to find model entity");

		return;
	}

	Game::entityManager->DestructEntity(model);

	LOG("Deleting model LOT %i", model->GetLOT());

	if (model->GetLOT() == 14) {
		//add it to the inv
		std::vector<LDFBaseData*> settings;

		//fill our settings with BBB gurbage
		LDFBaseData* ldfBlueprintID = new LDFData<LWOOBJID>(u"blueprintid", model->GetVar<LWOOBJID>(u"blueprintid"));
		LDFBaseData* userModelDesc = new LDFData<std::u16string>(u"userModelDesc", u"A cool model you made!");
		LDFBaseData* userModelHasBhvr = new LDFData<bool>(u"userModelHasBhvr", false);
		LDFBaseData* userModelID = new LDFData<LWOOBJID>(u"userModelID", model->GetVar<LWOOBJID>(u"userModelID"));
		LDFBaseData* userModelMod = new LDFData<bool>(u"userModelMod", false);
		LDFBaseData* userModelName = new LDFData<std::u16string>(u"userModelName", u"My Cool Model");
		LDFBaseData* propertyObjectID = new LDFData<bool>(u"userModelOpt", true);
		LDFBaseData* modelType = new LDFData<int>(u"userModelPhysicsType", 2);

		settings.push_back(ldfBlueprintID);
		settings.push_back(userModelDesc);
		settings.push_back(userModelHasBhvr);
		settings.push_back(userModelID);
		settings.push_back(userModelMod);
		settings.push_back(userModelName);
		settings.push_back(propertyObjectID);
		settings.push_back(modelType);

		inventoryComponent->AddItem(6662, 1, eLootSourceType::DELETION, eInventoryType::MODELS_IN_BBB, settings, LWOOBJID_EMPTY, false, false, spawnerId);
		auto* item = inventoryComponent->FindItemBySubKey(spawnerId);

		if (item == nullptr) {
			return;
		}

		if (deleteReason == 0) {
			//item->Equip();
		}

		if (deleteReason == 0 || deleteReason == 2) {
			GameMessages::SendUGCEquipPostDeleteBasedOnEditMode(entity->GetObjectID(), entity->GetSystemAddress(), item->GetId(), item->GetCount());
		}

		GameMessages::SendGetModelsOnProperty(entity->GetObjectID(), GetModels(), UNASSIGNED_SYSTEM_ADDRESS);

		GameMessages::SendPlaceModelResponse(entity->GetObjectID(), entity->GetSystemAddress(), NiPoint3::ZERO, LWOOBJID_EMPTY, 16, NiQuaternion::IDENTITY);

		if (spawner != nullptr) {
			Game::zoneManager->RemoveSpawner(spawner->m_Info.spawnerID);
		} else {
			model->Smash(LWOOBJID_EMPTY, eKillType::SILENT);
		}

		item->SetCount(0, true, false, false);

		return;
	}

	inventoryComponent->AddItem(model->GetLOT(), 1, eLootSourceType::DELETION, INVALID, {}, LWOOBJID_EMPTY, false);

	auto* item = inventoryComponent->FindItemByLot(model->GetLOT());

	if (item == nullptr) {
		return;
	}

	switch (deleteReason) {
	case 0: // Pickup
	{
		item->Equip();

		GameMessages::SendUGCEquipPostDeleteBasedOnEditMode(entity->GetObjectID(), entity->GetSystemAddress(), item->GetId(), item->GetCount());
		Game::entityManager->GetZoneControlEntity()->OnZonePropertyModelPickedUp(entity);

		break;
	}
	case 1: // Return to inv
	{
		Game::entityManager->GetZoneControlEntity()->OnZonePropertyModelRemoved(entity);

		break;
	}
	case 2: // Break apart
	{
		item->SetCount(item->GetCount() - 1);

		LOG("BODGE TIME, YES IT GOES HERE");

		break;
	}
	default:
	{
		LOG("Invalid delete reason");
	}
	}

	GameMessages::SendGetModelsOnProperty(entity->GetObjectID(), GetModels(), UNASSIGNED_SYSTEM_ADDRESS);

	GameMessages::SendPlaceModelResponse(entity->GetObjectID(), entity->GetSystemAddress(), NiPoint3::ZERO, LWOOBJID_EMPTY, 16, NiQuaternion::IDENTITY);

	if (spawner != nullptr) {
		Game::zoneManager->RemoveSpawner(spawner->m_Info.spawnerID);
	} else {
		model->Smash(LWOOBJID_EMPTY, eKillType::SILENT);
	}
}

void PropertyManagementComponent::UpdateApprovedStatus(const bool value) {
	if (owner == LWOOBJID_EMPTY) return;

	IProperty::Info info;
	info.id = propertyId;
	info.modApproved = value;
	info.privacyOption = static_cast<uint32_t>(privacyOption);
	info.rejectionReason = "";

	Database::Get()->UpdatePropertyModerationInfo(info);
}

void PropertyManagementComponent::Load() {
	if (propertyId == LWOOBJID_EMPTY) {
		return;
	}

	auto propertyModels = Database::Get()->GetPropertyModels(propertyId);

	for (const auto& databaseModel : propertyModels) {
		auto* node = new SpawnerNode();

		node->position = databaseModel.position;
		node->rotation = databaseModel.rotation;

		SpawnerInfo info{};

		info.templateID = databaseModel.lot;
		info.nodes = { node };
		info.templateScale = 1.0f;
		info.activeOnLoad = true;
		info.amountMaintained = 1;
		info.respawnTime = 10;

		//info.emulated = true;
		//info.emulator = Game::entityManager->GetZoneControlEntity()->GetObjectID();

		info.spawnerID = databaseModel.id;

		std::vector<LDFBaseData*> settings;

		//BBB property models need to have extra stuff set for them:
		if (databaseModel.lot == 14) {
			LWOOBJID blueprintID = databaseModel.ugcId;
			GeneralUtils::SetBit(blueprintID, eObjectBits::CHARACTER);
			GeneralUtils::SetBit(blueprintID, eObjectBits::PERSISTENT);

			LDFBaseData* ldfBlueprintID = new LDFData<LWOOBJID>(u"blueprintid", blueprintID);
			LDFBaseData* componentWhitelist = new LDFData<int>(u"componentWhitelist", 1);
			LDFBaseData* modelType = new LDFData<int>(u"modelType", 2);
			LDFBaseData* propertyObjectID = new LDFData<bool>(u"propertyObjectID", true);
			LDFBaseData* userModelID = new LDFData<LWOOBJID>(u"userModelID", databaseModel.id);

			settings.push_back(ldfBlueprintID);
			settings.push_back(componentWhitelist);
			settings.push_back(modelType);
			settings.push_back(propertyObjectID);
			settings.push_back(userModelID);
		} else {
			auto modelType = new LDFData<int>(u"modelType", 2);
			auto userModelID = new LDFData<LWOOBJID>(u"userModelID", databaseModel.id);
			auto ldfModelBehavior = new LDFData<LWOOBJID>(u"modelBehaviors", 0);
			auto propertyObjectID = new LDFData<bool>(u"propertyObjectID", true);
			auto componentWhitelist = new LDFData<int>(u"componentWhitelist", 1);

			settings.push_back(componentWhitelist);
			settings.push_back(ldfModelBehavior);
			settings.push_back(modelType);
			settings.push_back(propertyObjectID);
			settings.push_back(userModelID);
		}

		node->config = settings;

		const auto spawnerId = Game::zoneManager->MakeSpawner(info);

		auto* spawner = Game::zoneManager->GetSpawner(spawnerId);

		auto* model = spawner->Spawn();

		models.insert_or_assign(model->GetObjectID(), spawnerId);
	}
}

void PropertyManagementComponent::Save() {
	if (propertyId == LWOOBJID_EMPTY) {
		return;
	}

	auto present = Database::Get()->GetPropertyModels(propertyId);

	std::vector<LWOOBJID> modelIds;

	for (const auto& pair : models) {
		const auto id = pair.second;

		modelIds.push_back(id);

		auto* entity = Game::entityManager->GetEntity(pair.first);

		if (entity == nullptr) {
			continue;
		}

		const auto position = entity->GetPosition();
		const auto rotation = entity->GetRotation();

		if (std::find(present.begin(), present.end(), id) == present.end()) {
			IPropertyContents::Model model;
			model.id = id;
			model.lot = entity->GetLOT();
			model.position = position;
			model.rotation = rotation;
			model.ugcId = 0;

			Database::Get()->InsertNewPropertyModel(propertyId, model, "Objects_" + std::to_string(model.lot) + "_name");
		} else {
			Database::Get()->UpdateModelPositionRotation(id, position, rotation);
		}
	}

	for (auto model : present) {
		if (std::find(modelIds.begin(), modelIds.end(), model.id) != modelIds.end()) {
			continue;
		}

		Database::Get()->RemoveModel(model.id);
	}
}

void PropertyManagementComponent::AddModel(LWOOBJID modelId, LWOOBJID spawnerId) {
	models[modelId] = spawnerId;
}

PropertyManagementComponent* PropertyManagementComponent::Instance() {
	return instance;
}

void PropertyManagementComponent::OnQueryPropertyData(Entity* originator, const SystemAddress& sysAddr, LWOOBJID author) {
	if (author == LWOOBJID_EMPTY) {
		author = m_Parent->GetObjectID();
	}

	const auto& worldId = Game::zoneManager->GetZone()->GetZoneID();
	const auto zoneId = worldId.GetMapID();
	const auto cloneId = worldId.GetCloneID();

	LOG("Getting property info for %d", zoneId);
	GameMessages::PropertyDataMessage message = GameMessages::PropertyDataMessage(zoneId);

	const auto isClaimed = GetOwnerId() != LWOOBJID_EMPTY;

	LWOOBJID ownerId = GetOwnerId();
	std::string ownerName;
	auto charInfo = Database::Get()->GetCharacterInfo(ownerId);
	if (charInfo) ownerName = charInfo->name;
	std::string name = "";
	std::string description = "";
	uint64_t claimed = 0;
	char privacy = 0;

	if (isClaimed) {
		name = propertyName;
		description = propertyDescription;
		claimed = claimedTime;
		privacy = static_cast<char>(this->privacyOption);
		if (moderatorRequested) {
			auto moderationInfo = Database::Get()->GetPropertyInfo(zoneId, cloneId);
			if (moderationInfo->rejectionReason != "") {
				moderatorRequested = false;
				rejectionReason = moderationInfo->rejectionReason;
			} else if (moderationInfo->rejectionReason == "" && moderationInfo->modApproved == 1) {
				moderatorRequested = false;
				rejectionReason = "";
			} else {
				moderatorRequested = true;
				rejectionReason = "";
			}
		}
	}
	message.moderatorRequested = moderatorRequested;
	message.reputation = reputation;
	message.LastUpdatedTime = LastUpdatedTime;
	message.OwnerId = ownerId;
	message.OwnerName = ownerName;
	message.Name = name;
	message.Description = description;
	message.ClaimedTime = claimed;
	message.PrivacyOption = privacy;
	message.cloneId = clone_Id;
	message.rejectionReason = rejectionReason;
	message.Paths = GetPaths();

	SendDownloadPropertyData(author, message, UNASSIGNED_SYSTEM_ADDRESS);
	// send rejection here?
}

void PropertyManagementComponent::OnUse(Entity* originator) {
	OnQueryPropertyData(originator, UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::SendOpenPropertyManagment(m_Parent->GetObjectID(), originator->GetSystemAddress());
}

void PropertyManagementComponent::SetOwnerId(const LWOOBJID value) {
	owner = value;
}

const std::map<LWOOBJID, LWOOBJID>& PropertyManagementComponent::GetModels() const {
	return models;
}
