#ifndef __PLAYERMANAGER__H__
#define __PLAYERMANAGER__H__

#include "dCommonVars.h"

#include <string>

class Entity;
struct SystemAddress;

namespace PlayerManager {
	void AddPlayer(Entity* player);

	bool RemovePlayer(Entity* player);

	Entity* GetPlayer(const SystemAddress& sysAddr);

	Entity* GetPlayer(const std::string& name);

	Entity* GetPlayer(LWOOBJID playerID);

	const std::vector<Entity*>& GetAllPlayers();
};

#endif  //!__PLAYERMANAGER__H__
