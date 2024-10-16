// For basic once only & timed elevator spinners

#include "ElevatorSpinner.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "DestroyableComponent.h"
#include "ProximityMonitorComponent.h"
#include "MissionComponent.h"
#include "EntityInfo.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"
#include "SoundTriggerComponent.h"

Entity* ElevatorSpinner::caster1 = nullptr;

void ElevatorSpinner::OnStartup(Entity* self) {
	
//	Force spinner to proper waypoint	
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1, 1, eMovementPlatformState::Moving);

	self->SetNetworkVar(u"bIsInUse", false);
	self->SetVar(u"bActive", true);

	SpawnLegs(self);

	self->SetProximityRadius(3.5, "spin_distance");
}

void ElevatorSpinner::SpawnLegs(Entity* self) {
	auto pos = self->GetPosition();
	auto rot = self->GetRotation();
	
	EntityInfo info{};
	info.lot = 32000;
	info.spawnerID = self->GetObjectID();
	info.rot = rot;


	const auto dir = rot.GetRightVector();
	pos.x = pos.x;
	pos.y = pos.y - 0.1;
	pos.z = pos.z;
	info.pos = pos;


	info.rot = NiQuaternion::LookAt(info.pos, self->GetPosition());
	auto* entity = Game::entityManager->CreateEntity(info);
	Game::entityManager->ConstructEntity(entity);
}

void ElevatorSpinner::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message != "NinjagoSpinEvent" || self->GetNetworkVar<bool>(u"bIsInUse")) {
		return;
	} else {		
		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();

		if (proximityMonitorComponent == nullptr || !proximityMonitorComponent->IsInProximity("spin_distance", caster->GetObjectID())) {
			return;
		}
		self->SetNetworkVar(u"bIsInUse", true);		

//		Play cinematic
		auto checkcine = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"cinematic"));
		auto checkoffcine = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"off_cinematic"));			
		if (!checkcine.empty()) {
			auto cine = self->GetVar<std::u16string>(u"cinematic");
			GameMessages::SendPlayCinematic(caster->GetObjectID(), cine, caster->GetSystemAddress());		
		}	else if (!checkoffcine.empty()) {
			auto offcine = self->GetVar<std::u16string>(u"off_cinematic");
			GameMessages::SendPlayCinematic(caster->GetObjectID(), offcine, caster->GetSystemAddress());		
		}		
			
//		End

//	Start correct flourish audio sequence for caster

		const auto AttachedPath = self->GetVar<std::u16string>(u"attached_path");
		if (AttachedPath == u"ZSpinner01") {			
			self->SetNetworkVar(u"lowmed", 1);	
		} else if (AttachedPath == u"ZSpinner12" || AttachedPath == u"ZSpinner13" ) {			
			self->SetNetworkVar(u"lowshort", 1);	
		} else if (AttachedPath == u"ZSpinner14") {			
			self->SetNetworkVar(u"lowshort2", 1);	
		} else if (AttachedPath == u"ZSpinner52" || AttachedPath == u"ZSpinner51") {			
			self->SetNetworkVar(u"midshort", 1);		
		} else if (AttachedPath == u"ZSpinner59") {			
			self->SetNetworkVar(u"midmed", 1);		
		} else if (AttachedPath == u"ZSpinner57" || AttachedPath == u"ZSpinner64") {			
			self->SetNetworkVar(u"midmed2", 1);		
		} else if (AttachedPath == u"ZSpinner58" || AttachedPath == u"ZSpinner65" || AttachedPath == u"ZSpinner50") {			
			self->SetNetworkVar(u"midshort2", 1);	
		}

		if (caster1 != caster) {
			caster1 = caster;
		}
//	End





		TriggerDrill(self);
	}
}

void ElevatorSpinner::TriggerDrill(Entity* self) {
	

//	Move spinner	
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);

//	Play anim	
	RenderComponent::PlayAnimation(self, u"up");
	
//	Ascend sfx
	GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");	
	
//	Check if timed spinner
	auto ResetTime = self->GetVar<int32_t>(u"reset_time");
	if (ResetTime >= 1) {	
		self->AddTimer("Return", ResetTime + 2.5);	
//		2.5 = rough estimate of movetime for the timed platforms using this script			
	}
	
//	Notify group that spinner deactivated
	auto DeactivatedGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"group_deactivated_event"));
	auto DeactivatedEntities = Game::entityManager->GetEntitiesInGroup(DeactivatedGroup);	
	for (auto* entity : DeactivatedEntities) {	
//		Reduce unnecessary notifies	
		if (entity->GetLOT() != 32001 && entity->GetLOT() != 32007 && entity->GetLOT() != 16894) {
			entity->NotifyObject(self, "SpinnerDeactivated");	
		}	
	}
	
//	Check for attached door indicator	
	auto DeactivatedName = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"name_deactivated_event"));
	if (DeactivatedName == "spinnerdown1" || DeactivatedName == "spinnerdown2" || DeactivatedName == "spinnerdown3" ||
	DeactivatedName == "spinnerdown4" || DeactivatedName == "spinnerdown5" || DeactivatedName == "spinnerdown6" || 
	DeactivatedName == "spinnerdown7" || DeactivatedName == "spinnerdown8") {	
		self->AddTimer("TorchOn", 1.0f);
	}
}


