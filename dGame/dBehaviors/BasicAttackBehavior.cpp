#include "BasicAttackBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "dLogger.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "BehaviorContext.h"
#include "eBasicAttackSuccessTypes.h"

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

	DoHandleBehavior(context, bitStream, branch);

	bitStream->SetReadOffset(baseAddress + allocatedBits);
}

void BasicAttackBehavior::DoHandleBehavior(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* targetEntity = EntityManager::Instance()->GetEntity(branch.target);
	if (!targetEntity) {
		Game::logger->Log("BasicAttackBehavior", "Target targetEntity %i not found.", branch.target);
		return;
	}

	auto* destroyableComponent = targetEntity->GetComponent<DestroyableComponent>();
	if (!destroyableComponent) {
		Game::logger->Log("BasicAttackBehavior", "No destroyable found on the obj/lot %llu/%i", branch.target, targetEntity->GetLOT());
		return;
	}

	bool isBlocked{};
	bool isImmune{};
	bool isSuccess{};

	if (!bitStream->Read(isBlocked)) {
		Game::logger->Log("BasicAttackBehavior", "Unable to read isBlocked");
		return;
	}

	if (isBlocked) {
		destroyableComponent->SetAttacksToBlock(std::min(destroyableComponent->GetAttacksToBlock() - 1, static_cast<uint32_t>(0)));
		EntityManager::Instance()->SerializeEntity(targetEntity);
		this->m_OnFailBlocked->Handle(context, bitStream, branch);
		return;
	}

	if (!bitStream->Read(isImmune)) {
		Game::logger->Log("BasicAttackBehavior", "Unable to read isImmune");
		return;
	}

	if (isImmune) {
		this->m_OnFailImmune->Handle(context, bitStream, branch);
		return;
	}

	if (!bitStream->Read(isSuccess)) {
		Game::logger->Log("BasicAttackBehavior", "failed to read success from bitstream");
		return;
	}

	if (isSuccess) {
		uint32_t armorDamageDealt{};
		if (!bitStream->Read(armorDamageDealt)) {
			Game::logger->Log("BasicAttackBehavior", "Unable to read armorDamageDealt");
			return;
		}

		uint32_t healthDamageDealt{};
		if (!bitStream->Read(healthDamageDealt)) {
			Game::logger->Log("BasicAttackBehavior", "Unable to read healthDamageDealt");
			return;
		}

		uint32_t totalDamageDealt = armorDamageDealt + healthDamageDealt;

		// A value that's too large may be a cheating attempt, so we set it to MIN
		if (totalDamageDealt > this->m_MaxDamage) {
			totalDamageDealt = this->m_MinDamage;
		}

		bool died{};
		if (!bitStream->Read(died)) {
			Game::logger->Log("BasicAttackBehavior", "Unable to read died");
			return;
		}
		auto previousArmor = destroyableComponent->GetArmor();
		auto previousHealth = destroyableComponent->GetHealth();
		PlayFx(u"onhit", targetEntity->GetObjectID());
		destroyableComponent->Damage(totalDamageDealt, context->originator, context->skillID);
	}

	uint8_t successState{};
	if (!bitStream->Read(successState)) {
		Game::logger->Log("BasicAttackBehavior", "Unable to read success state");
		return;
	}

	switch (static_cast<eBasicAttackSuccessTypes>(successState)) {
	case eBasicAttackSuccessTypes::SUCCESS:
		this->m_OnSuccess->Handle(context, bitStream, branch);
		break;
	case eBasicAttackSuccessTypes::FAILARMOR:
		this->m_OnFailArmor->Handle(context, bitStream, branch);
		break;
	default:
		if (static_cast<eBasicAttackSuccessTypes>(successState) != eBasicAttackSuccessTypes::FAILIMMUNE) {
			Game::logger->Log("BasicAttackBehavior", "Unknown success state (%i)!", successState);
			return;
		}
		this->m_OnFailImmune->Handle(context, bitStream, branch);
		break;
	}
}

void BasicAttackBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	bitStream->AlignWriteToByteBoundary();

	const auto allocatedAddress = bitStream->GetWriteOffset();

	bitStream->Write<uint16_t>(0);

	const auto startAddress = bitStream->GetWriteOffset();

	DoBehaviorCalculation(context, bitStream, branch);

	const auto endAddress = bitStream->GetWriteOffset();
	const uint16_t allocate = endAddress - startAddress + 1;

	bitStream->SetWriteOffset(allocatedAddress);
	bitStream->Write(allocate);
	bitStream->SetWriteOffset(startAddress + allocate);
}

