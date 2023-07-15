#include "GameDependencies.h"

#include <gtest/gtest.h>

#include "BaseCombatAIComponent.h"
#include "BouncerComponent.h"
#include "BuffComponent.h"
#include "BuildBorderComponent.h"
#include "CharacterComponent.h"
#include "Component.h"
#include "ControllablePhysicsComponent.h"
#include "DestroyableComponent.h"
#include "InventoryComponent.h"
#include "LevelProgressionComponent.h"
#include "LUPExhibitComponent.h"
#include "MissionComponent.h"
#include "MissionOfferComponent.h"
#include "ModelComponent.h"
#include "ModuleAssemblyComponent.h"
#include "MovementAIComponent.h"
#include "MovingPlatformComponent.h"
#include "PetComponent.h"
#include "PhantomPhysicsComponent.h"
#include "PlayerForcedMovementComponent.h"
#include "PossessableComponent.h"
#include "PossessorComponent.h"
#include "PropertyComponent.h"
#include "PropertyEntranceComponent.h"
#include "PropertyManagementComponent.h"
#include "PropertyVendorComponent.h"
#include "ProximityMonitorComponent.h"
#include "RacingControlComponent.h"
#include "RailActivatorComponent.h"
#include "RebuildComponent.h"
#include "RenderComponent.h"
#include "RigidbodyPhantomPhysicsComponent.h"
#include "RocketLaunchLupComponent.h"
#include "RocketLaunchpadControlComponent.h"
#include "ScriptedActivityComponent.h"
#include "ShootingGalleryComponent.h"
#include "SimplePhysicsComponent.h"
#include "SkillComponent.h"
#include "SoundTriggerComponent.h"
#include "SwitchComponent.h"
#include "TriggerComponent.h"
#include "VehiclePhysicsComponent.h"
#include "VendorComponent.h"

#include "Entity.h"
#include "eReplicaPacketType.h"

