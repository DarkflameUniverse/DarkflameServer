#include "StoryBoxInteractServer.h"
#include "Character.h"
#include "GameMessages.h"
#include "dServer.h"
#include "Amf3.h"
#include "Entity.h"

void StoryBoxInteractServer::OnUse(Entity* self, Entity* user) {
	if (self->HasVar(u"customText")) {
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

	if (!self->HasVar(u"storyText")) return;
	const auto storyText = self->GetVarAsString(u"storyText");
	if (storyText.length() > 2) {
		auto storyValue = GeneralUtils::TryParse<uint32_t>(storyText.substr(storyText.length() - 2));
		if(!storyValue) return;
		int32_t boxFlag = self->GetVar<int32_t>(u"altFlagID");
		if (boxFlag <= 0) {
			boxFlag = (10000 + Game::server->GetZoneID() + storyValue.value());
		}

		if (user->GetCharacter()->GetPlayerFlag(boxFlag) == false) {
			user->GetCharacter()->SetPlayerFlag(boxFlag, true);
			GameMessages::SendFireEventClientSide(self->GetObjectID(), user->GetSystemAddress(), u"achieve", LWOOBJID_EMPTY, 0, -1, LWOOBJID_EMPTY);
		}
	}
}
