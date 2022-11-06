#include "SpawnStegoServer.h"
#include "Entity.h"

void SpawnStegoServer::SetVariables(Entity* self) {
	self->SetVar<LOT>(u"petLOT", 12431);
	self->SetVar<std::string>(u"petType", "stego");
	self->SetVar<uint32_t>(u"maxPets", 3);
	self->SetVar<std::u16string>(u"spawnAnim", u"spawn");
	self->SetVar<std::u16string>(u"spawnCinematic", u"VenturePet");
}
