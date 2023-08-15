#include "NjIceRailActivator.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "Entity.h"
#include "RenderComponent.h"

void NjIceRailActivator::OnPlayerRailArrived(Entity* self, Entity* sender, const std::u16string& pathName,
	int32_t waypoint) {
	const auto breakPoint = self->GetVar<int32_t>(BreakpointVariable);
	if (breakPoint == waypoint) {
		const auto& blockGroup = self->GetVar<std::u16string>(BlockGroupVariable);

		for (auto* block : Game::entityManager->GetEntitiesInGroup(GeneralUtils::UTF16ToWTF8(blockGroup))) {
			RenderComponent::PlayAnimation(block, u"explode");

			const auto blockID = block->GetObjectID();

			self->AddCallbackTimer(1.0f, [self, blockID]() {
				auto* block = Game::entityManager->GetEntity(blockID);

				if (block != nullptr) {
					block->Kill(self);
				}
				});
		}
	}
}
