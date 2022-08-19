#include "BasicAttackBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "dLogger.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "BehaviorContext.h"


void BasicAttackBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	if (context->unmanaged) {
		auto* entity = EntityManager::Instance()->GetEntity(branch.target);

		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();
		if (destroyableComponent != nullptr) {
			PlayFx(u"onhit", entity->GetObjectID());
			destroyableComponent->Damage(this->m_maxDamage, context->originator, context->skillID);
		}

		this->m_onSuccess->Handle(context, bitStream, branch);

		return;
	}

	bitStream->AlignReadToByteBoundary();

	uint16_t allocatedBits;
	bitStream->Read(allocatedBits);

	const auto baseAddress = bitStream->GetReadOffset();
	if (bitStream->ReadBit()) { // Blocked
		return;
	}

	if (bitStream->ReadBit()) { // Immune
		return;
	}

	if (bitStream->ReadBit()) { // Success
		uint32_t unknown;
		bitStream->Read(unknown);

		uint32_t damageDealt;
		bitStream->Read(damageDealt);

		// A value that's too large may be a cheating attempt, so we set it to MIN too
		if (damageDealt > this->m_maxDamage || damageDealt < this->m_minDamage) {
			damageDealt = this->m_minDamage;
		}

		auto* entity = EntityManager::Instance()->GetEntity(branch.target);
		bool died;
		bitStream->Read(died);

		if (entity != nullptr) {
			auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();
			if (destroyableComponent != nullptr) {
				PlayFx(u"onhit", entity->GetObjectID());
				destroyableComponent->Damage(damageDealt, context->originator, context->skillID);
			}
		}
	}

	uint8_t successState;
	bitStream->Read(successState);

	switch (successState) {
	case 1:
		this->m_onSuccess->Handle(context, bitStream, branch);
		break;
	default:
		Game::logger->Log("BasicAttackBehavior", "Unknown success state (%i)!", successState);
		break;
	}

	bitStream->SetReadOffset(baseAddress + allocatedBits);
}

void BasicAttackBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* self = EntityManager::Instance()->GetEntity(context->originator);
	if (self == nullptr) {
		Game::logger->Log("BasicAttackBehavior", "Invalid self entity (%llu)!", context->originator);
		return;
	}

	bitStream->AlignWriteToByteBoundary();

	const auto allocatedAddress = bitStream->GetWriteOffset();

	bitStream->Write(uint16_t(0));

	const auto startAddress = bitStream->GetWriteOffset();

	bitStream->Write0(); // Blocked
	bitStream->Write0(); // Immune
	bitStream->Write1(); // Success

	if (true) {
		uint32_t unknown3 = 0;
		bitStream->Write(unknown3);

		auto damage = this->m_minDamage;
		auto* entity = EntityManager::Instance()->GetEntity(branch.target);

		if (entity == nullptr) {
			damage = 0;
			bitStream->Write(damage);
			bitStream->Write(false);
		} else {
			bitStream->Write(damage);
			bitStream->Write(true);

			auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();
			if (damage != 0 && destroyableComponent != nullptr) {
				PlayFx(u"onhit", entity->GetObjectID(), 1);
				destroyableComponent->Damage(damage, context->originator, context->skillID, false);
				context->ScheduleUpdate(branch.target);
			}
		}
	}

	uint8_t successState = 1;
	bitStream->Write(successState);

	switch (successState) {
	case 1:
		this->m_onSuccess->Calculate(context, bitStream, branch);
		break;
	default:
		Game::logger->Log("BasicAttackBehavior", "Unknown success state (%i)!", successState);
		break;
	}

	const auto endAddress = bitStream->GetWriteOffset();
	const uint16_t allocate = endAddress - startAddress + 1;

	bitStream->SetWriteOffset(allocatedAddress);
	bitStream->Write(allocate);
	bitStream->SetWriteOffset(startAddress + allocate);
}

void BasicAttackBehavior::Load() {
	this->m_minDamage = GetInt("min damage");
	if (this->m_minDamage == 0) this->m_minDamage = 1;

	this->m_maxDamage = GetInt("max damage");
	if (this->m_maxDamage == 0) this->m_maxDamage = 1;

	this->m_onSuccess = GetAction("on_success");
}
