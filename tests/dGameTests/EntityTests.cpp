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
		Game::logger->Log("EntityTests", "starting");
		bouncerComponent = new BouncerComponent(testedEntity);
		Game::logger->Log("EntityTests", "BouncerComponent");
		buffComponent = new BuffComponent(testedEntity);
		Game::logger->Log("EntityTests", "BuffComponent");
		buildBorderComponent = new BuildBorderComponent(testedEntity);
		Game::logger->Log("EntityTests", "BuildBorderComponent");
		characterComponent = new CharacterComponent(testedEntity, nullptr);
		Game::logger->Log("EntityTests", "CharacterComponent");
		controllablePhysicsComponent = new ControllablePhysicsComponent(testedEntity);
		Game::logger->Log("EntityTests", "ControllablePhysicsComponent");
		destroyableComponent = new DestroyableComponent(testedEntity);
		Game::logger->Log("EntityTests", "DestroyableComponent");
		inventoryComponent = new InventoryComponent(testedEntity);
		Game::logger->Log("EntityTests", "InventoryComponent");
		levelProgressionComponent = new LevelProgressionComponent(testedEntity);
		Game::logger->Log("EntityTests", "LevelProgressionComponent");
		lUPExhibitComponent = new LUPExhibitComponent(testedEntity);
		Game::logger->Log("EntityTests", "LUPExhibitComponent");
		missionComponent = new MissionComponent(testedEntity);
		Game::logger->Log("EntityTests", "MissionComponent");
		missionOfferComponent = new MissionOfferComponent(testedEntity, 2345);
		Game::logger->Log("EntityTests", "MissionOfferComponent");
		modelComponent = new ModelComponent(testedEntity);
		Game::logger->Log("EntityTests", "ModelComponent");
		moduleAssemblyComponent = new ModuleAssemblyComponent(testedEntity);
		Game::logger->Log("EntityTests", "ModuleAssemblyComponent");
		movementAIComponent = new MovementAIComponent(testedEntity, MovementAIInfo());
		Game::logger->Log("EntityTests", "MovementAIComponent");
		movingPlatformComponent = new MovingPlatformComponent(testedEntity, "");
		Game::logger->Log("EntityTests", "MovingPlatformComponent");
		petComponent = new PetComponent(testedEntity, 1234);
		Game::logger->Log("EntityTests", "PetComponent");
		phantomPhysicsComponent = new PhantomPhysicsComponent(testedEntity);
		Game::logger->Log("EntityTests", "PhantomPhysicsComponent");
		playerForcedMovementComponent = new PlayerForcedMovementComponent(testedEntity);
		Game::logger->Log("EntityTests", "PlayerForcedMovementComponent");
		possessableComponent = new PossessableComponent(testedEntity, 124);
		Game::logger->Log("EntityTests", "PossessableComponent");
		possessorComponent = new PossessorComponent(testedEntity);
		Game::logger->Log("EntityTests", "PossessorComponent");
		propertyComponent = new PropertyComponent(testedEntity);
		Game::logger->Log("EntityTests", "PropertyComponent");
		propertyEntranceComponent = new PropertyEntranceComponent(38, testedEntity);
		Game::logger->Log("EntityTests", "PropertyEntranceComponent");
		propertyManagementComponent = new PropertyManagementComponent(testedEntity);
		Game::logger->Log("EntityTests", "PropertyManagementComponent");
		propertyVendorComponent = new PropertyVendorComponent(testedEntity);
		Game::logger->Log("EntityTests", "PropertyVendorComponent");
		proximityMonitorComponent = new ProximityMonitorComponent(testedEntity);
		Game::logger->Log("EntityTests", "ProximityMonitorComponent");
		racingControlComponent = new RacingControlComponent(testedEntity);
		Game::logger->Log("EntityTests", "RacingControlComponent");
		railActivatorComponent = new RailActivatorComponent(testedEntity, 4231);
		Game::logger->Log("EntityTests", "RailActivatorComponent");
		rebuildComponent = new RebuildComponent(testedEntity);
		Game::logger->Log("EntityTests", "RebuildComponent");
		renderComponent = new RenderComponent(testedEntity);
		Game::logger->Log("EntityTests", "RenderComponent");
		rigidbodyPhantomPhysicsComponent = new RigidbodyPhantomPhysicsComponent(testedEntity);
		Game::logger->Log("EntityTests", "RigidbodyPhantomPhysicsComponent");
		rocketLaunchLupComponent = new RocketLaunchLupComponent(testedEntity);
		Game::logger->Log("EntityTests", "RocketLaunchLupComponent");
		rocketLaunchpadControlComponent = new RocketLaunchpadControlComponent(testedEntity, 5);
		Game::logger->Log("EntityTests", "RocketLaunchpadControlComponent");
		scriptedActivityComponent = new ScriptedActivityComponent(testedEntity, 4);
		Game::logger->Log("EntityTests", "ScriptedActivityComponent");
		shootingGalleryComponent = new ShootingGalleryComponent(testedEntity);
		Game::logger->Log("EntityTests", "ShootingGalleryComponent");
		simplePhysicsComponent = new SimplePhysicsComponent(3, testedEntity);
		Game::logger->Log("EntityTests", "SimplePhysicsComponent");
		skillComponent = new SkillComponent(testedEntity);
		Game::logger->Log("EntityTests", "SkillComponent");
		soundTriggerComponent = new SoundTriggerComponent(testedEntity);
		Game::logger->Log("EntityTests", "SoundTriggerComponent");
		switchComponent = new SwitchComponent(testedEntity);
		Game::logger->Log("EntityTests", "SwitchComponent");
		triggerComponent = new TriggerComponent(testedEntity, "");
		Game::logger->Log("EntityTests", "TriggerComponent");
		vehiclePhysicsComponent = new VehiclePhysicsComponent(testedEntity);
		Game::logger->Log("EntityTests", "VehiclePhysicsComponent");
		vendorComponent = new VendorComponent(testedEntity);
