#include "BaseWavesGenericEnemy.h"
#include "dZoneManager.h"

void BaseWavesGenericEnemy::OnStartup(Entity* self) {
	self->SetNetworkVar<uint32_t>(u"points", GetPoints());
}

void BaseWavesGenericEnemy::OnDie(Entity* self, Entity* killer) {
	auto* zoneControlObject = dZoneManager::Instance()->GetZoneControlObject();
	if (zoneControlObject != nullptr) {
		zoneControlObject->OnFireEventServerSide(killer, "Survival_Update", GetPoints());
	}
}