class EntityTests : public GameDependenciesTest {
protected:
	struct {
		BaseCombatAIComponent* baseCombatAIComponent;
		int32_t combatAiComponentTarget = 0;
	};
	BouncerComponent* bouncerComponent;
	BuffComponent* buffComponent;
	BuildBorderComponent* buildBorderComponent;
	CharacterComponent* characterComponent;
	ControllablePhysicsComponent* controllablePhysicsComponent;
	DestroyableComponent* destroyableComponent;
	InventoryComponent* inventoryComponent;
	LevelProgressionComponent* levelProgressionComponent;
	LUPExhibitComponent* lUPExhibitComponent;
	MissionComponent* missionComponent;
	MissionOfferComponent* missionOfferComponent;
	ModelComponent* modelComponent;
	ModuleAssemblyComponent* moduleAssemblyComponent;
	MovementAIComponent* movementAIComponent;
	MovingPlatformComponent* movingPlatformComponent;
	PetComponent* petComponent;
	PhantomPhysicsComponent* phantomPhysicsComponent;
	PlayerForcedMovementComponent* playerForcedMovementComponent;
	PossessableComponent* possessableComponent;
	PossessorComponent* possessorComponent;
	PropertyComponent* propertyComponent;
	PropertyEntranceComponent* propertyEntranceComponent;
	PropertyManagementComponent* propertyManagementComponent;
	PropertyVendorComponent* propertyVendorComponent;
	ProximityMonitorComponent* proximityMonitorComponent;
	RacingControlComponent* racingControlComponent;
	RailActivatorComponent* railActivatorComponent;
	RebuildComponent* rebuildComponent;
	RenderComponent* renderComponent;
	RigidbodyPhantomPhysicsComponent* rigidbodyPhantomPhysicsComponent;
	RocketLaunchLupComponent* rocketLaunchLupComponent;
	RocketLaunchpadControlComponent* rocketLaunchpadControlComponent;
	ScriptedActivityComponent* scriptedActivityComponent;
	ShootingGalleryComponent* shootingGalleryComponent;
	SimplePhysicsComponent* simplePhysicsComponent;
	SkillComponent* skillComponent;
	SoundTriggerComponent* soundTriggerComponent;
	SwitchComponent* switchComponent;
	TriggerComponent* triggerComponent;
	VehiclePhysicsComponent* vehiclePhysicsComponent;
	VendorComponent* vendorComponent;
	Entity* testedEntity;
	void SetUp() override {
		srand(time(NULL));
		SetUpDependencies();
		testedEntity = new Entity(5, info);
		// We allocate through malloc because we cannot call the constructors of some of thse methods.
		baseCombatAIComponent = (BaseCombatAIComponent*)malloc(sizeof(BaseCombatAIComponent));
		baseCombatAIComponent->SetState(AiState::dead);
		combatAiComponentTarget = rand();
		baseCombatAIComponent->SetTarget(combatAiComponentTarget);
		testedEntity->AddComponent(BaseCombatAIComponent::ComponentType, baseCombatAIComponent);

		bouncerComponent = (BouncerComponent*)malloc(sizeof(BouncerComponent));
		// bouncerComponent->SetPetEnabled(true);

		testedEntity->AddComponent(BouncerComponent::ComponentType, bouncerComponent);
		buffComponent = (BuffComponent*)malloc(sizeof(BuffComponent));
		testedEntity->AddComponent(BuffComponent::ComponentType, buffComponent);
		buildBorderComponent = (BuildBorderComponent*)malloc(sizeof(BuildBorderComponent));
		testedEntity->AddComponent(BuildBorderComponent::ComponentType, buildBorderComponent);
		characterComponent = (CharacterComponent*)malloc(sizeof(CharacterComponent));
		testedEntity->AddComponent(CharacterComponent::ComponentType, characterComponent);
		controllablePhysicsComponent = (ControllablePhysicsComponent*)malloc(sizeof(ControllablePhysicsComponent));
		testedEntity->AddComponent(ControllablePhysicsComponent::ComponentType, controllablePhysicsComponent);
		destroyableComponent = (DestroyableComponent*)malloc(sizeof(DestroyableComponent));
		testedEntity->AddComponent(DestroyableComponent::ComponentType, destroyableComponent);
		inventoryComponent = (InventoryComponent*)malloc(sizeof(InventoryComponent));
		testedEntity->AddComponent(InventoryComponent::ComponentType, inventoryComponent);
		levelProgressionComponent = (LevelProgressionComponent*)malloc(sizeof(LevelProgressionComponent));
		testedEntity->AddComponent(LevelProgressionComponent::ComponentType, levelProgressionComponent);
		lUPExhibitComponent = (LUPExhibitComponent*)malloc(sizeof(LUPExhibitComponent));
		testedEntity->AddComponent(LUPExhibitComponent::ComponentType, lUPExhibitComponent);
		missionComponent = (MissionComponent*)malloc(sizeof(MissionComponent));
		testedEntity->AddComponent(MissionComponent::ComponentType, missionComponent);
		missionOfferComponent = (MissionOfferComponent*)malloc(sizeof(MissionOfferComponent));
		testedEntity->AddComponent(MissionOfferComponent::ComponentType, missionOfferComponent);
		modelComponent = (ModelComponent*)malloc(sizeof(ModelComponent));
		testedEntity->AddComponent(ModelComponent::ComponentType, modelComponent);
		moduleAssemblyComponent = (ModuleAssemblyComponent*)malloc(sizeof(ModuleAssemblyComponent));
		testedEntity->AddComponent(ModuleAssemblyComponent::ComponentType, moduleAssemblyComponent);
		movementAIComponent = (MovementAIComponent*)malloc(sizeof(MovementAIComponent));
		testedEntity->AddComponent(MovementAIComponent::ComponentType, movementAIComponent);
		movingPlatformComponent = (MovingPlatformComponent*)malloc(sizeof(MovingPlatformComponent));
		testedEntity->AddComponent(MovingPlatformComponent::ComponentType, movingPlatformComponent);
		petComponent = (PetComponent*)malloc(sizeof(PetComponent));
		testedEntity->AddComponent(PetComponent::ComponentType, petComponent);
		phantomPhysicsComponent = (PhantomPhysicsComponent*)malloc(sizeof(PhantomPhysicsComponent));
		testedEntity->AddComponent(PhantomPhysicsComponent::ComponentType, phantomPhysicsComponent);
		playerForcedMovementComponent = (PlayerForcedMovementComponent*)malloc(sizeof(PlayerForcedMovementComponent));
		testedEntity->AddComponent(PlayerForcedMovementComponent::ComponentType, playerForcedMovementComponent);
		possessableComponent = (PossessableComponent*)malloc(sizeof(PossessableComponent));
		testedEntity->AddComponent(PossessableComponent::ComponentType, possessableComponent);
		possessorComponent = (PossessorComponent*)malloc(sizeof(PossessorComponent));
		testedEntity->AddComponent(PossessorComponent::ComponentType, possessorComponent);
		propertyComponent = (PropertyComponent*)malloc(sizeof(PropertyComponent));
		testedEntity->AddComponent(PropertyComponent::ComponentType, propertyComponent);
		propertyEntranceComponent = (PropertyEntranceComponent*)malloc(sizeof(PropertyEntranceComponent));
		testedEntity->AddComponent(PropertyEntranceComponent::ComponentType, propertyEntranceComponent);
		propertyManagementComponent = (PropertyManagementComponent*)malloc(sizeof(PropertyManagementComponent));
		testedEntity->AddComponent(PropertyManagementComponent::ComponentType, propertyManagementComponent);
		propertyVendorComponent = (PropertyVendorComponent*)malloc(sizeof(PropertyVendorComponent));
		testedEntity->AddComponent(PropertyVendorComponent::ComponentType, propertyVendorComponent);
		proximityMonitorComponent = (ProximityMonitorComponent*)malloc(sizeof(ProximityMonitorComponent));
		testedEntity->AddComponent(ProximityMonitorComponent::ComponentType, proximityMonitorComponent);
		racingControlComponent = (RacingControlComponent*)malloc(sizeof(RacingControlComponent));
		testedEntity->AddComponent(RacingControlComponent::ComponentType, racingControlComponent);
		railActivatorComponent = (RailActivatorComponent*)malloc(sizeof(RailActivatorComponent));
		testedEntity->AddComponent(RailActivatorComponent::ComponentType, railActivatorComponent);
		rebuildComponent = (RebuildComponent*)malloc(sizeof(RebuildComponent));
		testedEntity->AddComponent(RebuildComponent::ComponentType, rebuildComponent);
		renderComponent = (RenderComponent*)malloc(sizeof(RenderComponent));
		testedEntity->AddComponent(RenderComponent::ComponentType, renderComponent);
		rigidbodyPhantomPhysicsComponent = (RigidbodyPhantomPhysicsComponent*)malloc(sizeof(RigidbodyPhantomPhysicsComponent));
		testedEntity->AddComponent(RigidbodyPhantomPhysicsComponent::ComponentType, rigidbodyPhantomPhysicsComponent);
		rocketLaunchLupComponent = (RocketLaunchLupComponent*)malloc(sizeof(RocketLaunchLupComponent));
		testedEntity->AddComponent(RocketLaunchLupComponent::ComponentType, rocketLaunchLupComponent);
		rocketLaunchpadControlComponent = (RocketLaunchpadControlComponent*)malloc(sizeof(RocketLaunchpadControlComponent));
		testedEntity->AddComponent(RocketLaunchpadControlComponent::ComponentType, rocketLaunchpadControlComponent);
		scriptedActivityComponent = (ScriptedActivityComponent*)malloc(sizeof(ScriptedActivityComponent));
		testedEntity->AddComponent(ScriptedActivityComponent::ComponentType, scriptedActivityComponent);
		shootingGalleryComponent = (ShootingGalleryComponent*)malloc(sizeof(ShootingGalleryComponent));
		testedEntity->AddComponent(ShootingGalleryComponent::ComponentType, shootingGalleryComponent);
		simplePhysicsComponent = (SimplePhysicsComponent*)malloc(sizeof(SimplePhysicsComponent));
		testedEntity->AddComponent(SimplePhysicsComponent::ComponentType, simplePhysicsComponent);
		skillComponent = (SkillComponent*)malloc(sizeof(SkillComponent));
		testedEntity->AddComponent(SkillComponent::ComponentType, skillComponent);
		soundTriggerComponent = (SoundTriggerComponent*)malloc(sizeof(SoundTriggerComponent));
		testedEntity->AddComponent(SoundTriggerComponent::ComponentType, soundTriggerComponent);
		switchComponent = (SwitchComponent*)malloc(sizeof(SwitchComponent));
		testedEntity->AddComponent(SwitchComponent::ComponentType, switchComponent);
		triggerComponent = (TriggerComponent*)malloc(sizeof(TriggerComponent));
		testedEntity->AddComponent(TriggerComponent::ComponentType, triggerComponent);
		vehiclePhysicsComponent = (VehiclePhysicsComponent*)malloc(sizeof(VehiclePhysicsComponent));
		testedEntity->AddComponent(VehiclePhysicsComponent::ComponentType, vehiclePhysicsComponent);
		vendorComponent = (VendorComponent*)malloc(sizeof(VendorComponent));
		testedEntity->AddComponent(VendorComponent::ComponentType, vendorComponent);
	}

