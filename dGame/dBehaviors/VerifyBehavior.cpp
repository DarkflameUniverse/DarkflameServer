#include "VerifyBehavior.h"
#include "BehaviorBranchContext.h"
#include "EntityManager.h"
#include "NiPoint3.h"
#include "BehaviorContext.h"
#include "Game.h"
#include "dLogger.h"


void VerifyBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* entity = EntityManager::Instance()->GetEntity(branch.target);

	auto success = true;

	if (entity == nullptr) {
		success = false;
	} else if (this->m_rangeCheck) {
		auto* self = EntityManager::Instance()->GetEntity(context->originator);

		if (self == nullptr) {
			Game::logger->Log("VerifyBehavior", "Invalid self for (%llu)", context->originator);

			return;
		}

		const auto distance = Vector3::DistanceSquared(self->GetPosition(), entity->GetPosition());

		if (distance > this->m_range * this->m_range) {
			success = false;
		}
	} else if (this->m_blockCheck) {
		// TODO
	}

	if (branch.target != LWOOBJID_EMPTY && branch.target != context->originator) {
		bitStream->Write(success);

		if (success) {
			bitStream->Write<uint32_t>(1);
			bitStream->Write0();
			bitStream->Write0();
		}
	}

	if (!success) {
		branch.target = LWOOBJID_EMPTY;
	}

	m_action->Calculate(context, bitStream, branch);
}

void VerifyBehavior::Load() {
	this->m_rangeCheck = GetBoolean("check_range");

	this->m_blockCheck = GetBoolean("check blocking");

	this->m_action = GetAction("action");

	this->m_range = GetFloat("range");
}
