#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "NiPoint3.h"

class dpEntity;

/**
 * Walls the characters the server walks (enemies, NPCs, pets on MovementAI) cannot cross. The server moves them along
 * navmesh paths without collision, so a wall that should stop them only does if their paths stop at it:
 * - Objects that carve the AI navmesh (navmesh_carver in their level config, read by the client's
 *   LWOBasePhysComponent::LoadConfigData 0x00c495c9 next to add_to_navmesh and carver_only): nothing walked by the
 *   server may go where they are. Many are carver_only too, which the client never loads, as the walls around the
 *   Avant Gardens Sentinel camp.
 * - Solid objects whose collision group touches the movers but not players (e.g. group 18, "FV - Enemy Blocking
 *   Volume" and the Nimbus Station pet ranch's "PR - Pet Blocker"): the client lets players through them, so they are
 *   only there to stop NPCs.
 */
struct dpMovementBlocker {
	const dpEntity* entity{};
	// The collision filter it blocks with (see dpCollisionFilter): 0 blocks every mover
	uint32_t filter{};
};

namespace dpMovementBlockers {
	// Paths are tested this far above the ground a mover walks on, so a wall sitting on uneven ground still catches them
	constexpr float STEP_HEIGHT = 1.0f;
	// How far before a wall a stopped path ends
	constexpr float STOP_DISTANCE = 1.0f;

	/**
	 * The filter a physics object blocks movers with, or none when it doesn't block them: 0 for a navmesh carver,
	 * its group for a solid object only enemies (not players) collide with.
	 */
	[[nodiscard]] std::optional<uint32_t> BlockingFilter(bool navmeshCarver, bool solid, uint32_t collisionGroup);

	/**
	 * Where a mover with the given collision filter first walks into one of the blockers, going along the segment from
	 * a to b, as a fraction of the way (0 to 1). Only boxes block; a mover that starts inside one can walk out.
	 */
	[[nodiscard]] std::optional<float> FirstHit(std::span<const dpMovementBlocker> blockers, const NiPoint3& a, const NiPoint3& b, uint32_t moverFilter);

	/**
	 * A path (the points after start) cut where it first walks into a blocker: it ends STOP_DISTANCE short of the wall.
	 * Returns the path unchanged when it crosses none.
	 */
	[[nodiscard]] std::vector<NiPoint3> ClampPath(std::span<const dpMovementBlocker> blockers, const NiPoint3& start, std::vector<NiPoint3> path, uint32_t moverFilter);
};
