#include "FvPandaSpawnerServer.h"
#include "Character.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "ObjectMessages.h"
#include "EntityInfo.h"
#include "ScriptedActivityComponent.h"

void FvPandaSpawnerServer::OnCollisionPhantom(Entity* self, Entity* target) {
	auto* character = target->GetCharacter();
	if (character != nullptr && character->GetPlayerFlag(81)) {

		auto raceObjects = Game::entityManager->GetEntitiesInGroup("PandaRaceObject");
		if (raceObjects.empty())
			return;

		// Check if the player is currently in a footrace
		auto* scriptedActivityComponent = raceObjects.at(0)->GetComponent<ScriptedActivityComponent>();
		if (scriptedActivityComponent == nullptr || !scriptedActivityComponent->IsPlayedBy(target))
			return;

		// If the player already spawned a panda
		auto playerPandas = Game::entityManager->GetEntitiesInGroup("panda" + std::to_string(target->GetObjectID()));
		if (!playerPandas.empty()) {
			GameMessages::FireEventClientSide(self->GetObjectID(), u"playerPanda", target->GetObjectID(), target->GetObjectID()).SendToClient(target->GetSystemAddress());
			return;
		}

		// If there's already too many spawned pandas
		auto pandas = Game::entityManager->GetEntitiesInGroup("pandas");
		if (pandas.size() > 4) {
			GameMessages::FireEventClientSide(self->GetObjectID(), u"tooManyPandas", target->GetObjectID(), target->GetObjectID()).SendToClient(target->GetSystemAddress());
			return;
		}

		EntityInfo info{};
		info.spawnerID = target->GetObjectID();
		info.pos = self->GetPosition();
		info.lot = 5643;
		info.settings.Insert<LWOOBJID>(u"tamer", target->GetObjectID());
		info.settings.Insert<std::u16string>(u"groupID", u"panda" + (GeneralUtils::to_u16string(target->GetObjectID())) + u";pandas");

		auto* panda = Game::entityManager->CreateEntity(info);
		Game::entityManager->ConstructEntity(panda);
	}
}
