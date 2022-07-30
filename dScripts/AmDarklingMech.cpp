#include "AmDarklingMech.h"

void AmDarklingMech::OnStartup(Entity* self) {
	BaseEnemyMech::OnStartup(self);
	qbTurretLOT = 13171;
}
