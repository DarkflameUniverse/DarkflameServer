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
	BaseCombatAIComponent* baseCombatAIComponent;
	BouncerComponent* bouncerComponent;
	BuffComponent* buffComponent;
	BuildBorderComponent* buildBorderComponent;
	CharacterComponent* characterComponent;
	Component* component;
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
	std::unique_ptr<Entity> testedEntity;
	void SetUp() override {
		SetUpDependencies();
		testedEntity = std::make_unique<Entity>(1, info);
		// We allocate through malloc because we cannot call the constructors of some of thse methods.
		baseCombatAIComponent = (BaseCombatAIComponent*)malloc(sizeof(BaseCombatAIComponent));
		bouncerComponent = (BouncerComponent*)malloc(sizeof(BouncerComponent));
		buffComponent = (BuffComponent*)malloc(sizeof(BuffComponent));
		buildBorderComponent = (BuildBorderComponent*)malloc(sizeof(BuildBorderComponent));
		characterComponent = (CharacterComponent*)malloc(sizeof(CharacterComponent));
		component = (Component*)malloc(sizeof(Component));
		controllablePhysicsComponent = (ControllablePhysicsComponent*)malloc(sizeof(ControllablePhysicsComponent));
		destroyableComponent = (DestroyableComponent*)malloc(sizeof(DestroyableComponent));
		inventoryComponent = (InventoryComponent*)malloc(sizeof(InventoryComponent));
		levelProgressionComponent = (LevelProgressionComponent*)malloc(sizeof(LevelProgressionComponent));
		lUPExhibitComponent = (LUPExhibitComponent*)malloc(sizeof(LUPExhibitComponent));
		missionComponent = (MissionComponent*)malloc(sizeof(MissionComponent));
		missionOfferComponent = (MissionOfferComponent*)malloc(sizeof(MissionOfferComponent));
		modelComponent = (ModelComponent*)malloc(sizeof(ModelComponent));
		moduleAssemblyComponent = (ModuleAssemblyComponent*)malloc(sizeof(ModuleAssemblyComponent));
		movementAIComponent = (MovementAIComponent*)malloc(sizeof(MovementAIComponent));
		movingPlatformComponent = (MovingPlatformComponent*)malloc(sizeof(MovingPlatformComponent));
		petComponent = (PetComponent*)malloc(sizeof(PetComponent));
		phantomPhysicsComponent = (PhantomPhysicsComponent*)malloc(sizeof(PhantomPhysicsComponent));
		playerForcedMovementComponent = (PlayerForcedMovementComponent*)malloc(sizeof(PlayerForcedMovementComponent));
		possessableComponent = (PossessableComponent*)malloc(sizeof(PossessableComponent));
		possessorComponent = (PossessorComponent*)malloc(sizeof(PossessorComponent));
		propertyComponent = (PropertyComponent*)malloc(sizeof(PropertyComponent));
		propertyEntranceComponent = (PropertyEntranceComponent*)malloc(sizeof(PropertyEntranceComponent));
		propertyManagementComponent = (PropertyManagementComponent*)malloc(sizeof(PropertyManagementComponent));
		propertyVendorComponent = (PropertyVendorComponent*)malloc(sizeof(PropertyVendorComponent));
		proximityMonitorComponent = (ProximityMonitorComponent*)malloc(sizeof(ProximityMonitorComponent));
		racingControlComponent = (RacingControlComponent*)malloc(sizeof(RacingControlComponent));
		railActivatorComponent = (RailActivatorComponent*)malloc(sizeof(RailActivatorComponent));
		rebuildComponent = (RebuildComponent*)malloc(sizeof(RebuildComponent));
		renderComponent = (RenderComponent*)malloc(sizeof(RenderComponent));
		rigidbodyPhantomPhysicsComponent = (RigidbodyPhantomPhysicsComponent*)malloc(sizeof(RigidbodyPhantomPhysicsComponent));
		rocketLaunchLupComponent = (RocketLaunchLupComponent*)malloc(sizeof(RocketLaunchLupComponent));
		rocketLaunchpadControlComponent = (RocketLaunchpadControlComponent*)malloc(sizeof(RocketLaunchpadControlComponent));
		scriptedActivityComponent = (ScriptedActivityComponent*)malloc(sizeof(ScriptedActivityComponent));
		shootingGalleryComponent = (ShootingGalleryComponent*)malloc(sizeof(ShootingGalleryComponent));
		simplePhysicsComponent = (SimplePhysicsComponent*)malloc(sizeof(SimplePhysicsComponent));
		skillComponent = (SkillComponent*)malloc(sizeof(SkillComponent));
		soundTriggerComponent = (SoundTriggerComponent*)malloc(sizeof(SoundTriggerComponent));
		switchComponent = (SwitchComponent*)malloc(sizeof(SwitchComponent));
		triggerComponent = (TriggerComponent*)malloc(sizeof(TriggerComponent));
		vehiclePhysicsComponent = (VehiclePhysicsComponent*)malloc(sizeof(VehiclePhysicsComponent));
		vendorComponent = (VendorComponent*)malloc(sizeof(VendorComponent));
	}

	void TearDown() override {
		TearDownDependencies();
		free(baseCombatAIComponent);
		free(bouncerComponent);
		free(buffComponent);
		free(buildBorderComponent);
		free(characterComponent);
		free(component);
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
	}
};

TEST_F(EntityTests, EntityConstructionTest) {
	CBITSTREAM;
	testedEntity->WriteComponents(&bitStream, eReplicaPacketType::CONSTRUCTION);
}

TEST_F(EntityTests, EntitySerializationTest) {
	CBITSTREAM;
	testedEntity->WriteComponents(&bitStream, eReplicaPacketType::SERIALIZATION);
}
