#include "NtBeamImaginationCollectors.h"
#include "GeneralUtils.h"
#include "GameMessages.h"

void NtBeamImaginationCollectors::OnStartup(Entity* self) {
	self->AddTimer("PlayFX", GetRandomNum());
}

int32_t NtBeamImaginationCollectors::GetRandomNum() {
	int32_t randNum = m_LastRandom;

	while (randNum == m_LastRandom) {
		randNum = GeneralUtils::GenerateRandomNumber<int32_t>(m_RandMin, m_RandMax);
	}

	m_LastRandom = randNum;

	return randNum;
}

void NtBeamImaginationCollectors::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName != "PlayFX") {
		return;
	}

	GameMessages::SendPlayFXEffect(self->GetObjectID(), -1, m_FxName, "Beam");

	self->AddTimer("PlayFX", GetRandomNum());
}
