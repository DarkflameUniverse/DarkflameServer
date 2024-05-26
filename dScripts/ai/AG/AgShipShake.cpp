#include "AgShipShake.h"
#include "EntityInfo.h"
#include "GeneralUtils.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "RenderComponent.h"
#include "Entity.h"

void AgShipShake::OnStartup(Entity* self) {
	EntityInfo info{};

	info.pos = { -418, 585, -30 };
	info.lot = 33;
	info.spawnerID = self->GetObjectID();

	auto* ref = Game::entityManager->CreateEntity(info);

	Game::entityManager->ConstructEntity(ref);

	self->SetVar(u"ShakeObject", ref->GetObjectID());

	self->AddTimer("ShipShakeIdle", 2.0f);
	self->SetVar(u"RandomTime", 10);
}

void AgShipShake::OnTimerDone(Entity* self, std::string timerName) {
	auto* shipFxObject = GetEntityInGroup(ShipFX);
	auto* shipFxObject2 = GetEntityInGroup(ShipFX2);
	auto* debrisObject = GetEntityInGroup(DebrisFX);
	if (timerName == "ShipShakeIdle") {
		auto* ref = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(u"ShakeObject"));

		const auto randomTime = self->GetVar<int>(u"RandomTime");
		auto time = GeneralUtils::GenerateRandomNumber<int>(0, randomTime + 1);

		if (time < randomTime / 2) {
			time += randomTime / 2;
		}

		self->AddTimer("ShipShakeIdle", static_cast<float>(time));

		if (ref)
			GameMessages::SendPlayEmbeddedEffectOnAllClientsNearObject(ref, FXName, ref->GetObjectID(), 500.0f);


		if (debrisObject)
			GameMessages::SendPlayFXEffect(debrisObject, -1, u"DebrisFall", "Debris", LWOOBJID_EMPTY, 1.0f, 1.0f, true);

		const auto randomFx = GeneralUtils::GenerateRandomNumber<int>(0, 3);

		if (shipFxObject) {
			std::string effectType = "shipboom" + std::to_string(randomFx);
			GameMessages::SendPlayFXEffect(shipFxObject, 559, GeneralUtils::ASCIIToUTF16(effectType), "FX", LWOOBJID_EMPTY, 1.0f, 1.0f, true);
		}

		self->AddTimer("ShipShakeExplode", 5.0f);

		if (shipFxObject2)
			RenderComponent::PlayAnimation(shipFxObject2, u"explosion");
	} else if (timerName == "ShipShakeExplode") {
		if (shipFxObject)
			RenderComponent::PlayAnimation(shipFxObject, u"idle");
		if (shipFxObject2)
			RenderComponent::PlayAnimation(shipFxObject2, u"idle");
	}
}

Entity* AgShipShake::GetEntityInGroup(const std::string& group) {
	auto entities = Game::entityManager->GetEntitiesInGroup(group);
	Entity* en = nullptr;

	for (auto entity : entities) {
		if (entity) {
			en = entity;
			break;
		}
	}

	return en;
}