void ElevatorSpinner::OnTimerDone(Entity* self, std::string timerName) {

	if (timerName == "MoveBack") {	
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1);		
	}	
	
	else if (timerName == "Return") {
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1);		
		RenderComponent::PlayAnimation(self, u"down");
		self->AddTimer("DownAnim", 0.1f);

//		Descend sfx
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{97b60c03-51f2-45b6-80cc-ccbbef0d94cf}");	
		
		self->AddTimer("Unlock", 2.5f);
	}
	else if (timerName == "Unlock") {	
		SpawnLegs(self);
		self->SetNetworkVar(u"bIsInUse", false);
		self->SetVar(u"bActive", true);		
	}
	else if (timerName == "TorchOn") {	
//	Activate torches accordingly	
//	Wont let me do it the easy way lol
		const auto DeactivatedEvent = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"name_deactivated_event"));
		const auto SpinnersGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"groupID"));
		auto groupObjs = Game::entityManager->GetEntitiesByLOT(16894);		
		if (DeactivatedEvent == "spinnerdown1") {		
			for (auto* obj : groupObjs) {
				if (obj) {
					const auto ActivatedEvent = GeneralUtils::UTF16ToWTF8(obj->GetVar<std::u16string>(u"name_activated_event"));
					if (ActivatedEvent == "spinnerdown1") {		
						obj->SetNetworkVar<bool>(u"FlameOn", true);	
					}
				}
			}
		} else if (DeactivatedEvent == "spinnerdown2") {		
			for (auto* obj : groupObjs) {
				if (obj) {
					const auto ActivatedEvent = GeneralUtils::UTF16ToWTF8(obj->GetVar<std::u16string>(u"name_activated_event"));
					if (ActivatedEvent == "spinnerdown2") {		
						obj->SetNetworkVar<bool>(u"FlameOn", true);	
					}
				}
			}
		} else if (DeactivatedEvent == "spinnerdown3") {		
			for (auto* obj : groupObjs) {
				if (obj) {
					const auto ActivatedEvent = GeneralUtils::UTF16ToWTF8(obj->GetVar<std::u16string>(u"name_activated_event"));
					if (ActivatedEvent == "spinnerdown3") {		
						obj->SetNetworkVar<bool>(u"FlameOn", true);	
					}
				}
			}
		} else if (DeactivatedEvent == "spinnerdown4") {		
			for (auto* obj : groupObjs) {
				if (obj) {
					const auto ActivatedEvent = GeneralUtils::UTF16ToWTF8(obj->GetVar<std::u16string>(u"name_activated_event"));
					if (ActivatedEvent == "spinnerdown4") {		
						obj->SetNetworkVar<bool>(u"FlameOn", true);	
					}
				}
			}
		} else if (DeactivatedEvent == "spinnerdown5") {		
			for (auto* obj : groupObjs) {
				if (obj) {
					const auto ActivatedEvent = GeneralUtils::UTF16ToWTF8(obj->GetVar<std::u16string>(u"name_activated_event"));
					if (ActivatedEvent == "spinnerdown5") {		
						obj->SetNetworkVar<bool>(u"FlameOn", true);	
					}
				}
			}
		} else if (DeactivatedEvent == "spinnerdown6") {		
			for (auto* obj : groupObjs) {
				if (obj) {
					const auto ActivatedEvent = GeneralUtils::UTF16ToWTF8(obj->GetVar<std::u16string>(u"name_activated_event"));
					if (ActivatedEvent == "spinnerdown6") {		
						obj->SetNetworkVar<bool>(u"FlameOn", true);	
					}
				}
			}
		} else if (DeactivatedEvent == "spinnerdown7") {		
			for (auto* obj : groupObjs) {
				if (obj) {
					const auto ActivatedEvent = GeneralUtils::UTF16ToWTF8(obj->GetVar<std::u16string>(u"name_activated_event"));
					if (ActivatedEvent == "spinnerdown7") {		
						obj->SetNetworkVar<bool>(u"FlameOn", true);	
					}
				}
			}
		} else if (DeactivatedEvent == "spinnerdown8") {		
			for (auto* obj : groupObjs) {
				if (obj) {
					const auto ActivatedEvent = GeneralUtils::UTF16ToWTF8(obj->GetVar<std::u16string>(u"name_activated_event"));
					if (ActivatedEvent == "spinnerdown8") {		
						obj->SetNetworkVar<bool>(u"FlameOn", true);	
					}
				}
			}
		}
	}

//Handle spinner sound orders
//	TODO: Remake onwaypointreached to handle gamemessage version of platform resync since DLU's startpathing is shitty	
//	^ All just to know when to end Movement sound loop
	else if (timerName == "MovementGUID") {
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{b91d2f01-1998-4ca3-bb5c-c25cf36c7a24}");	
		if (!self->GetNetworkVar<bool>(u"bHasArrived")) {
			self->AddTimer("MovementGUID", 1);
		}
	}	
}




