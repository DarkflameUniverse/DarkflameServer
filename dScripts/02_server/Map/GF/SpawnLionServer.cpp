#include "SpawnLionServer.h"
#include "Entity.h"

void SpawnLionServer::SetVariables(Entity* self) {
	self->SetVar<LOT>(u"petLOT", 3520);
	self->SetVar<std::string>(u"petType", "lion");
	self->SetVar<uint32_t>(u"maxPets", 5);
	self->SetVar<std::u16string>(u"spawnAnim", u"spawn-lion");
	self->SetVar<std::u16string>(u"spawnCinematic", u"Lion_spawn");
}