void BasicAttackBehavior::DoBehaviorCalculation(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* targetEntity = EntityManager::Instance()->GetEntity(branch.target);
	if (!targetEntity) {
		Game::logger->Log("BasicAttackBehavior", "Target entity %llu is null!", branch.target);
		return;
	}

	auto* destroyableComponent = targetEntity->GetComponent<DestroyableComponent>();
	if (!destroyableComponent || !destroyableComponent->GetParent()) {
		Game::logger->Log("BasicAttackBehavior", "No destroyable component on %llu", branch.target);
		return;
	}

	const bool isBlocking = destroyableComponent->GetAttacksToBlock() > 0;

	bitStream->Write(isBlocking);

	if (isBlocking) {
		destroyableComponent->SetAttacksToBlock(destroyableComponent->GetAttacksToBlock() - 1);
		EntityManager::Instance()->SerializeEntity(targetEntity);
		this->m_OnFailBlocked->Calculate(context, bitStream, branch);
		return;
	}

	const bool isImmune = destroyableComponent->IsImmune();

	bitStream->Write(isImmune);

	if (isImmune) {
		this->m_OnFailImmune->Calculate(context, bitStream, branch);
		return;
	}

	bool isSuccess = false;
	const uint32_t previousHealth = destroyableComponent->GetHealth();
	const uint32_t previousArmor = destroyableComponent->GetArmor();

	const auto damage = this->m_MinDamage;

	PlayFx(u"onhit", targetEntity->GetObjectID(), 1);
	destroyableComponent->Damage(damage, context->originator, context->skillID, false);
	context->ScheduleUpdate(branch.target);

	const uint32_t armorDamageDealt = previousArmor - destroyableComponent->GetArmor();
	const uint32_t healthDamageDealt = previousHealth - destroyableComponent->GetHealth();
	isSuccess = armorDamageDealt > 0 || healthDamageDealt > 0 || (armorDamageDealt + healthDamageDealt) > 0;

	bitStream->Write(isSuccess);

	eBasicAttackSuccessTypes successState = eBasicAttackSuccessTypes::FAILIMMUNE;
	if (isSuccess) {
		if (healthDamageDealt >= 1) {
			successState = eBasicAttackSuccessTypes::SUCCESS;
		} else if (armorDamageDealt >= 1) {
			successState = this->m_OnFailArmor->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY ? eBasicAttackSuccessTypes::FAILIMMUNE : eBasicAttackSuccessTypes::FAILARMOR;
		}

		bitStream->Write(armorDamageDealt);
		bitStream->Write(healthDamageDealt);
		bitStream->Write(targetEntity->GetIsDead());
	}

	bitStream->Write(successState);

	switch (static_cast<eBasicAttackSuccessTypes>(successState)) {
	case eBasicAttackSuccessTypes::SUCCESS:
		this->m_OnSuccess->Calculate(context, bitStream, branch);
		break;
	case eBasicAttackSuccessTypes::FAILARMOR:
		this->m_OnFailArmor->Calculate(context, bitStream, branch);
		break;
	default:
		if (static_cast<eBasicAttackSuccessTypes>(successState) != eBasicAttackSuccessTypes::FAILIMMUNE) {
			Game::logger->Log("BasicAttackBehavior", "Unknown success state (%i)!", successState);
			break;
		}
		this->m_OnFailImmune->Calculate(context, bitStream, branch);
		break;
	}
}

void BasicAttackBehavior::Load() {
	this->m_MinDamage = GetInt("min damage");
	if (this->m_MinDamage == 0) this->m_MinDamage = 1;

	this->m_MaxDamage = GetInt("max damage");
	if (this->m_MaxDamage == 0) this->m_MaxDamage = 1;

	// The client sets the minimum damage to maximum, so we'll do the same.  These are usually the same value anyways.
	if (this->m_MinDamage < this->m_MaxDamage) this->m_MinDamage = this->m_MaxDamage;

	this->m_OnSuccess = GetAction("on_success");

	this->m_OnFailArmor = GetAction("on_fail_armor");

	this->m_OnFailImmune = GetAction("on_fail_immune");

	this->m_OnFailBlocked = GetAction("on_fail_blocked");
}
