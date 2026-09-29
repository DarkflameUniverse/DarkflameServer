#include "dpMovementBlockers.h"

#include <algorithm>

#include "dpCollisionFilter.h"
#include "dpEntity.h"
#include "dpShapeBox.h"

namespace {
	constexpr uint32_t PLAYER_GROUP = 10;
	constexpr uint32_t ENEMY_GROUP = 12;
}

std::optional<uint32_t> dpMovementBlockers::BlockingFilter(const bool navmeshCarver, const bool solid, const uint32_t collisionGroup) {
	if (navmeshCarver) return 0;
	if (!solid || collisionGroup == 0) return std::nullopt;
	if (dpCollisionFilter::ShouldCollide(collisionGroup, ENEMY_GROUP) && !dpCollisionFilter::ShouldCollide(collisionGroup, PLAYER_GROUP)) return collisionGroup;
	return std::nullopt;
}

std::optional<float> dpMovementBlockers::FirstHit(const std::span<const dpMovementBlocker> blockers, const NiPoint3& a, const NiPoint3& b, const uint32_t moverFilter) {
	const NiPoint3 lift{ 0.0f, STEP_HEIGHT, 0.0f };
	const auto from = a + lift;
	const auto to = b + lift;

	std::optional<float> first;
	for (const auto& blocker : blockers) {
		if (!blocker.entity || blocker.entity->GetShape() == nullptr) continue;
		if (!dpCollisionFilter::ShouldCollide(blocker.filter, moverFilter)) continue;

		const auto* const box = dynamic_cast<const dpShapeBox*>(blocker.entity->GetShape());
		if (!box) continue;

		// Cheap reject against the box's axis aligned bounds first
		if (std::max(from.x, to.x) < box->m_MinX || std::min(from.x, to.x) > box->m_MaxX) continue;
		if (std::max(from.z, to.z) < box->m_MinZ || std::min(from.z, to.z) > box->m_MaxZ) continue;
		if (std::max(from.y, to.y) < box->m_MinY || std::min(from.y, to.y) > box->m_MaxY) continue;

		const auto hit = box->SegmentEntry(from, to);
		if (hit && (!first || *hit < *first)) first = hit;
	}
	return first;
}

std::vector<NiPoint3> dpMovementBlockers::ClampPath(const std::span<const dpMovementBlocker> blockers, const NiPoint3& start, std::vector<NiPoint3> path, const uint32_t moverFilter) {
	if (blockers.empty()) return path;

	auto previous = start;
	for (size_t i = 0; i < path.size(); i++) {
		const auto hit = FirstHit(blockers, previous, path[i], moverFilter);
		if (!hit) {
			previous = path[i];
			continue;
		}

		const auto delta = path[i] - previous;
		const auto length = delta.Length();
		const auto keep = std::max(0.0f, *hit * length - STOP_DISTANCE);
		path.resize(i);
		if (length > 0.0f && keep > 0.01f) path.push_back(previous + delta * (keep / length));
		return path;
	}
	return path;
}
