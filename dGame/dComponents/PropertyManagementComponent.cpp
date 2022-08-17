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

	const auto& worldId = dZoneManager::Instance()->GetZone()->GetZoneID();

	const auto zoneId = worldId.GetMapID();
	const auto cloneId = worldId.GetCloneID();

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT id FROM PropertyTemplate WHERE mapID = ?;");
	query.bind(1, (int)zoneId);

	auto result = query.execQuery();

	if (result.eof() || result.fieldIsNull(0)) {
		return;
	}

	templateId = result.getIntField(0);

	result.finalize();

	auto* propertyLookup = Database::CreatePreppedStmt("SELECT * FROM properties WHERE template_id = ? AND clone_id = ?;");

	propertyLookup->setInt(1, templateId);
	propertyLookup->setInt64(2, cloneId);

	auto* propertyEntry = propertyLookup->executeQuery();

	if (propertyEntry->next()) {
		this->propertyId = propertyEntry->getUInt64(1);
		this->owner = propertyEntry->getUInt64(2);
		this->owner = GeneralUtils::SetBit(this->owner, OBJECT_BIT_CHARACTER);
		this->owner = GeneralUtils::SetBit(this->owner, OBJECT_BIT_PERSISTENT);
		this->clone_Id = propertyEntry->getInt(2);
		this->propertyName = propertyEntry->getString(5).c_str();
		this->propertyDescription = propertyEntry->getString(6).c_str();
		this->privacyOption = static_cast<PropertyPrivacyOption>(propertyEntry->getUInt(9));
		this->moderatorRequested = propertyEntry->getInt(10) == 0 && rejectionReason == "" && privacyOption == PropertyPrivacyOption::Public;
		this->LastUpdatedTime = propertyEntry->getUInt64(11);
		this->claimedTime = propertyEntry->getUInt64(12);
		this->rejectionReason = std::string(propertyEntry->getString(13).c_str());
		this->reputation = propertyEntry->getUInt(14);

		Load();
	}

	delete propertyLookup;
}

LWOOBJID PropertyManagementComponent::GetOwnerId() const {
	return owner;
}

Entity* PropertyManagementComponent::GetOwner() const {
	return EntityManager::Instance()->GetEntity(owner);
}

void PropertyManagementComponent::SetOwner(Entity* value) {
	owner = value->GetObjectID();
}

std::vector<NiPoint3> PropertyManagementComponent::GetPaths() const {
	const auto zoneId = dZoneManager::Instance()->GetZone()->GetWorldID();

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
			Game::logger->Log("PropertyManagementComponent", "Failed to parse value (%s): (%s)!", token.c_str(), exception.what());
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

	auto* propertyUpdate = Database::CreatePreppedStmt("UPDATE properties SET privacy_option = ?, rejection_reason = ?, mod_approved = ? WHERE id = ?;");

	propertyUpdate->setInt(1, static_cast<int32_t>(value));
	propertyUpdate->setString(2, "");
	propertyUpdate->setInt(3, 0);
	propertyUpdate->setInt64(4, propertyId);

	propertyUpdate->executeUpdate();
}

void PropertyManagementComponent::UpdatePropertyDetails(std::string name, std::string description) {
	if (owner == LWOOBJID_EMPTY) return;

	propertyName = name;

	propertyDescription = description;

	auto* propertyUpdate = Database::CreatePreppedStmt("UPDATE properties SET name = ?, description = ? WHERE id = ?;");

	propertyUpdate->setString(1, name.c_str());
	propertyUpdate->setString(2, description.c_str());
	propertyUpdate->setInt64(3, propertyId);

	propertyUpdate->executeUpdate();

	OnQueryPropertyData(GetOwner(), UNASSIGNED_SYSTEM_ADDRESS);
}

