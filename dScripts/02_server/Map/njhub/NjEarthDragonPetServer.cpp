#include "NjEarthDragonPetServer.h"
#include "Entity.h"

void NjEarthDragonPetServer::SetVariables(Entity* self) {
	self->SetVar<LOT>(u"petLOT", 16210);
	self->SetVar<std::string>(u"petType", "earthpet");
	self->SetVar<uint32_t>(u"maxPets", 3);
	self->SetVar<std::u16string>(u"spawnAnim", u"spawn");
	self->SetVar<std::u16string>(u"spawnCinematic", u"EarthPetSpawn");
}
