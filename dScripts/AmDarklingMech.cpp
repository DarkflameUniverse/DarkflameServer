#include "AmDarklingMech.h"
#include "DestroyableComponent.h"

void AmDarklingMech::OnStartup(Entity* self) 
{
    BaseEnemyMech::OnStartup(self);
    qbTurretLOT = 13171;
}
