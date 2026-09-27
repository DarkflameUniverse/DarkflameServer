#ifndef DPCOLLISIONFILTER_H
#define DPCOLLISIONFILTER_H

#include <cstdint>

/**
 * Which physics objects can touch, the way the client's Havok filter decides it (1.10.64: table built in
 * PeCollisionFilter::SetupGroups 0x00fcf9a0, tested in PeCollisionFilter::IsCollisionEnabled 0x00fb6940).
 *
 * A filter value is a collision group (PhysicsComponent.collisionGroup, or the CollisionGroupID config) with
 * optional flags in the high bits:
 * - 0 touches everything.
 * - PHANTOM_ONLY (0x40000000): two objects that both have it never touch; otherwise it is ignored.
 * - GROUP_MASK (0x04000000): the low bits are a mask of the groups it touches (bit n = group n + 1) instead of a group.
 * - Two objects with the same group and the same non zero high bits never touch (the same system's parts).
 * Otherwise two groups touch when the client's group table says so. Players are group 10, enemies 12.
 */
namespace dpCollisionFilter {
	constexpr uint32_t PHANTOM_ONLY = 0x40000000;
	constexpr uint32_t GROUP_MASK = 0x04000000;
	constexpr uint32_t GROUP_BITS = 0x07ffffff;
	constexpr uint32_t SYSTEM_BITS = 0xf8000000;
	constexpr uint32_t GROUP_COUNT = 26;

	[[nodiscard]] bool ShouldCollide(uint32_t filterA, uint32_t filterB);

	// Whether group a touches group b in the client's table (groups start at 1)
	[[nodiscard]] bool GroupsCollide(uint32_t groupA, uint32_t groupB);
};

#endif  //!DPCOLLISIONFILTER_H
