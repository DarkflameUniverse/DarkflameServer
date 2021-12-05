#include "RaceImaginePowerup.h"
#include "DestroyableComponent.h"
#include "PossessorComponent.h"
#include "EntityManager.h"
#include "CharacterComponent.h"
#include "PossessableComponent.h"


void RaceImaginePowerup::OnFireEventServerSide(Entity *self, Entity *sender, std::string args, int32_t param1,
                                               int32_t param2, int32_t param3)
{
    if (sender->IsPlayer() && args == "powerup")
    {
        auto* possessorComponent = sender->GetComponent<PossessorComponent>();

        if (possessorComponent == nullptr)
        {
            return;
        }

        auto* vehicle = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());

        if (vehicle == nullptr)
        {
            return;
        }

        auto* destroyableComponent = vehicle->GetComponent<DestroyableComponent>();

        if (destroyableComponent == nullptr)
        {
            return;
        }

        destroyableComponent->Imagine(10);

        auto* missionComponent = sender->GetComponent<MissionComponent>();

        if (missionComponent != nullptr)
        {
            missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_RACING, self->GetLOT(), 12);
        }
    }
}
