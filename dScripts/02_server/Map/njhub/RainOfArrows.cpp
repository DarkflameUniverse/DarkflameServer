#include "RainOfArrows.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "GameMessages.h"

void RainOfArrows::OnStartup(Entity* self) {

}

void RainOfArrows::OnRebuildComplete(Entity* self, Entity* target) {
	auto myPos = self->GetPosition();
	auto myRot = self->GetRotation();

	EntityInfo info;
	info.lot = m_ArrowFXObject;
	info.pos = myPos;
	info.rot = myRot;
	info.spawnerID = self->GetObjectID();

	auto* entity = EntityManager::Instance()->CreateEntity(info);

	EntityManager::Instance()->ConstructEntity(entity);

	self->SetVar<LWOOBJID>(u"ChildFX", entity->GetObjectID());
	self->SetVar<LWOOBJID>(u"playerID", target->GetObjectID());

	self->AddTimer("ArrowsIncoming", m_ArrowDelay);
	self->AddTimer("PlayArrowSound", m_ArrowDelay - 4);
}

void RainOfArrows::OnTimerDone(Entity* self, std::string timerName) {
	auto* child = EntityManager::Instance()->GetEntity(
		self->GetVar<LWOOBJID>(u"ChildFX")
	);

	auto* player = EntityManager::Instance()->GetEntity(
		self->GetVar<LWOOBJID>(u"playerID")
	);

	if (timerName == "ArrowsIncoming") {
		if (child == nullptr) {
			return;
		}

		auto* skillComponent = child->GetComponent<SkillComponent>();

		if (skillComponent == nullptr) {
			return;
		}

		skillComponent->CalculateBehavior(
			m_ArrowSkill,
			m_ArrowBehavior,
			LWOOBJID_EMPTY,
			true,
			false,
			player != nullptr ? player->GetObjectID() : LWOOBJID_EMPTY
		);

		self->AddTimer("FireSkill", 0.7f);
	} else if (timerName == "PlayArrowSound") {
		GameMessages::SendPlayNDAudioEmitter(self, UNASSIGNED_SYSTEM_ADDRESS, m_ArrowsGUID);
	} else if (timerName == "FireSkill") {
		if (child != nullptr) {
			child->Smash();
		}

		self->Smash(player != nullptr ? player->GetObjectID() : LWOOBJID_EMPTY);
	}
}
