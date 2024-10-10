#include "BaseInteractDropLootChest.h"
#include "Loot.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "eMissionState.h"
#include "TeamManager.h"

void BaseInteractDropLootChest::OnUse(Entity* self, Entity* user) {

	if (self->GetLOT() == 16842) {
		if (self->GetNetworkVar<int>(u"WasOpened") != 1) {
			const auto teamSize = self->GetVar<int>(u"playercount");
			if (teamSize ==	2) {	
				self->SetVar<int32_t>(u"smashable_loot_matrix", 906);
			} else if (teamSize ==	3) {		
				self->SetVar<int32_t>(u"smashable_loot_matrix", 907);
			} else if (teamSize ==	4) {		
				self->SetVar<int32_t>(u"smashable_loot_matrix", 908);
			} else {
				self->SetVar<int32_t>(u"smashable_loot_matrix", 905);
			}
			self->SetNetworkVar<int>(u"WasOpened", 1);			
			BaseUse(self, user);	
			return;
		} else {
			return;
		}
	}	


    LWOOBJID userID1 = self->GetVar<LWOOBJID>(u"UserId1");
    LWOOBJID userID2 = self->GetVar<LWOOBJID>(u"UserId2");
    LWOOBJID userID3 = self->GetVar<LWOOBJID>(u"UserId3");
    LWOOBJID userID4 = self->GetVar<LWOOBJID>(u"UserId4");
    LWOOBJID userID5 = self->GetVar<LWOOBJID>(u"UserId5");
	
    if (userID1 == user->GetObjectID() || 
        userID2 == user->GetObjectID() || 
        userID3 == user->GetObjectID() || 
        userID4 == user->GetObjectID() || 
        userID5 == user->GetObjectID()) {
        return;
    }
	
    if (userID1 == LWOOBJID(0)) {
        self->SetVar<LWOOBJID>(u"UserId1", user->GetObjectID());
    } else if (userID2 == LWOOBJID(0)) {
        self->SetVar<LWOOBJID>(u"UserId2", user->GetObjectID());
    } else if (userID3 == LWOOBJID(0)) {
        self->SetVar<LWOOBJID>(u"UserId3", user->GetObjectID());
    } else if (userID4 == LWOOBJID(0)) {
        self->SetVar<LWOOBJID>(u"UserId4", user->GetObjectID());
    } else if (userID5 == LWOOBJID(0)) {
        self->SetVar<LWOOBJID>(u"UserId5", user->GetObjectID());
    }	

	if (self->GetVarAsString(u"trigger_id") == "25:2" && self->GetVar<int>(u"GateOpened") != 1) {
		
		self->SetVar<int>(u"GateOpened", 1);
		
		const auto MPGate = Game::entityManager->GetEntitiesInGroup("8SpinMPGate");
		for (auto* gate : MPGate) {
			gate->NotifyObject(self, "OnChestActivated");
		}		
	}
	BaseUse(self, user);	
}

void BaseInteractDropLootChest::BaseUse(Entity* self, Entity* user) {

//	Temple Raider acheivement

	auto* missionComponent = user->GetComponent<MissionComponent>();	
	if (missionComponent != nullptr) {	
		for (const auto missionID : MissionIds) {
			missionComponent->ForceProgressValue(missionID, 1, self->GetLOT());
		}
	}	

//	dispense loot
	
	uint32_t lootMatrix = self->GetVar<int32_t>(u"UseLootMatrix");
	if (lootMatrix == 0) lootMatrix = self->GetVar<int32_t>(u"smashable_loot_matrix");
	if (lootMatrix == 0) lootMatrix = 919;

	auto useSound = self->GetVar<std::string>(u"sound1");
	if (!useSound.empty()) {
		GameMessages::SendPlayNDAudioEmitter(self, user->GetSystemAddress(), useSound);
	}

	if (self->GetLOT() != 16842) {
		Loot::DropLoot(user, self, lootMatrix, 0, 0);
	} else {
		auto team = TeamManager::Instance()->GetTeam(user->GetObjectID());
		if (team != nullptr) { 		
			for (auto memberID : team->members) {
				auto* memberEntity = Game::entityManager->GetEntity(memberID);
				Loot::DropLoot(memberEntity, self, lootMatrix, 0, 0);
			}		
		} else {
			Loot::DropLoot(user, self, lootMatrix, 0, 0);
		}	
	}
	

	if (missionComponent != nullptr && missionComponent->GetMissionState(2106) == eMissionState::COMPLETE){
			PostTokenUse(self, user);
		}	
}

void BaseInteractDropLootChest::PostTokenUse(Entity* self, Entity* user) {		
		
	if (self->GetLOT() == 16837) {
		Loot::DropLoot(user, self, 32867, 0, 0);
	} else if (self->GetLOT() == 16841) {
		Loot::DropLoot(user, self, 32868, 0, 0);
	} else if (self->GetLOT() == 16839) {
		Loot::DropLoot(user, self, 32869, 0, 0);
	} else if (self->GetLOT() == 16838) {
		Loot::DropLoot(user, self, 32870, 0, 0);
	}	
}