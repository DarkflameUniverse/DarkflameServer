#include "NjFireRocksManager.h"

//	I need better movingplatform system pls :)

void NjFireRocksManager::OnStartup(Entity* self) {

//	set defaults	
	if (self->GetVar<std::u16string>(u"RockGroup") != u"LavaRocks02") {
		self->SetVar<std::u16string>(u"RockGroup", u"FireTransRocks");
	}
	if (self->GetVar<int32_t>(u"NumberOfRocks") != 2) {
		self->SetVar<int32_t>(u"NumberOfRocks", 3);
	}
}