#include "PropertyManagementComponent.h"
#include "PropertyRent.h"
#include "ChatPackets.h"
#include "DashboardNotify.h"

#include <sstream>

#include "MissionComponent.h"
#include "EntityManager.h"
#include "PropertyMessages.h"
#include "CDClientManager.h"
#include "CDPropertyTemplateTable.h"
#include "UserManager.h"
#include "GameMessages.h"
#include "Character.h"
#include "CDClientDatabase.h"
#include "dZoneManager.h"
#include "Game.h"
#include "Item.h"
#include "Database.h"
#include "ObjectIDManager.h"
#include "RocketLaunchpadControlComponent.h"
#include "PropertyEntranceComponent.h"
#include "InventoryComponent.h"
#include "eMissionTaskType.h"
#include "eObjectBits.h"
#include "CharacterComponent.h"
#include "PlayerManager.h"
#include "ModelComponent.h"

#include <vector>
#include "CppScripts.h"
#include <ranges>
#include "dConfig.h"
#include "BrickByBrick.h"
#include "eLootSourceType.h"
#include "eKillType.h"
#include "PropertyBuilders.h"
#include "BuildingMessages.h"

PropertyManagementComponent* PropertyManagementComponent::instance = nullptr;

PropertyManagementComponent::PropertyManagementComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
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

	if (result.eof() || result.fieldIsNull("id")) {
		return;
	}

	templateId = result.getIntField("id");

	auto propertyInfo = Database::Get()->GetPropertyInfo(zoneId, cloneId);

	if (propertyInfo) {
		this->propertyId = propertyInfo->id;
		this->owner = propertyInfo->ownerId;
		GeneralUtils::SetBit(this->owner, eObjectBits::CHARACTER);
		this->clone_Id = propertyInfo->cloneId;
		this->propertyName = propertyInfo->name;
		this->propertyDescription = propertyInfo->description;
		this->privacyOption = static_cast<PropertyPrivacyOption>(propertyInfo->privacyOption);
		this->rejectionReason = propertyInfo->rejectionReason;
		this->moderatorRequested = propertyInfo->modApproved == 0 && rejectionReason == "" && privacyOption == PropertyPrivacyOption::Public;
		this->LastUpdatedTime = propertyInfo->lastUpdatedTime;
		this->claimedTime = propertyInfo->claimedTime;
		this->reputation = propertyInfo->reputation;

		// Rent that went unpaid while the owner was away makes the property private
		if (privacyOption != PropertyPrivacyOption::Private && PropertyRent::IsOverdue(propertyId, zoneId)) {
			privacyOption = PropertyPrivacyOption::Private;
			Database::Get()->SetPropertyPrivacy(propertyId, static_cast<int32_t>(privacyOption));
			LOG("Property %llu is private: its rent is unpaid", propertyId);
		}

		Load();
	}

	// Turning property_bff_build on or off changes who can build right away
	static bool configHandlerAdded = false;
	if (!configHandlerAdded && Game::config) {
		Game::config->AddConfigHandler([]() { if (instance) instance->UpdateBuildRights(); });
		configHandlerAdded = true;
	}
}

PropertyManagementComponent::~PropertyManagementComponent() {
	if (instance == this) instance = nullptr;
}

bool PropertyManagementComponent::IsBestFriend(const LWOOBJID player) const {
	if (owner == LWOOBJID_EMPTY || player == owner) return false;
	const auto cached = bestFriends.find(player);
	if (cached != bestFriends.end()) return cached->second;
	const auto status = Database::Get()->GetBestFriendStatus(player, owner);
	const bool isBestFriend = status && status->bestFriendStatus == 3;
	bestFriends[player] = isBestFriend;
	return isBestFriend;
}

bool PropertyManagementComponent::CanBuild(const Entity& player) const {
	const bool bestFriendsBuild = PropertyBuilders::BestFriendsBuild();
	PropertyBuilders::Player state{ .id = player.GetObjectID(), .isBuilding = builders.contains(player.GetObjectID()) };
	// Only a best friend needs the lookup
	if (bestFriendsBuild && state.id != owner) state.isBestFriend = IsBestFriend(state.id);
	return PropertyBuilders::CanBuild(state, owner, builders.contains(owner), bestFriendsBuild);
}

