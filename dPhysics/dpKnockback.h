#ifndef DPKNOCKBACK_H
#define DPKNOCKBACK_H

#include <functional>

#include "NiPoint3.h"

/**
 * Server side knockback for objects the server moves (enemies, NPCs). The client only simulates knockbacks on the
 * character it controls (LWOControllablePhysComponent::msgKnockback, 1.10.64 0x00cf5610); everything else is drawn
 * where the server serializes it, so the server has to fly the object itself. This follows the client:
 *
 * - KnockbackBehavior::Cast (0x004efc10) builds the vector: the flat direction away from the source, raised by
 *   `angle` degrees (tan(angle) added to the unit flat direction, then unitized), times `strength` capped at 300.
 *   An angle strictly between 89 and 91 degrees knocks straight up.
 * - msgKnockback: a vector longer than 5 lifts the character 0.5 up, sets its velocity to the vector and lets the
 *   character controller fall under gravity (WorldConfig.pegravityvalue) until it lands, no sooner than 250ms later.
 *   A shorter vector just moves the character by the vector, dropped onto the ground if it's within half its height.
 */
namespace dpKnockback {
	// Vectors at or below this length move the object instead of launching it (0x0151370c)
	constexpr float MIN_LAUNCH_SPEED = 5.0f;
	// How far the object is lifted when launched (0x01479c58)
	constexpr float LAUNCH_LIFT = 0.5f;
	// The client keeps the knockback state at least this long (msgKnockback: knockbackExpire = now + 250)
	constexpr float MIN_AIRBORNE_TIME = 0.25f;
	// KnockbackBehavior caps strength at this (0x01513c38)
	constexpr float MAX_STRENGTH = 300.0f;
	// Ground this much above the object is a wall it can't pass, lower ground is landed on
	constexpr float MAX_STEP_HEIGHT = 1.0f;
	// Longest integration step, so fast knockbacks don't skip over walls
	constexpr float MAX_SUB_STEP = 1.0f / 60.0f;
	// Give up after this long in the air (knocked off the world)
	constexpr float MAX_AIRBORNE_TIME = 10.0f;

	/**
	 * The knockback vector KnockbackBehavior sends
	 * @param away the direction to knock towards (only its x and z are used)
	 * @param angleDegrees the elevation above the ground plane, the behavior's `angle`
	 * @param strength the behavior's `strength`
	 */
	[[nodiscard]] NiPoint3 ComputeVector(const NiPoint3& away, float angleDegrees, float strength);

	// Returns the ground height under a point
	using GroundQuery = std::function<float(const NiPoint3&)>;

	class Arc {
	public:
		/**
		 * Starts a knockback. Returns false when the vector is short enough that the object should just be moved
		 * by it (see Nudge) instead of flying.
		 */
		bool Start(const NiPoint3& position, const NiPoint3& vector);

		/**
		 * Advances the flight by deltaTime under the given gravity (a positive number pulling down)
		 * @return true while still in the air
		 */
		bool Step(float deltaTime, float gravity, const GroundQuery& ground);

		[[nodiscard]] bool IsActive() const { return m_Active; }
		[[nodiscard]] const NiPoint3& GetPosition() const { return m_Position; }
		[[nodiscard]] const NiPoint3& GetVelocity() const { return m_Velocity; }
		[[nodiscard]] float GetElapsed() const { return m_Elapsed; }

		void Cancel() { m_Active = false; m_Velocity = NiPoint3Constant::ZERO; }

	private:
		bool SubStep(float deltaTime, float gravity, const GroundQuery& ground);

		NiPoint3 m_Position{};
		NiPoint3 m_Velocity{};
		float m_Elapsed{};
		bool m_Active{};
	};

	/**
	 * A short knockback: the position moved by the vector, dropped onto the ground when the ground is within
	 * maxDrop of it; otherwise the object stays where it is (the client's msgKnockback does the same).
	 */
	[[nodiscard]] NiPoint3 Nudge(const NiPoint3& position, const NiPoint3& vector, float maxDrop, const GroundQuery& ground);
};

#endif  //!DPKNOCKBACK_H
