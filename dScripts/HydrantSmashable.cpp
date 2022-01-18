#include "HydrantSmashable.h"
#include "EntityManager.h"
#include "GeneralUtils.h"

void HydrantSmashable::OnDie(Entity* self, Entity* killer) 
{
    const auto hydrantName = self->GetVar<std::u16string>(u"hydrant");

    LDFBaseData* data = new LDFData<std::string>(u"hydrant", GeneralUtils::UTF16ToUTF8(hydrantName));

    EntityInfo info {};
    info.lot = 7328;
    info.pos = self->GetPosition();
    info.rot = self->GetRotation();
    info.settings = {data};
    info.spawnerID = self->GetSpawnerID();
    
    Game::logger->Log("HydrantBroken", "Hydrant spawned (%s)\n", data->GetString().c_str());

    auto* hydrant = EntityManager::Instance()->CreateEntity(info);

    EntityManager::Instance()->ConstructEntity(hydrant);
}
