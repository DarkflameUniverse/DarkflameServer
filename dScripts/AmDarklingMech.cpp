#include "AmDarklingMech.h"
#include "DestroyableComponent.h"

void AmDarklingMech::OnStartup(Entity* self) 
{
    auto* destroyableComponent = self->GetComponent<DestroyableComponent>();

    destroyableComponent->SetFaction(4);

    qbTurretLOT = 13171;
}
