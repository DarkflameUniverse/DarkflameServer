#include "AgSpaceStuff.h"
#include "GeneralUtils.h"
#include "GameMessages.h"
#include "EntityManager.h"

void AgSpaceStuff::OnStartup(Entity* self) {
	self->AddTimer("FloaterScale", 5.0f);

	EntityInfo info{};

	info.pos = { -418, 585, -30 };
	info.lot = 33;
	info.spawnerID = self->GetObjectID();

	auto* ref = EntityManager::Instance()->CreateEntity(info);

	EntityManager::Instance()->ConstructEntity(ref);

	self->SetVar(u"ShakeObject", ref->GetObjectID());

	self->AddTimer("ShipShakeIdle", 2.0f);
	self->SetVar(u"RandomTime", 10);
}

void AgSpaceStuff::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "FloaterScale") {
		int scaleType = GeneralUtils::GenerateRandomNumber<int>(1, 5);

		GameMessages::SendPlayAnimation(self, u"scale_0" + GeneralUtils::to_u16string(scaleType));
		self->AddTimer("FloaterPath", 0.4);
	} else if (timerName == "FloaterPath") {
		int pathType = GeneralUtils::GenerateRandomNumber<int>(1, 4);
		int randTime = GeneralUtils::GenerateRandomNumber<int>(20, 25);

		GameMessages::SendPlayAnimation(self, u"path_0" + (GeneralUtils::to_u16string(pathType)));
		self->AddTimer("FloaterScale", randTime);
	} else if (timerName == "ShipShakeExplode") {
		DoShake(self, true);
	} else if (timerName == "ShipShakeIdle") {
		DoShake(self, false);
	}
}

void AgSpaceStuff::DoShake(Entity* self, bool explodeIdle) {

	if (!explodeIdle) {
		auto* ref = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"ShakeObject"));

		const auto randomTime = self->GetVar<int>(u"RandomTime");
		auto time = GeneralUtils::GenerateRandomNumber<int>(0, randomTime + 1);

		if (time < randomTime / 2) {
			time += randomTime / 2;
		}

		self->AddTimer("ShipShakeIdle", static_cast<float>(time));

		if (ref)
			GameMessages::SendPlayEmbeddedEffectOnAllClientsNearObject(ref, FXName, ref->GetObjectID(), 500.0f);

		auto* debrisObject = GetEntityInGroup(DebrisFX);

		if (debrisObject)
			GameMessages::SendPlayFXEffect(debrisObject, -1, u"DebrisFall", "Debris", LWOOBJID_EMPTY, 1.0f, 1.0f, true);

		const auto randomFx = GeneralUtils::GenerateRandomNumber<int>(0, 3);

		auto* shipFxObject = GetEntityInGroup(ShipFX);
		if (shipFxObject) {
			std::string effectType = "shipboom" + std::to_string(randomFx);
			GameMessages::SendPlayFXEffect(shipFxObject, 559, GeneralUtils::ASCIIToUTF16(effectType), "FX", LWOOBJID_EMPTY, 1.0f, 1.0f, true);
		}

		self->AddTimer("ShipShakeExplode", 5.0f);

		auto* shipFxObject2 = GetEntityInGroup(ShipFX2);
		if (shipFxObject2)
			GameMessages::SendPlayAnimation(shipFxObject2, u"explosion");
	} else {
		auto* shipFxObject = GetEntityInGroup(ShipFX);
		auto* shipFxObject2 = GetEntityInGroup(ShipFX2);

		if (shipFxObject)
			GameMessages::SendPlayAnimation(shipFxObject, u"idle");

		if (shipFxObject2)
			GameMessages::SendPlayAnimation(shipFxObject2, u"idle");
	}
}

Entity* AgSpaceStuff::GetEntityInGroup(const std::string& group) {
	auto entities = EntityManager::Instance()->GetEntitiesInGroup(group);
	Entity* en = nullptr;

	for (auto entity : entities) {
		if (entity) {
			en = entity;
			break;
		}
	}

	return en;
}
