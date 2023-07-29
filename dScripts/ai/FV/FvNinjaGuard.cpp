#include "FvNinjaGuard.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "EntityManager.h"

void FvNinjaGuard::OnStartup(Entity* self) {
	if (self->GetLOT() == 7412) {
		m_LeftGuard = self->GetObjectID();
	} else if (self->GetLOT() == 11128) {
		m_RightGuard = self->GetObjectID();
	}
}

void FvNinjaGuard::OnEmoteReceived(Entity* self, const int32_t emote, Entity* target) {
	if (emote != 392) {
		RenderComponent::PlayAnimation(self, u"no");

		return;
	}

	RenderComponent::PlayAnimation(self, u"scared");

	if (self->GetLOT() == 7412) {
		auto* rightGuard = Game::entityManager->GetEntity(m_RightGuard);

		if (rightGuard != nullptr) {
			RenderComponent::PlayAnimation(rightGuard, u"laugh_rt");
		}
	} else if (self->GetLOT() == 11128) {
		auto* leftGuard = Game::entityManager->GetEntity(m_LeftGuard);

		if (leftGuard != nullptr) {
			RenderComponent::PlayAnimation(leftGuard, u"laugh_lt");
		}
	}
}
