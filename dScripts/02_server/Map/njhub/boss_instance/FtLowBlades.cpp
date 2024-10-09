// Seperate damage proxy for low blade spinners

#include "FtLowBlades.h"
#include "Entity.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "DestroyableComponent.h"
#include "ProximityMonitorComponent.h"
#include "MissionComponent.h"
#include "EntityInfo.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"
#include "SkillComponent.h"

void FtLowBlades::OnStartup(Entity* self) {
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();		
	self->SetProximityRadius(5.9, "damage_distance");		
	m_Counter = 0;
}

void FtLowBlades::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
// Damage players via proximity radius 	
	if (name == "damage_distance") {
		if (m_Counter == 1) {		
		
			if (entering->IsPlayer()) {
				auto* skillComponent = self->GetComponent<SkillComponent>();
				auto* skillComponentPlayer = entering->GetComponent<SkillComponent>();
				
				if (skillComponent == nullptr) {
					return;
				}

				skillComponentPlayer->CalculateBehavior(99994, 99994, entering->GetObjectID(), true); // player's skill	(got hit fx)	

				auto dir = entering->GetRotation().GetForwardVector();

				dir.y = 11;
				dir.x = -dir.x * 14;
				dir.z = -dir.z * 14;

				GameMessages::SendKnockback(entering->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 1000, dir);
			}

		}
	}
	
// End
}

void FtLowBlades::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {
	if (name == "LowBladeDamage") {
		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();		
		self->SetProximityRadius(5.9, "damage_distance");		
		self->AddTimer("ProxRadius", 1.1f);		
		self->AddTimer("ResetDamage", 7.1f);			
	}	
}		

void FtLowBlades::OnTimerDone(Entity* self, std::string timerName) {
		
	if (timerName == "SkillPulse") {	
// Spinner damage skill exclusive for enemies	
		auto* skillComponent = self->GetComponent<SkillComponent>();

		if (skillComponent == nullptr) {
			return;
		}

		skillComponent->CalculateBehavior(971, 20371, self->GetObjectID(), true); // spinner's skill
//		effectID from behaviorID 20370 removed -> inaccurate offset & annoying
		if (m_Counter == 1) {
			self->AddTimer("SkillPulse", 1);	// <- set based on pulseTime, default = 1 
		}
// End		
	}
	else if (timerName == "ProxRadius") {		
		m_Counter = 1;
//		^^ If ProxRadius activated, m_Counter should be 1 anyways		
		
	}	
	else if (timerName == "ResetDamage") {		
		m_Counter = 0;
	}	
}
