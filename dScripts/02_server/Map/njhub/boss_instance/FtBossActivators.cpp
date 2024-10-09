#include "FtBossActivators.h"
#include "QuickBuildComponent.h"
#include "Character.h"
#include "RenderComponent.h"
#include "dZoneManager.h"

void FtBossActivators::OnUse(Entity* self, Entity* user) {
	const auto flag = self->GetVar<int32_t>(u"RailFlagNum");
	const auto TornadoEntity = Game::entityManager->GetEntitiesByLOT(16807);	
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();
	

	// Only allow use if this is not a quick build or the quick build is built
	if (quickBuildComponent == nullptr || quickBuildComponent->GetState() == eQuickBuildState::COMPLETED) {
		
//		Send tornado back to middle			
		for (auto* tornado : TornadoEntity) {	
			tornado->NotifyObject(self, "CallToMid");
		}	
	
	

		if (self->GetLOT() == 16906) {		
			self->AddTimer("FtEarthRailArrived", 4.5f);
			if (!self->GetVar<bool>(u"CasterIdentified")) {
				self->SetVar<bool>(u"CasterIdentified", true);
				self->SetVar(u"casterID", user->GetObjectID());
			}			
			self->AddTimer("CastEarthFX", 3);				
		} else if (self->GetLOT() == 16909) {	
			self->AddTimer("FtLightningRailArrived", 2.4f);
			if (!self->GetVar<bool>(u"CasterIdentified")) {
				self->SetVar<bool>(u"CasterIdentified", true);
				self->SetVar(u"casterID", user->GetObjectID());
			}
			self->AddTimer("CastLightningFX", 0.9f);				
		} else if (self->GetLOT() == 16907) {	
			self->AddTimer("FtIceRailArrived", 3);
			if (!self->GetVar<bool>(u"CasterIdentified")) {
				self->SetVar<bool>(u"CasterIdentified", true);
				self->SetVar(u"casterID", user->GetObjectID());
			}			
			self->AddTimer("CastIceFX", 1.5f);				
		} else if (self->GetLOT() == 16910) {		
			self->AddTimer("FtFireRailArrived", 2.6f);	
			if (!self->GetVar<bool>(u"CasterIdentified")) {
				self->SetVar<bool>(u"CasterIdentified", true);
				self->SetVar(u"casterID", user->GetObjectID());
			}
			self->AddTimer("CastFireFX", 1.1f);	
		}				

		
		
		auto* character = user->GetCharacter();
		if (character != nullptr) {
			character->SetPlayerFlag(flag, true);
		}
		
		self->SetNetworkVar<bool>(u"NetworkNotActive", true);	
		self->SetVar<bool>(u"NotActive", true);			
		
	}
}

void FtBossActivators::PortalHitFX(Entity* self) {
	const auto PortalEntity = Game::entityManager->GetEntitiesByLOT(16802);
	
//	Play anim & proper fx		
	for (auto* portal : PortalEntity) {			
		RenderComponent::PlayAnimation(portal, u"onhit");				
		GameMessages::SendPlayFXEffect(portal->GetObjectID(), 10143, u"death", "darkitect_portal_shrink");			
	}	

//	Smash portal	
	self->AddTimer("SmashPortal", 2.1f);	
	
}	

void FtBossActivators::OnQuickBuildComplete(Entity* self, Entity* target) {
	const auto BossManager = Game::entityManager->GetEntitiesInGroup("BossManager");		
	
	self->SetNetworkVar<bool>(u"NetworkNotActive", false);	
	self->SetVar<bool>(u"NotActive", false);	


	for (auto* manager : BossManager) {
		manager->NotifyObject(self, "SendEndpostOn");	
	}		
	
}	

