#include "StoryBoxInteractServer.h"
#include "Character.h"
#include "GameMessages.h"
#include "dServer.h"
#include "Amf3.h"
#include "Entity.h"

void StoryBoxInteractServer::OnUse(Entity* self, Entity* user) {
	if (self->GetVar<bool>(u"hasCustomText")) {
		const auto& customText = self->GetVar<std::string>(u"customText");

		{
			AMFArrayValue args;

			args.Insert("state", "Story");

			GameMessages::SendUIMessageServerToSingleClient(user, user->GetSystemAddress(), "pushGameState", args);
		}

		user->AddCallbackTimer(0.1f, [user, customText]() {
			AMFArrayValue args;

			args.Insert("visible", true);
			args.Insert("text", customText);

			GameMessages::SendUIMessageServerToSingleClient(user, user->GetSystemAddress(), "ToggleStoryBox", args);
			});

		return;
	}

	const auto storyText = self->GetVarAsString(u"storyText");

	int32_t boxFlag = self->GetVar<int32_t>(u"altFlagID");
	if (boxFlag <= 0) {
		boxFlag = (10000 + Game::server->GetZoneID() + std::stoi(storyText.substr(storyText.length() - 2)));
	}

	if (user->GetCharacter()->GetPlayerFlag(boxFlag) == false) {
		user->GetCharacter()->SetPlayerFlag(boxFlag, true);
		GameMessages::SendFireEventClientSide(self->GetObjectID(), user->GetSystemAddress(), u"achieve", LWOOBJID_EMPTY, 0, -1, LWOOBJID_EMPTY);
	}
}
