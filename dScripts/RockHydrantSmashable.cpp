#include "RockHydrantSmashable.h"
#include "EntityManager.h"
#include "GeneralUtils.h"

void RockHydrantSmashable::OnDie(Entity* self, Entity* killer)
{
	const auto hydrantName = self->GetVar<std::u16string>(u"hydrant");

	LDFBaseData* data = new LDFData<std::string>(u"hydrant", GeneralUtils::UTF16ToWTF8(hydrantName));

	EntityInfo info {};
	info.lot = 12293;
	info.pos = self->GetPosition();
	info.rot = self->GetRotation();
	info.settings = {data};
	info.spawnerID = self->GetSpawnerID();

	Game::logger->Log("RockHydrantBroken", "Rock Hydrant spawned (%s)\n", data->GetString().c_str());

    auto* hydrant = EntityManager::Instance()->CreateEntity(info);

    EntityManager::Instance()->ConstructEntity(hydrant);
}
