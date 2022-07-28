#include "SpawnSaberCatServer.h"
#include "Entity.h"

void SpawnSaberCatServer::SetVariables(Entity* self) {
	self->SetVar<LOT>(u"petLOT", 12432);
	self->SetVar<std::string>(u"petType", "sabercat");
	self->SetVar<uint32_t>(u"maxPets", 3);
	self->SetVar<std::u16string>(u"spawnAnim", u"pq_m_drop-down");
	self->SetVar<std::u16string>(u"spawnCinematic", u"AssemblyPet");
}
