#include "MinigameBlueMark.h"
#include "Game.h"
#include "dZoneManager.h"

void MinigameBlueMark::OnStartup(Entity* self) {
	Game::zoneManager->GetZoneControlObject()->NotifyObject(self, "Blue_Mark");
}
