#include "ExplodingAsset.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "SkillComponent.h"
#include "eMissionTaskType.h"
#include "CDClientManager.h"
#include "CDObjectSkillsTable.h"
#include "RenderComponent.h"

//TODO: this has to be updated so that you only get killed if you're in a certain radius.
//And so that all entities in a certain radius are killed, not just the attacker.

void ExplodingAsset::OnStartup(Entity* self) {
	self->SetProximityRadius(20.0f, "outRadius");
	self->SetVar<int32_t>(u"playersNearChest", 0);
	self->SetProximityRadius(10.0f, "crateHitters");
}

void ExplodingAsset::OnHit(Entity* self, Entity* attacker) {
	std::vector<Entity*> entities;
	entities.push_back(attacker);

	if (!self->GetBoolean(u"bIsHit")) {
		for (Entity* en : entities) {
			if (en->GetObjectID() == attacker->GetObjectID()) {
				if (Vector3::DistanceSquared(en->GetPosition(), self->GetPosition()) > 10 * 10) continue;

				auto* destroyable = en->GetComponent<DestroyableComponent>();
				if (destroyable == nullptr) {
					continue;
				}

				destroyable->Smash(attacker->GetObjectID());
			}
		}
	}

	attacker = attacker->GetOwner();
	self->SetBoolean(u"bIsHit", true);
	self->SetOwnerOverride(attacker->GetObjectID());

	GameMessages::SendPlayEmbeddedEffectOnAllClientsNearObject(self, u"camshake", self->GetObjectID(), 16);
	self->Smash(attacker->GetObjectID());

	auto* skillComponent = self->GetComponent<SkillComponent>();
	if (skillComponent != nullptr) {
		// Technically supposed to get first skill in the skill component but only 1 object in the live game used this.
		skillComponent->CalculateBehavior(147, 4721, LWOOBJID_EMPTY, true);
	}

	const auto missionID = self->GetVar<int32_t>(u"missionID");
	auto achievementIDs = self->GetVar<std::u16string>(u"achieveID");

	// Progress all scripted missions related to this asset
	auto* missionComponent = attacker->GetComponent<MissionComponent>();
	if (missionComponent != nullptr) {
		if (missionID != 0) {
			missionComponent->ForceProgressValue(missionID,
				static_cast<uint32_t>(eMissionTaskType::SCRIPT),
				self->GetLOT(), false);
		}

		if (!achievementIDs.empty()) {
			for (const auto& achievementID : GeneralUtils::SplitString(achievementIDs, u'_')) {
				missionComponent->ForceProgressValue(std::stoi(GeneralUtils::UTF16ToWTF8(achievementID)),
					static_cast<uint32_t>(eMissionTaskType::SCRIPT),
					self->GetLOT());
			}
		}
	}
}

void ExplodingAsset::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	auto* destuctableComponent = entering->GetComponent<DestroyableComponent>();

	if (destuctableComponent == nullptr) return;

	const auto& factions = destuctableComponent->GetFactionIDs();

	if (!std::count(factions.begin(), factions.end(), 1)) return;

	if (status == "ENTER") {
		RenderComponent::PlayAnimation(self, u"bounce");
		GameMessages::SendPlayFXEffect(self, -1, u"anim", "bouncin", LWOOBJID_EMPTY, 1, 1, true);
		self->SetVar(u"playersNearChest", self->GetVar<int32_t>(u"playersNearChest") + 1);
	} else if (status == "LEAVE") {
		self->SetVar(u"playersNearChest", self->GetVar<int32_t>(u"playersNearChest") - 1);

		if (self->GetVar<int32_t>(u"playersNearChest") < 1) {
			RenderComponent::PlayAnimation(self, u"idle");
			GameMessages::SendStopFXEffect(self, true, "bouncin");
			self->SetVar<int32_t>(u"playersNearChest", 0);
		}
	}
}
