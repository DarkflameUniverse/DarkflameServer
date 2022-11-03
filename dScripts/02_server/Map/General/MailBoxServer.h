#pragma once

#include "CppScripts.h"

class MailBoxServer : public CppScripts::Script {
public:
	/**
	 * When a mailbox is interacted with, this method updates the player game state
	 * to be in a mailbox.
	 *
	 * @param self The object that owns this script.
	 * @param user The user that interacted with this Entity.
	 */
	void OnUse(Entity* self, Entity* user) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;
};