bool PropertyManagementComponent::Claim(const LWOOBJID playerId) {
	if (owner != LWOOBJID_EMPTY) {
		return false;
	}

	auto* entity = EntityManager::Instance()->GetEntity(playerId);

	auto* user = entity->GetParentUser();

	auto character = entity->GetCharacter();
	if (!character) return false;

	auto* zone = dZoneManager::Instance()->GetZone();

	const auto& worldId = zone->GetZoneID();
	const auto propertyZoneId = worldId.GetMapID();
	const auto propertyCloneId = worldId.GetCloneID();

	const auto playerCloneId = character->GetPropertyCloneID();

	// If we are not on our clone do not allow us to claim the property
	if (propertyCloneId != playerCloneId) return false;

	SetOwnerId(playerId);

	propertyId = ObjectIDManager::GenerateRandomObjectID();

	auto* insertion = Database::CreatePreppedStmt(
		"INSERT INTO properties"
		"(id, owner_id, template_id, clone_id, name, description, rent_amount, rent_due, privacy_option, last_updated, time_claimed, rejection_reason, reputation, zone_id, performance_cost)"
		"VALUES (?, ?, ?, ?, ?, '', 0, 0, 0, UNIX_TIMESTAMP(), UNIX_TIMESTAMP(), '', 0, ?, 0.0)"
	);
	insertion->setUInt64(1, propertyId);
	insertion->setUInt64(2, (uint32_t)playerId);
	insertion->setUInt(3, templateId);
	insertion->setUInt64(4, playerCloneId);
	insertion->setString(5, zone->GetZoneName().c_str());
	insertion->setInt(6, propertyZoneId);

	// Try and execute the query, print an error if it fails.
	try {
		insertion->execute();
	} catch (sql::SQLException& exception) {
		Game::logger->Log("PropertyManagementComponent", "Failed to execute query: (%s)!", exception.what());

		throw exception;
		return false;
	}

	auto* zoneControlObject = dZoneManager::Instance()->GetZoneControlObject();
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

	const auto entrance = EntityManager::Instance()->GetEntitiesByComponent(COMPONENT_TYPE_PROPERTY_ENTRANCE);

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
	Game::logger->Log("PropertyManagementComponent", "Placing model <%f, %f, %f>", position.x, position.y, position.z);

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
		Game::logger->Log("PropertyManagementComponent", "Failed to find item with id %d", id);

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

		Entity* newEntity = EntityManager::Instance()->CreateEntity(info);
		if (newEntity != nullptr) {
			EntityManager::Instance()->ConstructEntity(newEntity);

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
		info.emulator = EntityManager::Instance()->GetZoneControlEntity()->GetObjectID();

		LWOOBJID id = static_cast<LWOOBJID>(persistentId) | 1ull << OBJECT_BIT_CLIENT;

		info.spawnerID = id;

		const auto spawnerId = dZoneManager::Instance()->MakeSpawner(info);

		auto* spawner = dZoneManager::Instance()->GetSpawner(spawnerId);

		auto ldfModelBehavior = new LDFData<LWOOBJID>(u"modelBehaviors", 0);
		auto userModelID = new LDFData<LWOOBJID>(u"userModelID", id);
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

		EntityManager::Instance()->GetZoneControlEntity()->OnZonePropertyModelPlaced(entity);
		});
	// Progress place model missions
	auto missionComponent = entity->GetComponent<MissionComponent>();
	if (missionComponent != nullptr) missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_PLACE_MODEL, 0);
}

void PropertyManagementComponent::DeleteModel(const LWOOBJID id, const int deleteReason) {
	Game::logger->Log("PropertyManagementComponent", "Delete model: (%llu) (%i)", id, deleteReason);

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
		Game::logger->Log("PropertyManagementComponent", "Failed to find model");

		return;
	}

	const auto spawnerId = index->second;

	auto* spawner = dZoneManager::Instance()->GetSpawner(spawnerId);

	models.erase(id);

	if (spawner == nullptr) {
		Game::logger->Log("PropertyManagementComponent", "Failed to find spawner");
	}

	auto* model = EntityManager::Instance()->GetEntity(id);

	if (model == nullptr) {
		Game::logger->Log("PropertyManagementComponent", "Failed to find model entity");

		return;
	}

	EntityManager::Instance()->DestructEntity(model);

	Game::logger->Log("PropertyManagementComponent", "Deleting model LOT %i", model->GetLOT());

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

		inventoryComponent->AddItem(6662, 1, eLootSourceType::LOOT_SOURCE_DELETION, eInventoryType::HIDDEN, settings, LWOOBJID_EMPTY, false, false, spawnerId);
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
			dZoneManager::Instance()->RemoveSpawner(spawner->m_Info.spawnerID);
		} else {
			model->Smash(SILENT);
		}

		item->SetCount(0, true, false, false);

		return;
	}

	inventoryComponent->AddItem(model->GetLOT(), 1, eLootSourceType::LOOT_SOURCE_DELETION, INVALID, {}, LWOOBJID_EMPTY, false);

	auto* item = inventoryComponent->FindItemByLot(model->GetLOT());

	if (item == nullptr) {
		return;
	}

	switch (deleteReason) {
	case 0: // Pickup
	{
		item->Equip();

		GameMessages::SendUGCEquipPostDeleteBasedOnEditMode(entity->GetObjectID(), entity->GetSystemAddress(), item->GetId(), item->GetCount());
		EntityManager::Instance()->GetZoneControlEntity()->OnZonePropertyModelPickedUp(entity);

		break;
	}
	case 1: // Return to inv
	{
		EntityManager::Instance()->GetZoneControlEntity()->OnZonePropertyModelRemoved(entity);

		break;
	}
	case 2: // Break apart
	{
		item->SetCount(item->GetCount() - 1);

		Game::logger->Log("BODGE TIME", "YES IT GOES HERE");

		break;
	}
	default:
	{
		Game::logger->Log("PropertyManagementComponent", "Invalid delete reason");
	}
	}

	GameMessages::SendGetModelsOnProperty(entity->GetObjectID(), GetModels(), UNASSIGNED_SYSTEM_ADDRESS);

	GameMessages::SendPlaceModelResponse(entity->GetObjectID(), entity->GetSystemAddress(), NiPoint3::ZERO, LWOOBJID_EMPTY, 16, NiQuaternion::IDENTITY);

	if (spawner != nullptr) {
		dZoneManager::Instance()->RemoveSpawner(spawner->m_Info.spawnerID);
	} else {
		model->Smash(SILENT);
	}
}