void FtBossActivators::OnTimerDone(Entity* self, std::string timerName) {
	const auto BossManager = Game::entityManager->GetEntitiesInGroup("BossManager");
	const auto PortalEntity = Game::entityManager->GetEntitiesByLOT(16802);
	
	if (timerName == "FtFireRailArrived") {	
		for (auto* manager : BossManager) {
			manager->NotifyObject(self, "FireRailArrived");	
		}						

	} else if (timerName == "FtIceRailArrived") {
		for (auto* manager : BossManager) {
			manager->NotifyObject(self, "IceRailArrived");		
		}
						
	} else if (timerName == "FtLightningRailArrived") {
		for (auto* manager : BossManager) {
			manager->NotifyObject(self, "LightningRailArrived");			
		}			
		
	} else if (timerName == "FtEarthRailArrived") {
		for (auto* manager : BossManager) {
			manager->NotifyObject(self, "EarthRailArrived");		
		}				
									
	} else if (timerName == "CastEarthFX") {
		if (!self->GetVar<bool>(u"EarthRailUsed")) {
			self->SetVar<bool>(u"EarthRailUsed", true);		
			if (self->GetVar<LWOOBJID>(u"casterID")) {
				GameMessages::SendPlayFXEffect(self->GetVar<LWOOBJID>(u"casterID"), 10148, u"create", 
				"darkitect_portal_onhit_earth");				
				for (auto* portal : PortalEntity) {			
					GameMessages::SendPlayFXEffect(portal->GetObjectID(), 10148, u"create", 
					"darkitect_portal_onhit_earth");			
				}							
			}	
			PortalHitFX(self);				
		}			
	} else if (timerName == "CastLightningFX") {
		if (!self->GetVar<bool>(u"LightningRailUsed")) {
			self->SetVar<bool>(u"LightningRailUsed", true);		
			if (self->GetVar<LWOOBJID>(u"casterID")) {
				GameMessages::SendPlayFXEffect(self->GetVar<LWOOBJID>(u"casterID"), 10151, u"create", 
				"darkitect_portal_onhit_light");
				for (auto* portal : PortalEntity) {			
					GameMessages::SendPlayFXEffect(portal->GetObjectID(), 10151, u"create", 
					"darkitect_portal_onhit_light");			
				}								
			}
			PortalHitFX(self);				
		}
	} else if (timerName == "CastIceFX") {
		if (!self->GetVar<bool>(u"IceRailUsed")) {
			self->SetVar<bool>(u"IceRailUsed", true);		
			if (self->GetVar<LWOOBJID>(u"casterID")) {
				GameMessages::SendPlayFXEffect(self->GetVar<LWOOBJID>(u"casterID"), 10150, u"create", 
				"darkitect_portal_onhit_ice");	
				for (auto* portal : PortalEntity) {			
					GameMessages::SendPlayFXEffect(portal->GetObjectID(), 10150, u"create", 
					"darkitect_portal_onhit_ice");			
				}										
			}	
			PortalHitFX(self);			
		}
	} else if (timerName == "CastFireFX") {
		if (!self->GetVar<bool>(u"FireRailUsed")) {
			self->SetVar<bool>(u"FireRailUsed", true);			
			if (self->GetVar<LWOOBJID>(u"casterID")) {
				GameMessages::SendPlayFXEffect(self->GetVar<LWOOBJID>(u"casterID"), 10149, u"create", 
				"darkitect_portal_onhit_fire");
				for (auto* portal : PortalEntity) {			
					GameMessages::SendPlayFXEffect(portal->GetObjectID(), 10149, u"create", 
					"darkitect_portal_onhit_fire");			
				}					
			}
			PortalHitFX(self);			
		}
	} else if (timerName == "SmashPortal") {			
		for (auto* portal : PortalEntity) {					
			GameMessages::SendPlayFXEffect(portal->GetObjectID(), 10143, u"death", "darkitect_portal_shrink");			
		}			
		
		for (auto* manager : BossManager) {	
			manager->NotifyObject(self, "SmashPortal");
		}			
	}
}