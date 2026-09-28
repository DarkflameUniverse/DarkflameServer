#ifndef __LIVEUPDATECOORDINATOR__H__
#define __LIVEUPDATECOORDINATOR__H__

#include <functional>
#include <string>

#include "LiveUpdateMachine.h"
#include "master/LiveUpdate.h"

/**
 * Master's side of live updates (docs/LiveUpdate.md): runs LiveUpdate::Machine against the instance manager, the
 * instance migrations and the other servers. Started from the dashboard (LIVE_UPDATE_REQUEST), a GM's /liveupdate or
 * SIGUSR2 to master. Main thread only.
 */
namespace LiveUpdateCoordinator {
	// What MasterServer.cpp provides: the other servers, and where statuses go
	struct Hooks {
		std::function<LiveUpdate::ServiceView(LiveUpdate::eService)> service;
		std::function<void(LiveUpdate::eService)> retire;  // LIVE_UPDATE_RETIRE (chat, UGC) or SHUTDOWN (auth, dashboard)
		std::function<void(LiveUpdate::eService)> stop;    // SHUTDOWN
		std::function<void(LiveUpdate::eService)> start;   // start its process
		std::function<void()> chatReady;                   // CHAT_SERVER_READY to every world
		std::function<void(const LiveUpdateStatus&, bool toWorlds)> publish;
	};

	void Initialize(Hooks hooks);

	// Start one; false (and why) when it can't
	bool Start(const std::string& by, LWOOBJID requesterId, int32_t warnSeconds, std::string& error);

	// LIVE_UPDATE_REQUEST (the sender is checked by master: the dashboard, or a world for a GM)
	void HandleRequest(const LiveUpdateRequest& request);

	// Every instance migration's progress (MigrationCoordinator's observer)
	void OnMigrationStatus(const MigrationStatus& status);

	// Every master frame
	void Update();

	// Master is shutting down
	void Abort(const std::string& why);

	bool IsRunning();

	LiveUpdateStatus Status();
}

#endif  //!__LIVEUPDATECOORDINATOR__H__