void PropertyManagementComponent::OnPlayerLoaded(Entity& player) {
	IsBestFriend(player.GetObjectID());
}

void PropertyManagementComponent::OnPlayerRemoved(Entity& player) {
	const auto id = player.GetObjectID();
	const bool wasBuilding = EndBuilding(player);
	sentBuildRights.erase(id);
	bestFriends.erase(id);
	// The owner leaving build mode stops best friends who aren't building from joining
	if (wasBuilding) UpdateBuildRights();
}

void PropertyManagementComponent::UpdateBuildRights() {
	for (auto* player : PlayerManager::GetAllPlayers()) {
		if (!player) continue;
		const auto id = player->GetObjectID();
		const auto sent = sentBuildRights.find(id);
		// Never sent the property's data: the client has no rights to take away
		const bool couldBuild = sent != sentBuildRights.end() && sent->second;
		const bool canBuild = CanBuild(*player);
		if (couldBuild == canBuild) continue;

		if (!canBuild && builders.contains(id)) {
			// Out of build mode, as when they leave it themselves
			GameMessages::SetBuildModeConfirmed confirmed;
			confirmed.target = id;
			confirmed.start = false;
			confirmed.warnVisitors = true;
			confirmed.playerId = id;
			confirmed.startPos = player->GetPosition();
			confirmed.Send(player->GetSystemAddress());
			if (player->GetCharacter()) player->GetCharacter()->SetBuildMode(false);
			EndBuilding(*player);
			Game::zoneManager->GetZoneControlObject()->OnZonePropertyEditEnd();
			ChatPackets::SendSystemMessage(player->GetSystemAddress(), u"You can't build on this property any more.");
		}

		OnQueryPropertyData(player, player->GetSystemAddress());
	}
}

LWOOBJID PropertyManagementComponent::GetOwnerId() const {
	return owner;
}

Entity* PropertyManagementComponent::GetOwner() const {
	return Game::entityManager->GetEntity(owner);
}

void PropertyManagementComponent::SetOwner(Entity* value) {
	SetOwnerId(value->GetObjectID());
}

