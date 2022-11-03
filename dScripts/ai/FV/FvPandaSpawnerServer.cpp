#include "FvPandaSpawnerServer.h"
#include "Character.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "ScriptedActivityComponent.h"

void FvPandaSpawnerServer::OnCollisionPhantom(Entity* self, Entity* target) {
	auto* character = target->GetCharacter();
	if (character != nullptr && character->GetPlayerFlag(81)) {

		auto raceObjects = EntityManager::Instance()->GetEntitiesInGroup("PandaRaceObject");
		if (raceObjects.empty())
			return;

		// Check if the player is currently in a footrace
		auto* scriptedActivityComponent = raceObjects.at(0)->GetComponent<ScriptedActivityComponent>();
		if (scriptedActivityComponent == nullptr || !scriptedActivityComponent->IsPlayedBy(target))
			return;

		// If the player already spawned a panda
		auto playerPandas = EntityManager::Instance()->GetEntitiesInGroup("panda" + std::to_string(target->GetObjectID()));
		if (!playerPandas.empty()) {
			GameMessages::SendFireEventClientSide(self->GetObjectID(), target->GetSystemAddress(), u"playerPanda",
				target->GetObjectID(), 0, 0, target->GetObjectID());
			return;
		}

		// If there's already too many spawned pandas
		auto pandas = EntityManager::Instance()->GetEntitiesInGroup("pandas");
		if (pandas.size() > 4) {
			GameMessages::SendFireEventClientSide(self->GetObjectID(), target->GetSystemAddress(), u"tooManyPandas",
				target->GetObjectID(), 0, 0, target->GetObjectID());
			return;
		}

		EntityInfo info{};
		info.spawnerID = target->GetObjectID();
		info.pos = self->GetPosition();
		info.lot = 5643;
		info.settings = {
			new LDFData<LWOOBJID>(u"tamer", target->GetObjectID()),
			new LDFData<std::u16string>(u"groupID", u"panda" + (GeneralUtils::to_u16string(target->GetObjectID())) + u";pandas")
		};

		auto* panda = EntityManager::Instance()->CreateEntity(info);
		EntityManager::Instance()->ConstructEntity(panda);
	}
}
