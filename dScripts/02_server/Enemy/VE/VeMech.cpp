#include "VeMech.h"

void VeMech::OnStartup(Entity* self) {
	BaseEnemyMech::OnStartup(self);
	qbTurretLOT = 8432;
}
