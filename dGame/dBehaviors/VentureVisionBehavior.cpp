#include "VentureVisionBehavior.h"
#include "BehaviorBranchContext.h"
#include "CharacterComponent.h"
#include "BehaviorContext.h"

void VentureVisionBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {

	const auto targetEntity = EntityManager::Instance()->GetEntity(branch.target);

	if (targetEntity) {
		auto characterComponent = targetEntity->GetComponent<CharacterComponent>();

		if (characterComponent) {
			if (m_show_collectibles) characterComponent->AddVentureVisionEffect(m_ShowCollectibles);
			if (m_show_minibosses) characterComponent->AddVentureVisionEffect(m_ShowMiniBosses);
			if (m_show_pet_digs) characterComponent->AddVentureVisionEffect(m_ShowPetDigs);
		}

		if (branch.duration > 0) context->RegisterTimerBehavior(this, branch);
	}
}

void VentureVisionBehavior::UnCast(BehaviorContext* context, BehaviorBranchContext branch) {
	const auto targetEntity = EntityManager::Instance()->GetEntity(branch.target);

	if (targetEntity) {
		auto characterComponent = targetEntity->GetComponent<CharacterComponent>();

		if (characterComponent) {
			if (m_show_collectibles) characterComponent->RemoveVentureVisionEffect(m_ShowCollectibles);
			if (m_show_minibosses) characterComponent->RemoveVentureVisionEffect(m_ShowMiniBosses);
			if (m_show_pet_digs) characterComponent->RemoveVentureVisionEffect(m_ShowPetDigs);
		}
	}
}

void VentureVisionBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, LWOOBJID second) {
	UnCast(context, branch);
}

void VentureVisionBehavior::Load() {
	this->m_show_pet_digs = GetBoolean("show_pet_digs");

	this->m_show_minibosses = GetBoolean("show_minibosses");

	this->m_show_collectibles = GetBoolean("show_collectibles");
}
