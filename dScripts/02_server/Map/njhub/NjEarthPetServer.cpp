#include "NjEarthPetServer.h"
#include "PetComponent.h"

void NjEarthPetServer::OnStartup(Entity* self) {
	auto* petComponent = self->GetComponent<PetComponent>();
	if (petComponent == nullptr || petComponent->GetOwnerId() != LWOOBJID_EMPTY)
		return;

	// Removes the chocolate bars
	petComponent->SetPreconditions(const_cast<std::string&>(m_Precondition));
	PetFromObjectServer::OnStartup(self);
}
