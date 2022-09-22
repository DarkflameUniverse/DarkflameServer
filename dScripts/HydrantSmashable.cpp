#include "HydrantSmashable.h"
#include "EntityManager.h"
#include "GeneralUtils.h"

void HydrantSmashable::OnDie(Entity* self, Entity* killer) {
	const auto hydrantName = self->GetVar<std::u16string>(u"hydrant");

	LDFBaseData* data = new LDFData<std::string>(u"hydrant", GeneralUtils::UTF16ToWTF8(hydrantName));

	EntityInfo info{};
	info.lot = HYDRANT_BROKEN;
	info.pos = self->GetPosition();
	info.rot = self->GetRotation();
	info.settings = { data };
	info.spawnerID = self->GetSpawnerID();

	auto* hydrant = EntityManager::Instance()->CreateEntity(info);

	EntityManager::Instance()->ConstructEntity(hydrant);
}
