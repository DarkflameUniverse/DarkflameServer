#include "SpawnerSpinner30.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"

void SpawnerSpinner30::OnStartup(Entity* self) {
	
	AIsActivated = 0;
	BIsActivated = 0;
	CIsActivated = 0;
	DIsActivated = 0;
	
		

	RenderComponent::PlayAnimation(self, u"idle");
    // Turn on moving platforms
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1, 1, eMovementPlatformState::Moving);
	
}

void SpawnerSpinner30::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	
	statue = sender;
	const auto spawnNw = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawnNw1"));	
	
	if (args == "spinneron") {	
	

//The spawninFX is delayed by ~3 seconds, assuming it's intentional & timing everything around it:				
	
//		Spawner jumble
		if (spawnNw == "4StatuesBeetlesA") {
			if (AIsActivated == 0) {
				self->SetNetworkVar(u"enemiesalive", 2);		
//				self->SetVar<int>(u"enemycount", 2);					
				self->AddTimer("BeetlesA", 3.1f);
				self->AddTimer("MoveUp", 0.8f);	
				GameMessages::SendPlayFXEffect(self->GetObjectID(), 10102, u"create", "create");					
			}
		} 
		if (spawnNw == "4StatuesBeetlesB") {
			if (BIsActivated == 0) {
				self->SetNetworkVar(u"enemiesalive", 2);
//				self->SetVar<int>(u"enemycount", 2);					
				self->AddTimer("BeetlesB", 3.1f);
				self->AddTimer("MoveUp", 0.8f);	
				GameMessages::SendPlayFXEffect(self->GetObjectID(), 10102, u"create", "create");	
			}
		} 
		if (spawnNw == "4StatuesBeetlesC") {
			if (CIsActivated == 0) {	
				self->SetNetworkVar(u"enemiesalive", 2);	
//				self->SetVar<int>(u"enemycount", 2);					
				self->AddTimer("BeetlesC", 3.1f);
				self->AddTimer("MoveUp", 0.8f);	
				GameMessages::SendPlayFXEffect(self->GetObjectID(), 10102, u"create", "create");	
			}
		} 
		if (spawnNw == "4StatuesBeetlesD") {
			if (DIsActivated == 0) {	
				self->SetNetworkVar(u"enemiesalive", 2);	
//				self->SetVar<int>(u"enemycount", 2);	
				self->AddTimer("BeetlesD", 3.1f);
				self->AddTimer("MoveUp", 0.8f);	
				GameMessages::SendPlayFXEffect(self->GetObjectID(), 10102, u"create", "create");	
			}
		}

	
	}	
}

void SpawnerSpinner30::SpawnLegs(Entity* self, const std::string& loc) {
	auto pos = self->GetPosition();
	auto rot = self->GetRotation();
	pos.y += self->GetVarAs<float>(u"vert_offset");

	auto newRot = rot;
	auto offset = self->GetVarAs<float>(u"hort_offset");

	std::vector<LDFBaseData*> config = { new LDFData<std::string>(u"Leg", loc) };

	EntityInfo info{};
	info.lot = 13997;
	info.spawnerID = self->GetObjectID();
	info.settings = config;
	info.rot = newRot;

	if (loc == "BeetlesA1") {
		const auto dir = rot.GetRightVector();		
		pos.x = -678.97211;
		pos.y = 263.9136;
		pos.z = -485.1644;
		info.pos = pos;
	} else if (loc == "BeetlesA2") {		
		const auto dir = rot.GetRightVector();		
		pos.x = -693.16827;
		pos.y = 263.91735;
		pos.z = -485.13586;
		info.pos = pos;
	} else if (loc == "BeetlesB1") {
		const auto dir = rot.GetRightVector();		
		pos.x = -489.10843;
		pos.y = 264.16498;
		pos.z = -483.99808;
		info.pos = pos;
	} else if (loc == "BeetlesB2") {		
		const auto dir = rot.GetRightVector();		
		pos.x = -503.08203;
		pos.y = 264.02432;
		pos.z = -483.75735;
		info.pos = pos;
	} else if (loc == "BeetlesC1") {
		const auto dir = rot.GetRightVector();		
		pos.x = -496.74146;
		pos.y = 263.9549;
		pos.z = -628.29474;
		info.pos = pos;
	} else if (loc == "BeetlesC2") {		
		const auto dir = rot.GetRightVector();		
		pos.x = -481.57059;
		pos.y = 264.1012;
		pos.z = -627.54999;
		info.pos = pos;
	} else if (loc == "BeetlesD1") {
		const auto dir = rot.GetRightVector();		
		pos.x = -702.24298;
		pos.y = 263.92444;
		pos.z = -627.49658;
		info.pos = pos;
	} else if (loc == "BeetlesD2") {		
		const auto dir = rot.GetRightVector();		
		pos.x = -687.06604;
		pos.y = 263.91791;
		pos.z = -627.74445;
		info.pos = pos;
	}

	info.rot = NiQuaternion::LookAt(info.pos, statue->GetPosition());

	auto* entity = Game::entityManager->CreateEntity(info);

	Game::entityManager->ConstructEntity(entity);

	OnChildLoaded(self, entity);
}

