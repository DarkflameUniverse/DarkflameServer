#include "NjElementDragonPetServer.h"
#include "Entity.h"

void NjElementDragonPetServer::SetVariables(Entity* self) {
//	Fire pet	
	if (self->GetLOT() == 16257) {
		self->SetVar<LOT>(u"petLOT", 16741);
		self->SetVar<std::string>(u"petType", "zfirepet");
		self->SetVar<uint32_t>(u"maxPets", 3);
		self->SetVar<std::u16string>(u"spawnAnim", u"spawn");
		self->SetVar<std::u16string>(u"spawnCinematic", u"ZFirePetSpawn");
	} 
//	Ice pet		
	else if (self->GetLOT() == 16259) {
		self->SetVar<LOT>(u"petLOT", 16479);
		self->SetVar<std::string>(u"petType", "zzicepet");
		self->SetVar<uint32_t>(u"maxPets", 3);
		self->SetVar<std::u16string>(u"spawnAnim", u"spawn");
		self->SetVar<std::u16string>(u"spawnCinematic", u"ZZIcePetSpawn");		
	} 
//	Lightning pet		
	else if (self->GetLOT() == 16260) {
		self->SetVar<LOT>(u"petLOT", 16740);
		self->SetVar<std::string>(u"petType", "litngpet");
		self->SetVar<uint32_t>(u"maxPets", 3);
		self->SetVar<std::u16string>(u"spawnAnim", u"spawn");
		self->SetVar<std::u16string>(u"spawnCinematic", u"LitngPetSpawn");
	}	
}
