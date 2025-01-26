#ifndef __CHATWEB_H__
#define __CHATWEB_H__

#include <string>
#include <functional>

#include "Web.h"

enum class eWSSubscription {
	WORLD_CHAT,
	PRIVATE_CHAT,
	TEAM_CHAT,
	TEAM,
	PLAYER,
	INVALID
};

namespace ChatWeb {
	void RegisterRoutes();
};


#endif // __CHATWEB_H__

