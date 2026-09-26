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

		// Move everyone in one instance to another (replace or merge; see InstanceMigration.h). Sent by a world
		// for a GM command; a dashboard could send it too
		INSTANCE_MIGRATE,
		// Master -> source world: send your players to this instance
		MIGRATE_PLAYERS,
		// Source world -> master -> every world: how a migration is going
		MIGRATE_STATUS,
		// Source world -> master -> target world: what a moved player had that isn't in their saved character
		MIGRATE_PLAYER_STATE,
	};
}
