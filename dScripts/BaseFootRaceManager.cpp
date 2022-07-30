#include "BaseFootRaceManager.h"
#include "EntityManager.h"
#include "Character.h"

void BaseFootRaceManager::OnStartup(Entity* self) {
	// TODO: Add to FootRaceStarter group
}

void BaseFootRaceManager::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	const auto splitArguments = GeneralUtils::SplitString(args, '_');
	if (splitArguments.size() > 1) {

		const auto eventName = splitArguments[0];
		const auto player = EntityManager::Instance()->GetEntity(std::stoull(splitArguments[1]));

		if (player != nullptr) {
			if (eventName == "updatePlayer") {
				UpdatePlayer(self, player->GetObjectID());
			} else if (IsPlayerInActivity(self, player->GetObjectID())) {
				if (eventName == "initialActivityScore") {
					auto* character = player->GetCharacter();
					if (character != nullptr) {
						character->SetPlayerFlag(115, true);
					}

					SetActivityScore(self, player->GetObjectID(), 1);
				} else if (eventName == "updatePlayerTrue") {
					auto* character = player->GetCharacter();
					if (character != nullptr) {
						character->SetPlayerFlag(115, false);
					}

					UpdatePlayer(self, player->GetObjectID(), true);
				} else if (eventName == "PlayerWon") {
					auto* character = player->GetCharacter();
					if (character != nullptr) {
						character->SetPlayerFlag(115, false);
						if (param2 != -1) // Certain footraces set a flag
							character->SetPlayerFlag(param2, true);
					}

					StopActivity(self, player->GetObjectID(), 0, param1);
				}
			}
		}
	}
}