void SpawnerSpinner30::OnChildLoaded(Entity* self, Entity* child) {
//	int EnemiesAlive = self->GetNetworkVar<int>(u"enemiesalive");
//	auto EnemyCount = self->GetVar<int>(u"enemycount");		
	const auto selfID = self->GetObjectID();	
	child->AddDieCallback([this, selfID, child]() {
		auto* self = Game::entityManager->GetEntity(selfID);
		auto* destroyableComponent = child->GetComponent<DestroyableComponent>();

		if (destroyableComponent == nullptr || self == nullptr) {
			return;
		}

// Get the current value of the "enemiesalive" variable

		auto EnemiesAlive = self->GetNetworkVar<int>(u"enemiesalive");
		// Decrement the value by 1
		if (EnemiesAlive > 0) {
			EnemiesAlive--;
			self->SetNetworkVar(u"enemiesalive", EnemiesAlive);
		}



		});
}

void SpawnerSpinner30::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "MoveUp") {		
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
		RenderComponent::PlayAnimation(self, u"up");	
		
//		Ascend sfx
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{5c30c263-00ae-42a2-80a3-2ae33c8f13fe}");	
		self->AddTimer("AscentGUID", 0.1f);		
	}
	if (timerName == "BeetlesA") {
		AIsActivated = 1;
//		self->SetVar<uint32_t>(u"AIsActivated", 1);
		SpawnLegs(self, "BeetlesA1");	
		SpawnLegs(self, "BeetlesA2");
	}
	if (timerName == "BeetlesB") {	
		BIsActivated = 1;
//		self->SetVar<uint32_t>(u"BIsActivated", 1);	
		SpawnLegs(self, "BeetlesB1");	
		SpawnLegs(self, "BeetlesB2");
	}
	if (timerName == "BeetlesC") {	
		CIsActivated = 1;	
//		self->SetVar<uint32_t>(u"CIsActivated", 1);
		SpawnLegs(self, "BeetlesC1");	
		SpawnLegs(self, "BeetlesC2");
	}
	if (timerName == "BeetlesD") {		
		DIsActivated = 1;
//		self->SetVar<uint32_t>(u"DIsActivated", 1);
		SpawnLegs(self, "BeetlesD1");	
		SpawnLegs(self, "BeetlesD2");	
	}
	
//Handle spinner sound orders
	if (timerName == "AscentGUID") {
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");	
	}
	if (timerName == "DescentGUID") {
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{97b60c03-51f2-45b6-80cc-ccbbef0d94cf}");	
	}	
}

//Statue default val
Entity* SpawnerSpinner30::statue = nullptr;
