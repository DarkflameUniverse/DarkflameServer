#include "EnemySpiderSpawner.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"

//----------------------------------------------
//--Initiate egg hatching on call
//----------------------------------------------
void EnemySpiderSpawner::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (args == "prepEgg") {
		// Highlight eggs about to hatch with Maelstrom effect
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 2856, u"maelstrom", "test", LWOOBJID_EMPTY, 1.0f, 1.0f, true);

		// Make indestructible
		auto dest = static_cast<DestroyableComponent*>(self->GetComponent(COMPONENT_TYPE_DESTROYABLE));
		if (dest) {
			dest->SetFaction(-1);
		}
		EntityManager::Instance()->SerializeEntity(self);

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
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 644, u"create", "egg_puff_b", LWOOBJID_EMPTY, 1.0f, 1.0f, true);

		//TODO: set the aggro radius larger

		EntityInfo info{};
		info.lot = 16197;
		info.pos = self->GetPosition();
		info.spawner = nullptr;
		info.spawnerID = self->GetI64(u"SpawnOwner");
		info.spawnerNodeID = 0;

		Entity* newEntity = EntityManager::Instance()->CreateEntity(info, nullptr);
		if (newEntity) {
			EntityManager::Instance()->ConstructEntity(newEntity);
			newEntity->GetGroups().push_back("BabySpider");

			/*
			auto* movementAi = newEntity->GetComponent<MovementAIComponent>();

			movementAi->SetDestination(newEntity->GetPosition());
			*/
		}

		self->ScheduleKillAfterUpdate();
	}
}

//--------------------------------------------------------------
//Called when it is finally time to release the Spiderlings
//--------------------------------------------------------------
void EnemySpiderSpawner::SpawnSpiderling(Entity* self) {
	//Initiate the actual spawning
	GameMessages::SendPlayFXEffect(self->GetObjectID(), 2260, u"rebuild_medium", "dropdustmedium", LWOOBJID_EMPTY, 1.0f, 1.0f, true);
	self->AddTimer("SpawnSpiderling", spawnTime);
}
