#include "AgJetEffectServer.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "SkillComponent.h"

void AgJetEffectServer::OnUse(Entity* self, Entity* user) {
	if (inUse) {
		return;
	}

	GameMessages::SendNotifyClientObject(
		self->GetObjectID(),
		u"isInUse",
		0,
		0,
		LWOOBJID_EMPTY,
		"",
		UNASSIGNED_SYSTEM_ADDRESS
	);

	inUse = true;

	auto entities = EntityManager::Instance()->GetEntitiesInGroup("Jet_FX");

	if (entities.empty()) {
		return;
	}

	auto* effect = entities[0];

	GameMessages::SendPlayFXEffect(effect, 641, u"create", "radarDish", LWOOBJID_EMPTY, 1, 1, true);

	self->AddTimer("radarDish", 2);
	self->AddTimer("CineDone", 9);
}

void AgJetEffectServer::OnRebuildComplete(Entity* self, Entity* target) {
	auto entities = EntityManager::Instance()->GetEntitiesInGroup("Jet_FX");

	if (entities.empty()) {
		return;
	}

	auto* effect = entities[0];

	auto groups = self->GetGroups();

	if (groups.empty()) {
		return;
	}

	builder = target->GetObjectID();

	const auto group = groups[0];

	GameMessages::SendPlayAnimation(effect, u"jetFX");

	self->AddTimer("PlayEffect", 2.5f);

	if (group == "Base_Radar") {
		self->AddTimer("CineDone", 5);
	}
}

void AgJetEffectServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "radarDish") {
		GameMessages::SendStopFXEffect(self, true, "radarDish");

		return;
	}

	if (timerName == "PlayEffect") {
		auto entities = EntityManager::Instance()->GetEntitiesInGroup("mortarMain");

		if (entities.empty()) {
			return;
		}

		const auto size = entities.size();

		if (size == 0) {
			return;
		}

		const auto selected = GeneralUtils::GenerateRandomNumber<int>(0, size - 1);

		auto* mortar = entities[selected];

		Game::logger->Log("AgJetEffectServer", "Mortar (%i) (&d)", mortar->GetLOT(), mortar->HasComponent(COMPONENT_TYPE_SKILL));

		mortar->SetOwnerOverride(builder);

		SkillComponent* skillComponent;
		if (!mortar->TryGetComponent(COMPONENT_TYPE_SKILL, skillComponent)) {
			return;
		}

		skillComponent->CalculateBehavior(318, 3727, LWOOBJID_EMPTY, true);

		return;
	}

	if (timerName == "CineDone") {
		GameMessages::SendNotifyClientObject(
			self->GetObjectID(),
			u"toggleInUse",
			-1,
			0,
			LWOOBJID_EMPTY,
			"",
			UNASSIGNED_SYSTEM_ADDRESS
		);

		inUse = false;
	}
}
