#include "FvCandle.h"
#include "MissionComponent.h"

void FvCandle::OnStartup(Entity* self) {
	auto* render = static_cast<RenderComponent*>(self->GetComponent(COMPONENT_TYPE_RENDER));
	if (render == nullptr)
		return;

	render->PlayEffect(2108, u"create", "candle_light", LWOOBJID_EMPTY, 1.0f, 1.0f, true);
	self->SetI32(u"Smoke", static_cast<int32_t>(5));
	self->SetBoolean(u"AmHit", false);
}

void FvCandle::OnHit(Entity* self, Entity* attacker) {
	BlowOutCandle(self, attacker);
}

void FvCandle::BlowOutCandle(Entity* self, Entity* blower) {
	if (self->GetBoolean(u"AmHit"))
		return;

	auto* render = static_cast<RenderComponent*>(self->GetComponent(COMPONENT_TYPE_RENDER));
	if (render == nullptr)
		return;
	
	auto* missionComponent = blower->GetComponent<MissionComponent>();

	if (missionComponent == nullptr) return;

	missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());

	self->SetBoolean(u"AmHit", true);

	render->StopEffect("candle_light", false);
	render->PlayEffect(2109, u"create", "candle_smoke", LWOOBJID_EMPTY, 1.0f, 1.0f, true);

	self->AddTimer("SmokeTime", self->GetI32(u"Smoke"));
}

void FvCandle::OnTimerDone(Entity* self, std::string timerName) {
	self->SetBoolean(u"AmHit", false);

	auto* render = static_cast<RenderComponent*>(self->GetComponent(COMPONENT_TYPE_RENDER));
	if (render == nullptr)
		return;

	render->StopEffect("candle_smoke", false);
	render->PlayEffect(2108, u"create", "candle_light", LWOOBJID_EMPTY, 1.0f, 1.0f, true);
}