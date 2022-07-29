#include "NjIceRailActivator.h"
#include "EntityManager.h"
#include "GameMessages.h"

void NjIceRailActivator::OnPlayerRailArrived(Entity* self, Entity* sender, const std::u16string& pathName,
	int32_t waypoint) {
	const auto breakPoint = self->GetVar<int32_t>(BreakpointVariable);
	if (breakPoint == waypoint) {
		const auto& blockGroup = self->GetVar<std::u16string>(BlockGroupVariable);

		for (auto* block : EntityManager::Instance()->GetEntitiesInGroup(GeneralUtils::UTF16ToWTF8(blockGroup))) {
			GameMessages::SendPlayAnimation(block, u"explode");

			const auto blockID = block->GetObjectID();

			self->AddCallbackTimer(1.0f, [self, blockID]() {
				auto* block = EntityManager::Instance()->GetEntity(blockID);

				if (block != nullptr) {
					block->Kill(self);
				}
				});
		}
	}
}
