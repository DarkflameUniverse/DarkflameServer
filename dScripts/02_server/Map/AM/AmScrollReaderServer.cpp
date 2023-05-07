#include "AmScrollReaderServer.h"
#include "MissionComponent.h"

void AmScrollReaderServer::OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	if (identifier == u"story_end") {
		auto* missionComponent = sender->GetComponent<MissionComponent>();

		if (missionComponent == nullptr) {
			return;
		}

		missionComponent->ForceProgressTaskType(969, 1, 1, false);
	}
}
