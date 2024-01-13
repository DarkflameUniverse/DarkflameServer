#ifndef __PLAYERMANAGER__H__
#define __PLAYERMANAGER__H__

#include "dCommonVars.h"

#include <string>

class Player;
struct SystemAddress;

namespace PlayerManager {
	void AddPlayer(Player* player);

	void RemovePlayer(Player* player);

	Player* GetPlayer(const SystemAddress& sysAddr);

	Player* GetPlayer(const std::string& name);

	Player* GetPlayer(LWOOBJID playerID);

	const std::vector<Player*>& GetAllPlayers();
};

#endif  //!__PLAYERMANAGER__H__
