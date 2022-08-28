#include "MinigameBlueMark.h"
#include "Entity.h"
#include "dZoneManager.h"

void MinigameBlueMark::OnStartup(Entity* self) {
	auto* controller = dZoneManager::Instance()->GetZoneControlObject();
	if (controller) controller->NotifyObject(self, m_Notification);
}
