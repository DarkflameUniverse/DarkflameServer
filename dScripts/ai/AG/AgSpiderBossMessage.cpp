#include "AgSpiderBossMessage.h"

#include "Entity.h"
#include "GameMessages.h"

#include "RenderComponent.h"

Box AgSpiderBossMessage::GetBox(Entity* self) const {
	return self->GetVar<Box>(u"box");
}

void AgSpiderBossMessage::SetBox(Entity* self, const Box& box) const {
	self->SetVar(u"box", box);
}

void AgSpiderBossMessage::MakeBox(Entity* self) const {
	auto box = GetBox(self);
	if (box.boxTarget == LWOOBJID_EMPTY || box.isDisplayed || box.boxSelf == LWOOBJID_EMPTY) return;

	box.isDisplayed = true;
	SetBox(self, box);
	self->AddTimer("BoxTimer", box.boxTime);

	const auto* const tgt = Game::entityManager->GetEntity(box.boxTarget);
	if (!tgt) return;
	GameMessages::DisplayTooltip tooltip;
	tooltip.target = tgt->GetObjectID();
	tooltip.show = true;
	tooltip.text = box.boxText;
	tooltip.time = box.boxTime * 1000; // to ms
	tooltip.Send(tgt->GetSystemAddress());
}

void AgSpiderBossMessage::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target || !target->IsPlayer()) return;

	auto box = GetBox(self);
	// knockback the target
	auto forward = self->GetRotation().GetForwardVector();
	box.boxTarget = target->GetObjectID();
	GameMessages::SendPlayFXEffect(target->GetObjectID(), 1378, u"create", "pushBack");
	RenderComponent::PlayAnimation(target, "knockback-recovery");
	forward.y += 15;
	forward.x *= 100;
	forward.z *= 100;
	GameMessages::SendKnockback(target->GetObjectID(), LWOOBJID_EMPTY, LWOOBJID_EMPTY, 0, forward);

	if (box.isTouch || box.isDisplayed) return;
	box.boxSelf = self->GetObjectID();
	box.isTouch = true;
	box.boxText = u"%[SPIDER_CAVE_MESSAGE]";
	SetBox(self, box);
	self->AddTimer("EventTimer", 0.1f);
}

void AgSpiderBossMessage::OnOffCollisionPhantom(Entity* self, Entity* target) {
	if (!target) return;
	auto box = GetBox(self);
	box.isTouch = false;
	box.Reset();
	SetBox(self, box);
}

void AgSpiderBossMessage::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "BoxTimer") {
		auto box = GetBox(self);
		box.isDisplayed = false;
		SetBox(self, box);
		ResetBox(self);
	} else if (timerName == "EventTimer") {
		auto box = GetBox(self);
		MakeBox(self);
	}
}

void AgSpiderBossMessage::ResetBox(Entity* self) const {
	auto box = GetBox(self);
	box.Reset();
	SetBox(self, box);
}