	void TearDown() override {
		TearDownDependencies();
		free(baseCombatAIComponent);
		free(bouncerComponent);
		free(buffComponent);
		free(buildBorderComponent);
		free(characterComponent);
		free(controllablePhysicsComponent);
		free(destroyableComponent);
		free(inventoryComponent);
		free(levelProgressionComponent);
		free(lUPExhibitComponent);
		free(missionComponent);
		free(missionOfferComponent);
		free(modelComponent);
		free(moduleAssemblyComponent);
		free(movementAIComponent);
		free(movingPlatformComponent);
		free(petComponent);
		free(phantomPhysicsComponent);
		free(playerForcedMovementComponent);
		free(possessableComponent);
		free(possessorComponent);
		free(propertyComponent);
		free(propertyEntranceComponent);
		free(propertyManagementComponent);
		free(propertyVendorComponent);
		free(proximityMonitorComponent);
		free(racingControlComponent);
		free(railActivatorComponent);
		free(rebuildComponent);
		free(renderComponent);
		free(rigidbodyPhantomPhysicsComponent);
		free(rocketLaunchLupComponent);
		free(rocketLaunchpadControlComponent);
		free(scriptedActivityComponent);
		free(shootingGalleryComponent);
		free(simplePhysicsComponent);
		free(skillComponent);
		free(soundTriggerComponent);
		free(switchComponent);
		free(triggerComponent);
		free(vehiclePhysicsComponent);
		free(vendorComponent);
		operator delete(testedEntity);
	}
};

TEST_F(EntityTests, EntityConstructionTest) {
	CBITSTREAM;
	// testedEntity->WriteComponents(&bitStream, eReplicaPacketType::CONSTRUCTION);
}

TEST_F(EntityTests, EntitySerializationTest) {
	CBITSTREAM;
	// testedEntity->WriteComponents(&bitStream, eReplicaPacketType::SERIALIZATION);
}
