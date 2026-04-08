#ifndef SIMPLEMOVERSUBCOMPONENT_H
#define SIMPLEMOVERSUBCOMPONENT_H

#include "PlatformSubComponent.h"

#include <memory>

class Path;

/**
 * Simple mover - auto-generates a 2-waypoint path from component properties.
 * Corresponds to client LWOPlatformSimpleMover (type 5).
 */
class SimpleMoverSubComponent final : public PlatformSubComponent {
public:
	SimpleMoverSubComponent(Entity* parentEntity, const NiPoint3& startPos,
		const NiQuaternion& startRot, const NiPoint3& platformMove, float platformMoveTime);

	const Path* GetGeneratedPath() const { return m_GeneratedPath.get(); }

private:
	void GeneratePath(const NiPoint3& startPos, const NiQuaternion& startRot,
		const NiPoint3& platformMove, float platformMoveTime);

	std::unique_ptr<Path> m_GeneratedPath;
};

#endif // SIMPLEMOVERSUBCOMPONENT_H
