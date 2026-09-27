#include "ExplodingAsset.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "MissionComponent.h"
#include "SkillComponent.h"
#include "eMissionTaskType.h"
#include "CDClientManager.h"
#include "CDObjectSkillsTable.h"
#include "RenderComponent.h"
#include "TeamManager.h"
#include "ProximityMonitorComponent.h"

void ExplodingAsset::OnStartup(Entity* self) {
	self->SetProximityRadius(20.0f, "outRadius");
	self->SetVar<int32_t>(u"playersNearChest", 0);
	self->SetProximityRadius(10.0f, "crateHitters");
}

void ExplodingAsset::ProgressPlayerMissions(Entity& self, Entity& player) {
	const auto missionID = self.GetVar<int32_t>(u"missionID");
	auto achievementIDs = self.GetVarAsString(u"achieveID");
	auto* const missionComponent = player.GetComponent<MissionComponent>();
	if (missionComponent) {
		if (missionID != 0) {
			missionComponent->ForceProgressValue(missionID,
				static_cast<uint32_t>(eMissionTaskType::SCRIPT),
				self.GetLOT(), false);
		}

		if (!achievementIDs.empty()) {
			for (const auto& achievementID : GeneralUtils::SplitString(achievementIDs, u'_')) {
				const auto achievementIDInt = GeneralUtils::TryParse<int32_t>(achievementID);
				if (!achievementIDInt) continue;
				missionComponent->ForceProgressValue(achievementIDInt.value(),
					static_cast<uint32_t>(eMissionTaskType::SCRIPT),
					self.GetLOT());
			}
		}
	}
}

void ExplodingAsset::OnHit(Entity* self, Entity* attacker) {
	const auto* const proximityComponent = self->GetComponent<ProximityMonitorComponent>();
	if (!proximityComponent) return;

	// Like the live script, the asset only explodes once. Only the attacker is smashed directly, and only when
	// standing close to the asset; everyone else in range is hit by the explosion skill.
	if (!attacker || self->GetBoolean(u"bIsHit")) return;

	for (const auto objID : proximityComponent->GetProximityObjects("crateHitters")) {
		auto* const entity = Game::entityManager->GetEntity(objID);
		if (!entity || entity->GetObjectID() != attacker->GetObjectID()) continue;

		auto* const destroyable = entity->GetComponent<DestroyableComponent>();
		if (destroyable) destroyable->Smash(attacker->GetObjectID());
	}

	attacker = attacker->GetOwner();
	self->SetBoolean(u"bIsHit", true);
	self->SetOwnerOverride(attacker->GetObjectID());

	GameMessages::PlayEmbeddedEffectOnAllClientsNearObject embeddedEffect;
	embeddedEffect.target = self->GetObjectID();
	embeddedEffect.effectName = u"camshake";
	embeddedEffect.fromObjectID = self->GetObjectID();
	embeddedEffect.radius = 16;
	embeddedEffect.Send(UNASSIGNED_SYSTEM_ADDRESS);
	self->Smash(attacker->GetObjectID());

	auto* skillComponent = self->GetComponent<SkillComponent>();
	if (skillComponent != nullptr) {
		// Technically supposed to get first skill in the skill component but only 1 object in the live game used this.
		skillComponent->CalculateBehavior(147, 4721, LWOOBJID_EMPTY, true);
	}
	const auto* const team = TeamManager::Instance()->GetTeam(attacker->GetObjectID());
	// Progress all scripted missions related to this asset
	if (team) {
		for (const auto& member : team->members) {
			auto* const memberEntity = Game::entityManager->GetEntity(member);
			if (memberEntity) {
				ProgressPlayerMissions(*self, *memberEntity);
			}
		}
	} else {
		ProgressPlayerMissions(*self, *attacker);
	}
}

void ExplodingAsset::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	auto* destuctableComponent = entering->GetComponent<DestroyableComponent>();

	if (destuctableComponent == nullptr) return;

	const auto& factions = destuctableComponent->GetFactionIDs();

	if (!std::count(factions.begin(), factions.end(), 1)) return;

	if (status == "ENTER") {
		RenderComponent::PlayAnimation(self, u"bounce");
		GameMessages::PlayFXEffect(self->GetObjectID(), -1, u"anim", "bouncin").Send(UNASSIGNED_SYSTEM_ADDRESS);
		self->SetVar(u"playersNearChest", self->GetVar<int32_t>(u"playersNearChest") + 1);
	} else if (status == "LEAVE") {
		self->SetVar(u"playersNearChest", self->GetVar<int32_t>(u"playersNearChest") - 1);

		if (self->GetVar<int32_t>(u"playersNearChest") < 1) {
			RenderComponent::PlayAnimation(self, u"idle");
			GameMessages::StopFXEffect(self->GetObjectID(), true, "bouncin").Send(UNASSIGNED_SYSTEM_ADDRESS);
			self->SetVar<int32_t>(u"playersNearChest", 0);
		}
	}
}
