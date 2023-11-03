#include "AgJetEffectServer.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "eReplicaComponentType.h"
#include "RenderComponent.h"

void AgJetEffectServer::OnUse(Entity* self, Entity* user) {
	if (inUse || self->GetLOT() != 6859) return;
	GameMessages::SendNotifyClientObject(
		self->GetObjectID(), u"toggleInUse", 1, 0, LWOOBJID_EMPTY, "", UNASSIGNED_SYSTEM_ADDRESS
	);
	inUse = true;

	auto entities = Game::entityManager->GetEntitiesInGroup("Jet_FX");
	if (entities.empty()) return;
	GameMessages::SendPlayFXEffect(entities.at(0), 641, u"create", "radarDish", LWOOBJID_EMPTY, 1, 1, true);
	self->AddTimer("radarDish", 2.0f);
	self->AddTimer("PlayEffect", 2.5f);
	self->AddTimer("CineDone", 7.5f + 5.0f); // 7.5f is time the cinematic takes to play
}

void AgJetEffectServer::OnRebuildComplete(Entity* self, Entity* target) {
	if (self->GetLOT() != 6209) return;
	auto entities = Game::entityManager->GetEntitiesInGroup("Jet_FX");
	if (entities.empty()) return;
	RenderComponent::PlayAnimation(entities.at(0), u"jetFX");

	// So we can give kill credit to person who build this
	builder = target->GetObjectID();

	auto groups = self->GetGroups();
	if (!groups.empty() && groups.at(0) == "Base_Radar") {
		self->AddTimer("PlayEffect", 2.5f);
		self->AddTimer("CineDone", 7.5f + 5.0f); // 7.5f is time the cinematic takes to play
	}
}

void AgJetEffectServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "radarDish") {
		GameMessages::SendStopFXEffect(self, true, "radarDish");
	} else if (timerName == "PlayEffect") {
		auto entities = Game::entityManager->GetEntitiesInGroup("mortarMain");
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
