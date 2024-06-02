#ifndef __PLAYERMANAGER__H__
#define __PLAYERMANAGER__H__

#include "dCommonVars.h"
#include "Observable.h"

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

	static Observable<Entity*> OnPlayerAdded;
	static Observable<Entity*> OnPlayerRemoved;
};

#endif  //!__PLAYERMANAGER__H__
