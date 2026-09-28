#pragma once
#include <cstdint>

namespace MessageType {
	enum class Master : uint32_t {
		REQUEST_ZONE_TRANSFER = 1,
		REQUEST_ZONE_TRANSFER_RESPONSE,
		SERVER_INFO,
		REQUEST_SESSION_KEY,
		SET_SESSION_KEY,
		SESSION_KEY_RESPONSE,
		PLAYER_ADDED,
		PLAYER_REMOVED,

		CREATE_PRIVATE_ZONE,
		REQUEST_PRIVATE_ZONE,

		WORLD_READY,
		PREP_ZONE,

		SHUTDOWN,
		SHUTDOWN_RESPONSE,
		SHUTDOWN_IMMEDIATE,

		SHUTDOWN_UNIVERSE,

		AFFIRM_TRANSFER_REQUEST,
		AFFIRM_TRANSFER_RESPONSE,

		NEW_SESSION_ALERT,

		REQUEST_SERVER_LIST,

		SERVER_LIST_RESPONSE,

		// Dashboard -> master -> every world: act on an online player (see PlayerAction.h)
		PLAYER_ACTION,
		// World -> master: whether this world handled a PLAYER_ACTION; master -> dashboard: aggregated result
		PLAYER_ACTION_RESULT,
		// World -> master -> dashboard: rows the game just wrote, so open dashboards update at once (see DataChanged.h)
		DATA_CHANGED,
		// World -> master -> dashboard: where players are (see DashboardMessages.h)
		PLAYER_POSITIONS,
		// Dashboard -> master -> every world: show an announcement to everyone online
		ANNOUNCE,
		// Dashboard -> master: shut the whole server down now (scheduled restarts; a supervisor starts it again)
		DASHBOARD_SHUTDOWN,
		// Dashboard -> master -> every server: settings changed on the dashboard, reload the config
		CONFIG_RELOAD,
		// Dashboard -> master: shut down one world instance (uint32 zone, uint32 instance)
		INSTANCE_SHUTDOWN,
		// Dashboard -> master -> every world: start or stop capturing a player's game messages (see MessageCapture.h)
		MESSAGE_CAPTURE_CONTROL,
		// World -> master -> dashboard: captured game messages and the capture's state (see MessageCapture.h)
		MESSAGE_CAPTURE_DATA,

		// Move everyone in one instance to another (replace or merge; see InstanceMigration.h). Sent by a world
		// for a GM command; a dashboard could send it too
		INSTANCE_MIGRATE,
		// Master -> source world: send your players to this instance
		MIGRATE_PLAYERS,
		// Source world -> master -> every world: how a migration is going
		MIGRATE_STATUS,
		// Source world -> master -> target world: what a moved player had that isn't in their saved character
		MIGRATE_PLAYER_STATE,

		// Any server -> master -> dashboard: traffic counters of the last few seconds (see ServerTraffic.h)
		SERVER_TRAFFIC,

		// UGC server -> master -> every world: player models whose mesh (model.nif) was just made or made again (see
		// UgcModelsMade.h), so worlds showing them tell their clients (NotifyClientUGCModelReady)
		UGC_MODELS_MADE,

		// Live updates (see LiveUpdate.h, docs/LiveUpdate.md)
		// Master -> world: a property (clone) instance is about to be replaced; freeze and save it, answer MIGRATE_STATUS
		MIGRATE_PREPARE,
		// Dashboard or world (a GM's /liveupdate) -> master: start, cancel or ask about a live update
		LIVE_UPDATE_REQUEST,
		// Master -> dashboard and worlds: how a live update is going
		LIVE_UPDATE_STATUS,
		// Master -> chat or UGC server: a new build takes over; finish up and exit
		LIVE_UPDATE_RETIRE,
		// Retiring chat server -> master -> the next chat server: the teams to carry over
		CHAT_HANDOFF,
		// Master -> worlds during a live update: a new chat server is up; connect and send it who is online
		CHAT_SERVER_READY,
	};
}
