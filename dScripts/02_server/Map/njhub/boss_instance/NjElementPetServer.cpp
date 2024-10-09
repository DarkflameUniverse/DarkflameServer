#include "NjElementPetServer.h"
#include "PetComponent.h"

void NjElementPetServer::OnStartup(Entity* self) {
	auto* petComponent = self->GetComponent<PetComponent>();
	if (petComponent == nullptr || petComponent->GetOwnerId() != LWOOBJID_EMPTY)
		return;
//	Fire
	if (self->GetLOT() == 16527 || 16741) {
		petComponent->SetPreconditions(const_cast<std::string&>(m_Precondition_Fire));
	}
//	Ice	
	else if (self->GetLOT() == 16479) {
		petComponent->SetPreconditions(const_cast<std::string&>(m_Precondition_Ice));	
	}
//	Lightning	
	else if (self->GetLOT() == 16740) {
		petComponent->SetPreconditions(const_cast<std::string&>(m_Precondition_Lightning));
	}
	
	PetFromObjectServer::OnStartup(self);
}
