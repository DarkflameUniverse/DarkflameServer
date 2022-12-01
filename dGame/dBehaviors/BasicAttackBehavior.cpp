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
			destroyableComponent->Damage(this->m_MaxDamage, context->originator, context->skillID);
		}

		this->m_OnSuccess->Handle(context, bitStream, branch);

		return;
	}

	bitStream->AlignReadToByteBoundary();

	uint16_t allocatedBits{};
	if (!bitStream->Read(allocatedBits) || allocatedBits == 0) {
		Game::logger->LogDebug("BasicAttackBehavior", "No allocated bits");
		return;
	}
	Game::logger->LogDebug("BasicAttackBehavior", "Number of allocated bits %i", allocatedBits);
	const auto baseAddress = bitStream->GetReadOffset();
	bool isBlocked{};
	bool isImmune{};
	bool isSuccess{};

	if (!bitStream->Read(isBlocked)) {
		Game::logger->LogDebug("BasicAttackBehavior", "Unable to read isBlocked");
		return;
	}

	if (isBlocked) return;

	if (!bitStream->Read(isImmune)) {
		Game::logger->LogDebug("BasicAttackBehavior", "Unable to read isImmune");
		return;
	}

	if (isImmune) return;

	if (bitStream->Read(isSuccess) && isSuccess) { // Success
		uint32_t unknown{};
		if (!bitStream->Read(unknown)) {
			Game::logger->LogDebug("BasicAttackBehavior", "Unable to read unknown");
			return;
		}

		uint32_t damageDealt{};
		if (!bitStream->Read(damageDealt)) {
			Game::logger->LogDebug("BasicAttackBehavior", "Unable to read damageDealt");
			return;
		}

		// A value that's too large may be a cheating attempt, so we set it to MIN too
		if (damageDealt > this->m_MaxDamage || damageDealt < this->m_MinDamage) {
			damageDealt = this->m_MinDamage;
		}

		auto* entity = EntityManager::Instance()->GetEntity(branch.target);
		bool died{};
		if (!bitStream->Read(died)) {
			Game::logger->LogDebug("BasicAttackBehavior", "Unable to read died");
			return;
		}

		if (entity != nullptr) {
			auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();
			if (destroyableComponent != nullptr) {
				PlayFx(u"onhit", entity->GetObjectID());
				destroyableComponent->Damage(damageDealt, context->originator, context->skillID);
			}
		}
	}

	uint8_t successState{};
	if (!bitStream->Read(successState)) {
		Game::logger->LogDebug("BasicAttackBehavior", "Unable to read success state");
		return;
	}

	switch (successState) {
	case 1:
		this->m_OnSuccess->Handle(context, bitStream, branch);
		break;
	case 2:
		this->m_OnFailArmor->Handle(context, bitStream, branch);
		break;
	default:
		Game::logger->LogDebug("BasicAttackBehavior", "Unknown success state (%i)!", successState);
		break;
	}

	bitStream->SetReadOffset(baseAddress + allocatedBits);
}

void BasicAttackBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* self = EntityManager::Instance()->GetEntity(context->originator);
	if (self == nullptr) {
		Game::logger->LogDebug("BasicAttackBehavior", "Invalid self entity (%llu)!", context->originator);
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

		auto damage = this->m_MinDamage;
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
		this->m_OnSuccess->Calculate(context, bitStream, branch);
		break;
	default:
		Game::logger->LogDebug("BasicAttackBehavior", "Unknown success state (%i)!", successState);
		break;
	}

	const auto endAddress = bitStream->GetWriteOffset();
	const uint16_t allocate = endAddress - startAddress + 1;

	bitStream->SetWriteOffset(allocatedAddress);
	bitStream->Write(allocate);
	bitStream->SetWriteOffset(startAddress + allocate);
}

void BasicAttackBehavior::Load() {
	this->m_MinDamage = GetInt("min damage");
	if (this->m_MinDamage == 0) this->m_MinDamage = 1;

	this->m_MaxDamage = GetInt("max damage");
	if (this->m_MaxDamage == 0) this->m_MaxDamage = 1;

	this->m_OnSuccess = GetAction("on_success");

	this->m_OnFailArmor = GetAction("on_fail_armor");
}
