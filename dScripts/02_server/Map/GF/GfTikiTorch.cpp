#include "GfTikiTorch.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "MissionComponent.h"
#include "RenderComponent.h"

void GfTikiTorch::OnStartup(Entity* self) {
	LightTorch(self);
}

void GfTikiTorch::OnUse(Entity* self, Entity* killer) {
	if (self->GetBoolean(u"isInUse")) {
		self->SetBoolean(u"isInUse", false);
		return;
	}

	GameMessages::SendPlayAnimation(self, u"interact");
	self->SetI64(u"userID", killer->GetObjectID());

	for (int i = 0; i < m_numspawn; i++) {
		GameMessages::SendDropClientLoot(killer, self->GetObjectID(), 935, 0, self->GetPosition());
	}

	self->AddTimer("InteractionCooldown", 4);
}

void GfTikiTorch::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "Relight") {
		LightTorch(self);
	} else if (timerName == "InteractionCooldown") {
		Entity* player = EntityManager::Instance()->GetEntity(self->GetI64(u"userID"));

		if (player != nullptr && player->GetCharacter()) {
			GameMessages::SendTerminateInteraction(player->GetObjectID(), FROM_INTERACTION, self->GetObjectID());
		}

		self->SetBoolean(u"isInUse", false);

		self->SetI64(u"userID", 0);
	}
}

void GfTikiTorch::LightTorch(Entity* self) {
	auto* renderComponent = static_cast<RenderComponent*>(self->GetComponent(COMPONENT_TYPE_RENDER));
	if (renderComponent == nullptr)
		return;

	self->SetBoolean(u"isInUse", false);

	renderComponent->PlayEffect(611, u"fire", "tikitorch");
	self->SetBoolean(u"isBurning", true);
}

void GfTikiTorch::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (self->GetBoolean(u"isBurning") && message == "waterspray") {
		GameMessages::SendPlayAnimation(self, u"water");

		auto* renderComponent = self->GetComponent<RenderComponent>();
		if (renderComponent != nullptr) {
			renderComponent->StopEffect("tikitorch");
			renderComponent->PlayEffect(611, u"water", "water");
			renderComponent->PlayEffect(611, u"steam", "steam");
		}

		auto* casterMissionComponent = caster->GetComponent<MissionComponent>();
		if (casterMissionComponent != nullptr) {
			for (const auto missionID : m_missions) {
				casterMissionComponent->ForceProgressTaskType(missionID, static_cast<uint32_t>(MissionTaskType::MISSION_TASK_TYPE_SCRIPT), 1);
			}
		}

		self->AddTimer("Relight", 7.0f);
		self->SetBoolean(u"isBurning", false);
	}
}
