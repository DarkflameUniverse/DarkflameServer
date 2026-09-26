#ifndef __WORLDMIGRATION__H__
#define __WORLDMIGRATION__H__

#include <functional>

#include "InstanceMigration.h"
#include "RakNetTypes.h"
#include "dCommonVars.h"

class Entity;

/**
 * A world server's side of instance migrations (InstanceMigration.h, docs/SeamlessTransfer.md).
 *
 * As the source: warns everyone with the client's own "Mythran Maintenance Alert", then moves players a few at a
 * time. Each is saved (position included), locked (their packets are ignored and they aren't saved again when they
 * disconnect, so nothing they do after the save can be lost or duplicated), and sent TRANSFER_TO_WORLD with the
 * Mythran shift flag. As the target: puts back what was carried over (the pet that was out) once they have loaded.
 * GMs start migrations with /replaceinstance and /mergeinstance.
 */
namespace WorldMigration {
	// What a disconnect runs (save and remove the user); used to drop players whose client never left
	void SetCleanupHandler(std::function<void(const SystemAddress&)> handler);

	// MIGRATE_PLAYERS from master (a target port of 0 cancels the running migration)
	void HandleOrder(const MigratePlayersOrder& order);

	// MIGRATE_PLAYER_STATE from master: a player on their way here
	void StoreCarriedState(const CarriedPlayerState& state);

	// Every world frame
	void Update(float deltaTime);

	// The player was sent away: ignore their packets, and don't save them again when they disconnect
	bool IsLeaving(const SystemAddress& sysAddr);

	// Their connection closed; forget them
	void OnDisconnected(const SystemAddress& sysAddr);

	// A player finished loading here (PLAYER_LOADED)
	void OnPlayerLoaded(Entity* player);

	// Experimental seamless migration: this character is on its way and its client kept the zone loaded, so it must
	// not be sent LOAD_STATIC_ZONE (that tears the scene down); load it at once instead
	bool ArrivesSeamlessly(LWOOBJID characterId);

	// Its player entity was created and constructed: run what the client's PlayerLoaded would have
	void OnSeamlessArrival(Entity* player);

	// MIGRATE_STATUS from master: tell the GM who asked, if they are here
	void HandleStatus(const MigrationStatus& status);

	// Ask master to move everyone in this instance; the seam a dashboard or other tool can use too, by sending
	// INSTANCE_MIGRATE to master
	void RequestMigration(Entity* requester, InstanceMigration::eKind kind, uint32_t targetInstance, uint16_t warnSeconds, bool seamless);

	// /replaceinstance [warn seconds] [seamless]
	void ReplaceInstanceCommand(Entity* entity, const SystemAddress& sysAddr, const std::string args);

	// /mergeinstance [target instance, 0 for the best fit] [warn seconds] [seamless]
	void MergeInstanceCommand(Entity* entity, const SystemAddress& sysAddr, const std::string args);
}

#endif  //!__WORLDMIGRATION__H__
