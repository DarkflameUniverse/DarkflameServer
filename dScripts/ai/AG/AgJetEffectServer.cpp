#include "AgJetEffectServer.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "eReplicaComponentType.h"

void AgJetEffectServer::OnUse(Entity* self, Entity* user) {
	if (!inUse || self->GetLOT() == 6859) {
		GameMessages::SendNotifyClientObject(
			self->GetObjectID(), u"toggleInUse", 1, 0, LWOOBJID_EMPTY, "", UNASSIGNED_SYSTEM_ADDRESS
		);
		inUse = true;

		auto entities = EntityManager::Instance()->GetEntitiesInGroup("Jet_FX");
		if (entities.empty()) return;
		GameMessages::SendPlayFXEffect(entities.at(0), 641, u"create", "radarDish", LWOOBJID_EMPTY, 1, 1, true);
		self->AddTimer("radarDish", 2);
		self->AddTimer("PlayEffect", 2.5);
		self->AddTimer("CineDone", 7.5 + 5);
	}
}

void AgJetEffectServer::OnRebuildComplete(Entity* self, Entity* target) {
	if (self->GetLOT() != 6209) return;
	auto entities = EntityManager::Instance()->GetEntitiesInGroup("Jet_FX");
	if (entities.empty()) return;
	GameMessages::SendPlayAnimation(entities.at(0), u"jetFX");

	// So we can give kill credit to person who build this
	builder = target->GetObjectID();

	auto groups = self->GetGroups();
	if (!groups.empty() && groups.at(0) == "Base_Radar") {
		self->AddTimer("PlayEffect", 2.5f);
		self->AddTimer("CineDone", 7.5 + 5);
	}
}

void AgJetEffectServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "radarDish") {
		GameMessages::SendStopFXEffect(self, true, "radarDish");
	} else if (timerName == "PlayEffect") {
		auto entities = EntityManager::Instance()->GetEntitiesInGroup("mortarMain");
		if (entities.empty()) return;

		const auto selected = GeneralUtils::GenerateRandomNumber<int>(0, entities.size() - 1);
		auto* mortar = entities.at(selected);

		// so we give proper credit to the builder for the kills from this skill
		mortar->SetOwnerOverride(builder);

		auto* skillComponent = mortar->GetComponent<SkillComponent>();
		if (skillComponent) skillComponent->CastSkill(318);
	} else if (timerName == "CineDone") {
		GameMessages::SendNotifyClientObject(
			self->GetObjectID(), u"toggleInUse", -1, 0, LWOOBJID_EMPTY, "", UNASSIGNED_SYSTEM_ADDRESS
		);
		inUse = false;
	}
}
