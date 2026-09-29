#include "EnemySpiderSpawner.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "EntityManager.h"
#include "EntityInfo.h"
#include "DestroyableComponent.h"
#include "eReplicaComponentType.h"

//----------------------------------------------
//--Initiate egg hatching on call
//----------------------------------------------
void EnemySpiderSpawner::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (args == "prepEgg") {
		// Highlight eggs about to hatch with Maelstrom effect
		GameMessages::PlayFXEffect(self->GetObjectID(), 2856, u"maelstrom", "test").Send(UNASSIGNED_SYSTEM_ADDRESS);

		// Make indestructible
		auto dest = static_cast<DestroyableComponent*>(self->GetComponent(eReplicaComponentType::DESTROYABLE));
		if (dest) {
			dest->SetFaction(-1);
		}
		Game::entityManager->SerializeEntity(self);

		// Keep track of who prepped me
		self->SetI64(u"SpawnOwner", sender->GetObjectID());

	} else if (args == "hatchEgg") {
		// Final countdown to pop
		self->AddTimer("StartSpawnTime", hatchTime);
	}
}

//----------------------------------------------------------------
//--Called when timers are done
//----------------------------------------------------------------
void EnemySpiderSpawner::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "StartSpawnTime") {
		SpawnSpiderling(self);
	} else if (timerName == "SpawnSpiderling") {
		GameMessages::PlayFXEffect(self->GetObjectID(), 644, u"create", "egg_puff_b").Send(UNASSIGNED_SYSTEM_ADDRESS);

		//TODO: set the aggro radius larger

		EntityInfo info{};
		info.lot = 16197;
		info.pos = self->GetPosition();
		info.spawner = nullptr;
		info.spawnerID = self->GetI64(u"SpawnOwner");
		info.spawnerNodeID = 0;

		Entity* newEntity = Game::entityManager->CreateEntity(info, nullptr);
		if (newEntity) {
			Game::entityManager->ConstructEntity(newEntity);
			newEntity->GetGroups().push_back("BabySpider");

			// The Spider Queen screams from the mountain when one of her spiderlings dies
			const auto spawnOwner = self->GetI64(u"SpawnOwner");
			const auto spiderlingID = newEntity->GetObjectID();
			newEntity->AddDieCallback([spawnOwner, spiderlingID]() {
				auto* const spiderBoss = Game::entityManager->GetEntity(spawnOwner);
				auto* const spiderling = Game::entityManager->GetEntity(spiderlingID);
				if (spiderBoss && spiderling) spiderBoss->GetScript()->OnNotifyObject(spiderBoss, spiderling, "SpiderlingDied");
			});
		}

		self->ScheduleKillAfterUpdate();
	}
}

//--------------------------------------------------------------
//Called when it is finally time to release the Spiderlings
//--------------------------------------------------------------
void EnemySpiderSpawner::SpawnSpiderling(Entity* self) {
	//Initiate the actual spawning
	GameMessages::PlayFXEffect(self->GetObjectID(), 2260, u"rebuild_medium", "dropdustmedium").Send(UNASSIGNED_SYSTEM_ADDRESS);
	self->AddTimer("SpawnSpiderling", spawnTime);
}
