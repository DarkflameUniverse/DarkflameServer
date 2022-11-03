#include "NtSleepingGuard.h"
#include "GameMessages.h"
#include "MissionComponent.h"

void NtSleepingGuard::OnStartup(Entity* self) {
	self->SetNetworkVar<bool>(u"asleep", true);
}

void NtSleepingGuard::OnEmoteReceived(Entity* self, const int32_t emote, Entity* target) {
	if (!self->GetNetworkVar<bool>(u"asleep"))
		return;

	// Check if emote is in m_ValidEmotes
	if (std::find(m_ValidEmotes.begin(), m_ValidEmotes.end(), emote) == m_ValidEmotes.end())
		return;

	// Set asleep to false
	self->SetNetworkVar<bool>(u"asleep", false);

	GameMessages::SendPlayAnimation(self, u"greet");

	auto* missionComponent = target->GetComponent<MissionComponent>();

	if (missionComponent != nullptr) {
		missionComponent->CompleteMission(1346);
	}

	self->AddTimer("AsleepAgain", 5.0f);
}

void NtSleepingGuard::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "AsleepAgain") {
		self->SetNetworkVar<bool>(u"asleep", true);
	}
}