std::vector<NiPoint3> PropertyManagementComponent::GetPaths() const {
	const auto zoneId = Game::zoneManager->GetZone()->GetWorldID();

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT path FROM PropertyTemplate WHERE mapID = ?;");
	query.bind(1, static_cast<int>(zoneId));

	auto result = query.execQuery();

	std::vector<NiPoint3> paths{};

	if (result.eof()) {
		return paths;
	}

	std::vector<float> points;

	for (const auto& str : GeneralUtils::SplitString(result.getStringField("path"), ' ')) {
		const auto value = GeneralUtils::TryParse<float>(str);
		if (value) points.push_back(value.value());
	}

	for (auto i = 0u; i + 2 < points.size(); i += 3) {
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

	// Unpaid rent keeps the property private, like live
	if (value != PropertyPrivacyOption::Private && PropertyRent::IsOverdue(propertyId, Game::zoneManager->GetZoneID().GetMapID())) {
		value = PropertyPrivacyOption::Private;
		auto* ownerEntity = GetOwner();
		if (ownerEntity) ChatPackets::SendSystemMessage(ownerEntity->GetSystemAddress(), u"Your property's rent is unpaid, so it stays private until the rent is paid.");
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

	if (models.empty() && Game::config->GetValue("auto_reject_empty_properties") == "1") {
		UpdateApprovedStatus(false, "Your property is empty. Please place a model to have a public property.");
	} else {
		Database::Get()->UpdatePropertyModerationInfo(info);
		DashboardNotify::Changed("properties", propertyId);
	}
}

void PropertyManagementComponent::UpdatePropertyDetails(std::string name, std::string description) {
	if (owner == LWOOBJID_EMPTY) return;

	propertyName = name;

	propertyDescription = description;

	IProperty::Info info;
	info.id = propertyId;
	info.name = propertyName;
	info.description = propertyDescription;
	info.lastUpdatedTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	Database::Get()->UpdateLastSave(info);
	Database::Get()->UpdatePropertyDetails(info);
	DashboardNotify::Changed("properties", propertyId);

	OnQueryPropertyData(nullptr, UNASSIGNED_SYSTEM_ADDRESS);
}

bool PropertyManagementComponent::Claim(const LWOOBJID playerId) {
	if (owner != LWOOBJID_EMPTY) {
		return false;
	}

	auto* entity = Game::entityManager->GetEntity(playerId);

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

	if (prop_path) {
		if (!prop_path->property.displayName.empty()) name = prop_path->property.displayName;
		description = prop_path->property.displayDesc;
	}

	SetOwnerId(playerId);

	// Due to legacy IDs being random
	propertyId = ObjectIDManager::GetPersistentID();
	const uint32_t maxTries = 100;
	uint32_t tries = 0;
	while (Database::Get()->GetPropertyInfo(propertyId) && tries < maxTries) {
		tries++;
		LOG("Found a duplicate property %llu, getting a new propertyId", propertyId);
		propertyId = ObjectIDManager::GetPersistentID();
	}

	IProperty::Info info;
	info.id = propertyId;
	info.ownerId = character->GetID();
	info.cloneId = playerCloneId;
	info.name = name;
	info.description = description;

	Database::Get()->InsertNewProperty(info, templateId, worldId);
	DashboardNotify::Changed("properties", propertyId);

	auto* zoneControlObject = Game::zoneManager->GetZoneControlObject();
	if (zoneControlObject) zoneControlObject->GetScript()->OnZonePropertyRented(zoneControlObject, entity);
	return true;
}

void PropertyManagementComponent::OnStartBuilding(Entity& builder) {
	const bool first = builders.empty();
	if (!builders.insert(builder.GetObjectID()).second) return;

	// The owner starting lets their best friends join
	if (builder.GetObjectID() == owner) UpdateBuildRights();

	auto inventoryComponent = builder.GetComponent<InventoryComponent>();

	// Push equipped items
	if (inventoryComponent) inventoryComponent->PushEquippedItems();

	// Someone else is already building: the property is already private and paused
	if (!first) return;

	const auto players = PlayerManager::GetAllPlayers();

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

	// Everyone who can't build here leaves
	for (auto* player : players) {
		if (!player || CanBuild(*player)) continue;

		auto* characterComponent = player->GetComponent<CharacterComponent>();
		if (characterComponent) characterComponent->SendToZone(zoneId);
	}

	for (auto modelID : models | std::views::keys) {
		auto* model = Game::entityManager->GetEntity(modelID);
		if (model) {
			auto* modelComponent = model->GetComponent<ModelComponent>();
			if (modelComponent) modelComponent->Pause();
			Game::entityManager->SerializeEntity(model);
			GameMessages::ResetModelToDefaults reset;
			reset.target = modelID;
			model->HandleMsg(reset);
		}
	}

	for (auto* const entity : Game::entityManager->GetEntitiesInGroup("SpawnedPropertyEnemies")) {
		if (entity) entity->Smash();
	}
}

void PropertyManagementComponent::OnFinishBuilding(const Entity& builder) {
	// A best friend who left can't come back until the owner builds; the owner leaving stops the others joining
	if (EndBuilding(builder)) UpdateBuildRights();
}

bool PropertyManagementComponent::EndBuilding(const Entity& builder) {
	if (builders.erase(builder.GetObjectID()) == 0) return false;

	// Others are still building: keep the property private and paused
	if (!builders.empty()) {
		Save();
		return true;
	}

	UpdateApprovedStatus(false);

	SetPrivacyOption(originalPrivacyOption);

	Save();

	for (auto modelID : models | std::views::keys) {
		auto* model = Game::entityManager->GetEntity(modelID);
		if (model) {
			auto* modelComponent = model->GetComponent<ModelComponent>();
			if (modelComponent) modelComponent->Resume();
			Game::entityManager->SerializeEntity(model);
			GameMessages::ResetModelToDefaults reset;
			reset.target = modelID;
			model->HandleMsg(reset);
		}
	}

	for (auto* const entity : Game::entityManager->GetEntitiesInGroup("SpawnedPropertyEnemies")) {
		if (entity) entity->Smash();
	}
	return true;
}

Entity* PropertyManagementComponent::SpawnModel(const LOT lot, const LWOOBJID modelId, const NiPoint3& position, const NiQuaternion& rotation, const LwoNameValue& config, const LWOOBJID placer) {
	auto* node = new SpawnerNode();
	node->position = position;
	node->rotation = rotation;
	node->config = config;
	// Every placed model: a model object whose UGID is its property model id (GetModelsOnProperty pairs the two)
	node->config.Insert<LWOOBJID>(u"userModelID", modelId);
	node->config.Insert<int>(u"modelType", 2);
	node->config.Insert<bool>(u"propertyObjectID", true);
	node->config.Insert<int>(u"componentWhitelist", 1);
	if (lot != BrickByBrick::MODEL_OBJECT_LOT) node->config.Insert<LWOOBJID>(u"modelBehaviors", 0);

	SpawnerInfo info{};
	info.templateID = lot;
	info.nodes = { node };
	info.templateScale = 1.0f;
	info.activeOnLoad = true;
	info.amountMaintained = 1;
	info.respawnTime = 10;
	info.emulated = true;
	info.emulator = Game::entityManager->GetZoneControlEntity()->GetObjectID();
	// The spawner's id is the model's id in properties_contents
	info.spawnerID = modelId;

	const auto spawnerId = Game::zoneManager->MakeSpawner(info);
	auto* spawner = Game::zoneManager->GetSpawner(spawnerId);
	if (!spawner) return nullptr;

	auto* model = spawner->Spawn();
	if (!model) return nullptr;

	// Placed while someone is building: stays still until the last builder finishes (OnFinishBuilding resumes every
	// model)
	auto* modelComponent = model->GetComponent<ModelComponent>();
	if (modelComponent) modelComponent->Pause();

	models.insert_or_assign(model->GetObjectID(), spawnerId);
	placedBy.insert_or_assign(modelId, placer);
	return model;
}

LWOOBJID PropertyManagementComponent::PlaceModelFromItem(const Entity& builder, Item& item, const NiPoint3& position, const NiQuaternion& rotation) {
	LWOOBJID modelId = LWOOBJID_EMPTY;
	Entity* model = nullptr;

	if (item.GetLot() == BrickByBrick::MODEL_ITEM_LOT) {
		// A brick built model keeps its UGID (the item's subkey and userModelID) and blueprint wherever it goes
		const auto& config = item.GetConfig();
		modelId = item.GetSubKey();
		if (modelId == LWOOBJID_EMPTY) modelId = BrickByBrick::ConfigObjectId(config, u"userModelID");
		if (modelId == LWOOBJID_EMPTY) modelId = ObjectIDManager::GetNewModelIDs().modelID;
		const auto blueprintId = BrickByBrick::ConfigObjectId(config, u"blueprintid");
		if (blueprintId == LWOOBJID_EMPTY) {
			LOG("Model item %llu has no blueprint, not placing it", item.GetId());
			return LWOOBJID_EMPTY;
		}

		LwoNameValue modelConfig;
		modelConfig.Insert<LWOOBJID>(u"blueprintid", blueprintId);
		const auto behaviors = config.find(u"userModelBehaviors");
		if (behaviors != config.end() && behaviors->second) modelConfig.Insert<std::string>(u"userModelBehaviors", behaviors->second->GetValueAsString());
		model = SpawnModel(BrickByBrick::MODEL_OBJECT_LOT, modelId, position, rotation, modelConfig, builder.GetObjectID());
	} else {
		// A premade model gets a new UGID each time it is placed (live: the placed model's id, then a new item id
		// when it is picked up)
		modelId = ObjectIDManager::GetPersistentID();
		GeneralUtils::SetBit(modelId, eObjectBits::CLIENT);
		model = SpawnModel(item.GetLot(), modelId, position, rotation, {}, builder.GetObjectID());
	}

	if (!model) return LWOOBJID_EMPTY;

	item.SetCount(item.GetCount() - 1, false, false, false, eLootSourceType::PROPERTY);
	// Straight to the database: the model must not be lost if the server stops before the builder finishes editing
	Save();
	return modelId;
}

void PropertyManagementComponent::SendModelsOnProperty() const {
	GameMessages::GetModelsOnProperty msg;
	msg.models = { models.begin(), models.end() };
	// Each player's own character, as PropertyContentsFromClient answers: the owner may not be here to target
	for (const auto* player : PlayerManager::GetAllPlayers()) {
		if (!player) continue;
		msg.target = player->GetObjectID();
		msg.Send(player->GetSystemAddress());
	}
}

void PropertyManagementComponent::UpdateModelPosition(Entity& builder, const LWOOBJID id, const NiPoint3 position, NiQuaternion rotation) {
	LOG("Placing model <%f, %f, %f>", position.x, position.y, position.z);

	auto* entity = &builder;

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();
	if (inventoryComponent == nullptr) return;

	auto* item = inventoryComponent->FindItemById(id);
	if (item == nullptr) {
		LOG("Failed to find item with id %llu", id);
		return;
	}

	const NiQuaternion originalRotation = rotation;
	if (rotation != QuatUtils::IDENTITY) {
		rotation = { rotation.w, rotation.z, rotation.y, rotation.x };
	}

	const auto modelId = PlaceModelFromItem(builder, *item, position, rotation);
	if (modelId == LWOOBJID_EMPTY) return;

	// As a live server answered a placed model
	{
		GameMessages::HandleUGCEquipPreCreateBasedOnEditMode msg;
		msg.target = entity->GetObjectID();
		msg.modelCount = 0;
		msg.modelID = modelId;
		msg.Send(entity->GetSystemAddress());
	}

	{
		GameMessages::PlaceModelResponse msg;
		msg.target = entity->GetObjectID();
		msg.position = position;
		msg.propertyPlaqueID = m_Parent->GetObjectID();
		msg.response = BrickByBrick::PLACE_MODEL_PLACED;
		msg.rotation = originalRotation;
		msg.Send(entity->GetSystemAddress());
	}

	SendModelsOnProperty();

	Game::entityManager->GetZoneControlEntity()->OnZonePropertyModelPlaced(entity);

	// Progress place model missions
	auto missionComponent = entity->GetComponent<MissionComponent>();
	if (missionComponent != nullptr) missionComponent->Progress(eMissionTaskType::PLACE_MODEL, 0);
}

void PropertyManagementComponent::DeleteModel(Entity& builder, const LWOOBJID id, const int deleteReason) {
	LOG("Delete model: (%llu) (%i)", id, deleteReason);

	auto* model = Game::entityManager->GetEntity(id);
	if (model == nullptr) {
		LOG("Failed to find model entity");
		return;
	}

	const auto index = models.find(id);
	if (index == models.end()) {
		LOG("Failed to find model");
		return;
	}

	const auto modelId = index->second;

	// The model goes back to whoever placed it
	const auto placer = GetPlacedBy(modelId);
	auto* placerEntity = placer == builder.GetObjectID() ? &builder : PlayerManager::GetPlayer(placer);
	const auto placerName = [&]() -> std::string {
		if (placerEntity && placerEntity->GetCharacter()) return placerEntity->GetCharacter()->GetName();
		const auto info = Database::Get()->GetCharacterInfo(placer);
		return info ? info->name : "another player";
	};
	switch (PropertyBuilders::PlanModelReturn(builder.GetObjectID(), placer, placerEntity != nullptr, deleteReason)) {
	case PropertyBuilders::eModelReturn::PLACER_AWAY:
		LOG("%llu picked up model %llu placed by %llu, who isn't here; leaving it on the property", builder.GetObjectID(), modelId, placer);
		ChatPackets::SendSystemMessage(builder.GetSystemAddress(), "This model is " + placerName() + "'s. It stays on the property until they are here to get it back.");
		return;
	case PropertyBuilders::eModelReturn::NOT_THEIRS:
		ChatPackets::SendSystemMessage(builder.GetSystemAddress(), "This model is " + placerName() + "'s. Only they can take it apart.");
		return;
	default:
		break;
	}
	auto& receiver = *placerEntity;
	const bool toBuilder = &receiver == &builder;

	auto* inventoryComponent = receiver.GetComponent<InventoryComponent>();
	if (inventoryComponent == nullptr) return;

	const auto removal = BrickByBrick::PlanModelRemoval(deleteReason);

	// Every way off the property puts the model in MODELS; taking it apart then opens it in brick by brick building
	LOT itemLot = model->GetLOT();
	LwoNameValue config;
	LWOOBJID subKey = LWOOBJID_EMPTY;
	if (model->GetLOT() == BrickByBrick::MODEL_OBJECT_LOT) {
		itemLot = BrickByBrick::MODEL_ITEM_LOT;
		config = BrickByBrick::ModelItemConfig(model->GetVar<LWOOBJID>(u"blueprintid"), modelId, model->GetVar<std::string>(u"userModelBehaviors"));
		subKey = modelId;
	}

	// Only the player picking it up carries it
	const auto received = inventoryComponent->ReceiveItem(LWOOBJID_EMPTY, itemLot, 1, eLootSourceType::PROPERTY, config, subKey, false,
		{ .inventory = eInventoryType::MODELS, .showFlyingLoot = false, .equip = toBuilder && removal.equip });
	if (received.id == LWOOBJID_EMPTY) {
		LOG("Could not give model %llu back to %llu, leaving it on the property", modelId, receiver.GetObjectID());
		if (!toBuilder) ChatPackets::SendSystemMessage(builder.GetSystemAddress(), "This model is " + placerName() + "'s, and they have no room for it. It stays on the property.");
		return;
	}

	models.erase(index);
	placedBy.erase(modelId);
	Game::entityManager->DestructEntity(model);
	auto* spawner = Game::zoneManager->GetSpawner(modelId);
	if (spawner != nullptr) {
		Game::zoneManager->RemoveSpawner(spawner->m_Info.spawnerID);
	} else {
		model->Smash(LWOOBJID_EMPTY, eKillType::SILENT);
	}

	// Straight to the database, as for placing
	Save();

	if (removal.notifyPostDelete && toBuilder) {
		auto* item = inventoryComponent->FindItemById(received.id);
		GameMessages::HandleUGCEquipPostDeleteBasedOnEditMode msg;
		msg.target = builder.GetObjectID();
		msg.invItem = received.id;
		msg.itemsTotal = item ? item->GetCount() : 1;
		msg.Send(builder.GetSystemAddress());
	}

	if (!toBuilder) {
		const auto builderName = builder.GetCharacter() ? builder.GetCharacter()->GetName() : "Another player";
		ChatPackets::SendSystemMessage(builder.GetSystemAddress(), "This model is " + placerName() + "'s, so it went back to them.");
		ChatPackets::SendSystemMessage(receiver.GetSystemAddress(), builderName + " picked up one of your models. It is back in your models.");
	}

	SendModelsOnProperty();

	{
		GameMessages::PlaceModelResponse msg;
		msg.target = builder.GetObjectID();
		msg.response = BrickByBrick::PLACE_MODEL_REMOVED;
		msg.Send(builder.GetSystemAddress());
	}

	switch (static_cast<BrickByBrick::eDeleteReason>(deleteReason)) {
	case BrickByBrick::eDeleteReason::PICKING_MODEL_UP:
		Game::entityManager->GetZoneControlEntity()->OnZonePropertyModelPickedUp(&builder);
		break;
	case BrickByBrick::eDeleteReason::RETURNING_MODEL_TO_INVENTORY:
		Game::entityManager->GetZoneControlEntity()->OnZonePropertyModelRemoved(&builder);
		break;
	default:
		break;
	}
}

LWOOBJID PropertyManagementComponent::GetPlacedBy(const LWOOBJID modelId) const {
	const auto placer = placedBy.find(modelId);
	return PropertyBuilders::Placer(placer != placedBy.end() ? placer->second : LWOOBJID_EMPTY, owner);
}

void PropertyManagementComponent::UpdateApprovedStatus(const bool value, const std::string& rejectionReason) {
	if (owner == LWOOBJID_EMPTY) return;

	IProperty::Info info;
	info.id = propertyId;
	info.modApproved = value;
	info.privacyOption = static_cast<uint32_t>(privacyOption);
	info.rejectionReason = rejectionReason;

	Database::Get()->UpdatePropertyModerationInfo(info);
	DashboardNotify::Changed("properties", propertyId);
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

		LwoNameValue& settings = node->config;

		//BBB property models need to have extra stuff set for them:
		if (databaseModel.lot == 14) {
			LWOOBJID blueprintID = databaseModel.ugcId;

			settings.Insert<LWOOBJID>(u"blueprintid", blueprintID);
			settings.Insert<int>(u"componentWhitelist", 1);
			settings.Insert<int>(u"modelType", 2);
			settings.Insert<bool>(u"propertyObjectID", true);
			settings.Insert<LWOOBJID>(u"userModelID", databaseModel.id);
		} else {
			settings.Insert<int>(u"modelType", 2);
			settings.Insert<LWOOBJID>(u"userModelID", databaseModel.id);
			settings.Insert<LWOOBJID>(u"modelBehaviors", 0);
			settings.Insert<bool>(u"propertyObjectID", true);
			settings.Insert<int>(u"componentWhitelist", 1);
		}

		std::ostringstream userModelBehavior;
		bool firstAdded = false;
		for (auto behavior : databaseModel.behaviors) {
			if (behavior < 0) {
				LOG("Invalid behavior ID: %d, removing behavior reference from model", behavior);
				behavior = 0;
			}
			if (firstAdded) userModelBehavior << ",";
			userModelBehavior << behavior;
			firstAdded = true;
		}

		settings.Insert<std::string>(u"userModelBehaviors", userModelBehavior.str());

		const auto spawnerId = Game::zoneManager->MakeSpawner(info);

		auto* spawner = Game::zoneManager->GetSpawner(spawnerId);

		auto* model = spawner->Spawn();

		models.insert_or_assign(model->GetObjectID(), spawnerId);
		if (databaseModel.placedBy != LWOOBJID_EMPTY) placedBy.insert_or_assign(databaseModel.id, databaseModel.placedBy);
	}
}

void PropertyManagementComponent::Save() {
	// From the property's own ids: it saves whoever is here (the owner may not be)
	if (propertyId == LWOOBJID_EMPTY || owner == LWOOBJID_EMPTY) {
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
		auto* modelComponent = entity->GetComponent<ModelComponent>();
		if (!modelComponent) continue;
		const auto modelBehaviors = modelComponent->GetBehaviorsForSave();

		// save the behaviors of the model, as the model's placer's
		const auto placer = GetPlacedBy(id);
		for (const auto& [behaviorId, behaviorStr] : modelBehaviors) {
			if (behaviorStr.empty() || behaviorId == -1 || behaviorId == 0) continue;
			IBehaviors::Info info{
				.behaviorId = behaviorId,
				.characterId = placer,
				.behaviorInfo = behaviorStr
			};
			Database::Get()->AddBehavior(info);
		}

		// Always save the original position so we can move the model freely
		const auto& position = modelComponent->GetOriginalPosition();
		const auto& rotation = modelComponent->GetOriginalRotation();

		if (std::find(present.begin(), present.end(), id) == present.end()) {
			IPropertyContents::Model model;
			model.id = id;
			model.lot = entity->GetLOT();
			model.position = position;
			model.rotation = rotation;
			// A brick built model keeps its blueprint (it used to be saved as 0, losing the model on the next load)
			model.ugcId = model.lot == BrickByBrick::MODEL_OBJECT_LOT ? entity->GetVar<LWOOBJID>(u"blueprintid") : 0;
			for (auto i = 0; i < model.behaviors.size(); i++) {
				model.behaviors[i] = modelBehaviors[i].first;
			}
			model.placedBy = placer;

			Database::Get()->InsertNewPropertyModel(propertyId, model, "Objects_" + std::to_string(model.lot) + "_name");
		} else {
			Database::Get()->UpdateModel(id, position, rotation, modelBehaviors);
		}
	}

	for (auto model : present) {
		if (std::find(modelIds.begin(), modelIds.end(), model.id) != modelIds.end()) {
			continue;
		}

		Database::Get()->RemoveModel(model.id);
	}
	IProperty::Info info;
	info.id = propertyId;
	info.lastUpdatedTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	Database::Get()->UpdateLastSave(info);
	DashboardNotify::Changed("properties", propertyId);
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
	const auto propertyTemplate = CDClientManager::GetTable<CDPropertyTemplateTable>()->GetByMapID(zoneId);

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
			if (moderationInfo) {
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
	}
	GameMessages::DownloadPropertyData message;
	message.target = author;
	message.propertyId = 0;
	message.templateId = static_cast<int32_t>(propertyTemplate.id);
	message.mapId = static_cast<uint16_t>(propertyTemplate.mapID);
	message.vendorMapId = static_cast<uint16_t>(propertyTemplate.vendorMapID);
	message.cloneId = clone_Id;
	message.name = GeneralUtils::UTF8ToUTF16(name);
	message.description = GeneralUtils::UTF8ToUTF16(description);
	message.ownerName = GeneralUtils::UTF8ToUTF16(ownerName);
	message.ownerId = ownerId;
	message.propertyType = 0;
	message.zoneCode = 0;
	message.rent = 0;
	message.rentalPeriod = 1;
	message.expirationDate = LastUpdatedTime;
	message.rentAmount = 1;
	message.reputation = reputation;
	message.spawnName = GeneralUtils::ASCIIToUTF16(propertyTemplate.spawnName);
	message.rentDuration = 0;
	message.votes = 1;
	message.durationType = 1;
	message.renew = static_cast<uint8_t>(privacy);
	message.ownerAccountID = 0;
	if (rejectionReason != "") message.moderationStatus = GameMessages::DownloadPropertyData::REJECTION_STATUS_REJECTED;
	else if (moderatorRequested == true && rejectionReason == "") message.moderationStatus = GameMessages::DownloadPropertyData::REJECTION_STATUS_APPROVED;
	else message.moderationStatus = GameMessages::DownloadPropertyData::REJECTION_STATUS_PENDING;
	message.lastLogoutTime = 0;
	message.dayOfMonthPlaqueWasBought = 1;
	message.repAchievementReq = 1;
	message.zonePosition = { 548.0f, 406.0f, 178.0f };
	message.maxBuildHeight = 128.0f;
	message.rentalDate = claimed;
	message.accessType = static_cast<uint8_t>(privacy);
	message.pathPositions = GetPaths();

	LOG("(%llu) sending property data (%d)", author, true);
	// Each player gets their own: it tells builders they own the property
	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) {
		for (auto* player : PlayerManager::GetAllPlayers()) {
			if (player) SendPropertyData(*player, message);
		}
	} else {
		auto* player = PlayerManager::GetPlayer(sysAddr);
		if (player) SendPropertyData(*player, message);
	}
	// send rejection here?
}

