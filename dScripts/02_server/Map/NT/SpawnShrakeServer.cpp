#include "SpawnShrakeServer.h"
#include "Entity.h"

void SpawnShrakeServer::SetVariables(Entity* self) {
	self->SetVar<LOT>(u"petLOT", 12434);
	self->SetVar<std::string>(u"petType", "shrake");
	self->SetVar<uint32_t>(u"maxPets", 3);
	self->SetVar<std::u16string>(u"spawnAnim", u"mf_u_g_TT_spawn-1");
	self->SetVar<std::u16string>(u"spawnCinematic", u"ParadoxPet");
}