Game::logger->Log("EntityTests", "VendorComponent");
		baseCombatAIComponent->SetState(AiState::dead);
		combatAiComponentTarget = rand();
		baseCombatAIComponent->SetTarget(combatAiComponentTarget);

		bouncerComponent->SetPetEnabled(true);
	}

	void TearDown() override {
		TearDownDependencies();
		delete baseCombatAIComponent;
		delete bouncerComponent;
		delete buffComponent;
		delete buildBorderComponent;
		delete characterComponent;
		delete controllablePhysicsComponent;
		delete destroyableComponent;
		delete inventoryComponent;
		delete levelProgressionComponent;
		delete lUPExhibitComponent;
		delete missionComponent;
		delete missionOfferComponent;
		delete modelComponent;
		delete moduleAssemblyComponent;
		delete movementAIComponent;
		delete movingPlatformComponent;
		delete petComponent;
		delete phantomPhysicsComponent;
		delete playerForcedMovementComponent;
		delete possessableComponent;
		delete possessorComponent;
		delete propertyComponent;
		delete propertyEntranceComponent;
		delete propertyManagementComponent;
		delete propertyVendorComponent;
		delete proximityMonitorComponent;
		delete racingControlComponent;
		delete railActivatorComponent;
		delete rebuildComponent;
		delete renderComponent;
		delete rigidbodyPhantomPhysicsComponent;
		delete rocketLaunchLupComponent;
		delete rocketLaunchpadControlComponent;
		delete scriptedActivityComponent;
		delete shootingGalleryComponent;
		delete simplePhysicsComponent;
		delete skillComponent;
		delete soundTriggerComponent;
		delete switchComponent;
		delete triggerComponent;
		delete vehiclePhysicsComponent;
		delete vendorComponent;
		operator delete(testedEntity);
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
