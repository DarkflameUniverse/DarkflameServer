#include "ZoneSGServer.h"
#include "EntityManager.h"

void ZoneSGServer::OnStartup(Entity* self) {
	const auto cannons = EntityManager::Instance()->GetEntitiesByLOT(1864);
	for (const auto& cannon : cannons)
		self->SetVar<LWOOBJID>(CannonIDVariable, cannon->GetObjectID());
}

void ZoneSGServer::OnActivityStateChangeRequest(Entity* self, const LWOOBJID senderID, const int32_t value1,
	const int32_t value2, const std::u16string& stringValue) {

	auto* cannon = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(CannonIDVariable));
	if (cannon != nullptr) {
		cannon->OnActivityStateChangeRequest(senderID, value1, value2, stringValue);
	}
}

void ZoneSGServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {

	auto* cannon = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(CannonIDVariable));
	if (cannon != nullptr) {
		cannon->OnFireEventServerSide(sender, args, param1, param2, param3);
	}
}
