#ifndef __CHATWEB_H__
#define __CHATWEB_H__

#include <string>
#include <functional>

#include "Web.h"
#include "PlayerContainer.h"
#include "IActivityLog.h"
#include "ChatPacketHandler.h"

namespace ChatWeb {
	void RegisterRoutes();
	void SendWSPlayerUpdate(const PlayerData& player, eActivityType activityType);
};


#endif // __CHATWEB_H__