void PropertyManagementComponent::UpdateApprovedStatus(const bool value) {
	if (owner == LWOOBJID_EMPTY) return;

	auto* update = Database::CreatePreppedStmt("UPDATE properties SET mod_approved = ? WHERE id = ?;");

	update->setBoolean(1, value);
	update->setInt64(2, propertyId);

	update->executeUpdate();

	delete update;
}

void PropertyManagementComponent::Load() {
	if (propertyId == LWOOBJID_EMPTY) {
		return;
	}

	auto* lookup = Database::CreatePreppedStmt("SELECT id, lot, x, y, z, rx, ry, rz, rw, ugc_id FROM properties_contents WHERE property_id = ?;");

	lookup->setUInt64(1, propertyId);

	auto* lookupResult = lookup->executeQuery();

	while (lookupResult->next()) {
		const LWOOBJID id = lookupResult->getUInt64(1);
		const LOT lot = lookupResult->getInt(2);

		const NiPoint3 position =
		{
			static_cast<float>(lookupResult->getDouble(3)),
			static_cast<float>(lookupResult->getDouble(4)),
			static_cast<float>(lookupResult->getDouble(5))
		};

		const NiQuaternion rotation =
		{
			static_cast<float>(lookupResult->getDouble(9)),
			static_cast<float>(lookupResult->getDouble(6)),
			static_cast<float>(lookupResult->getDouble(7)),
			static_cast<float>(lookupResult->getDouble(8))
		};

		auto* node = new SpawnerNode();

		node->position = position;
		node->rotation = rotation;

		SpawnerInfo info{};

		info.templateID = lot;
		info.nodes = { node };
		info.templateScale = 1.0f;
		info.activeOnLoad = true;
		info.amountMaintained = 1;
		info.respawnTime = 10;

		//info.emulated = true;
		//info.emulator = EntityManager::Instance()->GetZoneControlEntity()->GetObjectID();

		info.spawnerID = id;

		std::vector<LDFBaseData*> settings;

		//BBB property models need to have extra stuff set for them:
		if (lot == 14) {
			LWOOBJID blueprintID = lookupResult->getUInt(10);
			blueprintID = GeneralUtils::SetBit(blueprintID, OBJECT_BIT_CHARACTER);
			blueprintID = GeneralUtils::SetBit(blueprintID, OBJECT_BIT_PERSISTENT);

			LDFBaseData* ldfBlueprintID = new LDFData<LWOOBJID>(u"blueprintid", blueprintID);
			LDFBaseData* componentWhitelist = new LDFData<int>(u"componentWhitelist", 1);
			LDFBaseData* modelType = new LDFData<int>(u"modelType", 2);
			LDFBaseData* propertyObjectID = new LDFData<bool>(u"propertyObjectID", true);
			LDFBaseData* userModelID = new LDFData<LWOOBJID>(u"userModelID", id);

			settings.push_back(ldfBlueprintID);
			settings.push_back(componentWhitelist);
			settings.push_back(modelType);
			settings.push_back(propertyObjectID);
			settings.push_back(userModelID);
		} else {
			auto modelType = new LDFData<int>(u"modelType", 2);
			auto userModelID = new LDFData<LWOOBJID>(u"userModelID", id);
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

		const auto spawnerId = dZoneManager::Instance()->MakeSpawner(info);

		auto* spawner = dZoneManager::Instance()->GetSpawner(spawnerId);

		auto* model = spawner->Spawn();

		models.insert_or_assign(model->GetObjectID(), spawnerId);
	}

	delete lookup;
}

void PropertyManagementComponent::Save() {
	if (propertyId == LWOOBJID_EMPTY) {
		return;
	}

	auto* insertion = Database::CreatePreppedStmt("INSERT INTO properties_contents VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
	auto* update = Database::CreatePreppedStmt("UPDATE properties_contents SET x = ?, y = ?, z = ?, rx = ?, ry = ?, rz = ?, rw = ? WHERE id = ?;");
	auto* lookup = Database::CreatePreppedStmt("SELECT id FROM properties_contents WHERE property_id = ?;");
	auto* remove = Database::CreatePreppedStmt("DELETE FROM properties_contents WHERE id = ?;");

	lookup->setUInt64(1, propertyId);
	sql::ResultSet* lookupResult = nullptr;
	try {
		lookupResult = lookup->executeQuery();
	} catch (sql::SQLException& ex) {
		Game::logger->Log("PropertyManagementComponent", "lookup error %s", ex.what());
	}
	std::vector<LWOOBJID> present;

	while (lookupResult->next()) {
		const auto dbId = lookupResult->getUInt64(1);

		present.push_back(dbId);
	}

	delete lookupResult;

	std::vector<LWOOBJID> modelIds;

	for (const auto& pair : models) {
		const auto id = pair.second;

		modelIds.push_back(id);

		auto* entity = EntityManager::Instance()->GetEntity(pair.first);

		if (entity == nullptr) {
			continue;
		}

		const auto position = entity->GetPosition();
		const auto rotation = entity->GetRotation();

		if (std::find(present.begin(), present.end(), id) == present.end()) {
			insertion->setInt64(1, id);
			insertion->setUInt64(2, propertyId);
			insertion->setNull(3, 0);
			insertion->setInt(4, entity->GetLOT());
			insertion->setDouble(5, position.x);
			insertion->setDouble(6, position.y);
			insertion->setDouble(7, position.z);
			insertion->setDouble(8, rotation.x);
			insertion->setDouble(9, rotation.y);
			insertion->setDouble(10, rotation.z);
			insertion->setDouble(11, rotation.w);
			try {
				insertion->execute();
			} catch (sql::SQLException& ex) {
				Game::logger->Log("PropertyManagementComponent", "Error inserting into properties_contents. Error %s", ex.what());
			}
		} else {
			update->setDouble(1, position.x);
			update->setDouble(2, position.y);
			update->setDouble(3, position.z);
			update->setDouble(4, rotation.x);
			update->setDouble(5, rotation.y);
			update->setDouble(6, rotation.z);
			update->setDouble(7, rotation.w);

			update->setInt64(8, id);
			try {
				update->executeUpdate();
			} catch (sql::SQLException& ex) {
				Game::logger->Log("PropertyManagementComponent", "Error updating properties_contents. Error: %s", ex.what());
			}
		}
	}

	for (auto id : present) {
		if (std::find(modelIds.begin(), modelIds.end(), id) != modelIds.end()) {
			continue;
		}

		remove->setInt64(1, id);
		try {
			remove->execute();
		} catch (sql::SQLException& ex) {
			Game::logger->Log("PropertyManagementComponent", "Error removing from properties_contents. Error %s", ex.what());
		}
	}

	auto* removeUGC = Database::CreatePreppedStmt("DELETE FROM ugc WHERE id NOT IN (SELECT ugc_id FROM properties_contents);");

	removeUGC->execute();

	delete removeUGC;
	delete insertion;
	delete update;
	delete lookup;
	delete remove;
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

	const auto& worldId = dZoneManager::Instance()->GetZone()->GetZoneID();
	const auto zoneId = worldId.GetMapID();

	Game::logger->Log("Properties", "Getting property info for %d", zoneId);
	GameMessages::PropertyDataMessage message = GameMessages::PropertyDataMessage(zoneId);

	const auto isClaimed = GetOwnerId() != LWOOBJID_EMPTY;

	LWOOBJID ownerId = GetOwnerId();
	std::string ownerName = "";
	std::string name = "";
	std::string description = "";
	uint64_t claimed = 0;
	char privacy = 0;

	if (isClaimed) {
		const auto cloneId = worldId.GetCloneID();

		auto* nameLookup = Database::CreatePreppedStmt("SELECT name FROM charinfo WHERE prop_clone_id = ?;");
		nameLookup->setUInt64(1, cloneId);

		auto* nameResult = nameLookup->executeQuery();
		if (nameResult->next()) {
			ownerName = nameResult->getString(1).c_str();
		}

		delete nameResult;
		delete nameLookup;

		name = propertyName;
		description = propertyDescription;
		claimed = claimedTime;
		privacy = static_cast<char>(this->privacyOption);
		if (moderatorRequested) {
			auto checkStatus = Database::CreatePreppedStmt("SELECT rejection_reason, mod_approved FROM properties WHERE id = ?;");

			checkStatus->setInt64(1, propertyId);

			auto result = checkStatus->executeQuery();

			result->next();

			const auto reason = std::string(result->getString(1).c_str());
			const auto modApproved = result->getInt(2);
			if (reason != "") {
				moderatorRequested = false;
				rejectionReason = reason;
			} else if (reason == "" && modApproved == 1) {
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