void PropertyManagementComponent::SendPropertyData(const Entity& player, const GameMessages::DownloadPropertyData& message) {
	const bool canBuild = CanBuild(player);
	sentBuildRights[player.GetObjectID()] = canBuild;

	// The client lets only the property's owner build (ownerId == its character); a best friend who can build is told
	// they own it, while ownerName stays the owner's
	auto forPlayer = message;
	if (canBuild) forPlayer.ownerId = player.GetObjectID();
	forPlayer.Send(player.GetSystemAddress());
}

void PropertyManagementComponent::OnUse(Entity* originator) {
	OnQueryPropertyData(originator, UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::OpenPropertyManagement msg;
	msg.target = PropertyManagementComponent::Instance()->GetParent()->GetObjectID();
	msg.Send(originator->GetSystemAddress());
}

void PropertyManagementComponent::SetOwnerId(const LWOOBJID value) {
	if (owner != value) bestFriends.clear();
	owner = value;
}

const std::map<LWOOBJID, LWOOBJID>& PropertyManagementComponent::GetModels() const {
	return models;
}

void PropertyManagementComponent::OnChatMessageReceived(const std::string& sMessage) const {
	for (const auto& modelID : models | std::views::keys) {
		auto* const model = Game::entityManager->GetEntity(modelID);
		if (!model) continue;
		auto* const modelComponent = model->GetComponent<ModelComponent>();
		if (!modelComponent) continue;

		modelComponent->OnChatMessageReceived(sMessage);
	}
}

void PropertyManagementComponent::ApplyModeration(const bool approved, const std::string& reason) {
	rejectionReason = approved ? "" : reason;
	// The dashboard makes rejected properties private; don't let a later save here publish it again
	if (!approved) privacyOption = PropertyPrivacyOption::Private;
	OnQueryPropertyData(nullptr, UNASSIGNED_SYSTEM_ADDRESS);
}
