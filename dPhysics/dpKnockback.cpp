#include "dpKnockback.h"

#include <algorithm>
#include <cmath>
#include <numbers>

NiPoint3 dpKnockback::ComputeVector(const NiPoint3& away, const float angleDegrees, const float strength) {
	NiPoint3 direction{ 0.0f, 1.0f, 0.0f };

	// Anything but (nearly) straight up: the flat direction raised by the angle
	if (angleDegrees <= 89.0f || angleDegrees >= 91.0f) {
		direction = NiPoint3(away.x, 0.0f, away.z);
		const auto flatLength = direction.Length();
		direction = flatLength > 0.0f ? direction / flatLength : NiPoint3Constant::ZERO;
		direction.y += std::tan(angleDegrees * std::numbers::pi_v<float> / 180.0f);
		const auto length = direction.Length();
		direction = length > 0.0f ? direction / length : NiPoint3Constant::ZERO;
	}

	return direction * std::min(strength, MAX_STRENGTH);
}

bool dpKnockback::Arc::Start(const NiPoint3& position, const NiPoint3& vector) {
	m_Elapsed = 0.0f;
	if (vector.SquaredLength() <= MIN_LAUNCH_SPEED * MIN_LAUNCH_SPEED) {
		m_Active = false;
		m_Position = position;
		m_Velocity = NiPoint3Constant::ZERO;
		return false;
	}

	m_Active = true;
	m_Position = position;
	m_Position.y += LAUNCH_LIFT;
	m_Velocity = vector;
	return true;
}

bool dpKnockback::Arc::Step(float deltaTime, const float gravity, const GroundQuery& ground) {
	while (m_Active && deltaTime > 0.0f) {
		const auto step = std::min(deltaTime, MAX_SUB_STEP);
		deltaTime -= step;
		if (!SubStep(step, gravity, ground)) m_Active = false;
	}

	return m_Active;
}

bool dpKnockback::Arc::SubStep(const float deltaTime, const float gravity, const GroundQuery& ground) {
	m_Elapsed += deltaTime;

	// Semi-implicit Euler, like the character controller: velocity first, then position
	m_Velocity.y -= gravity * deltaTime;
	auto next = m_Position + m_Velocity * deltaTime;

	auto groundHeight = ground(next);
	if (groundHeight - next.y > MAX_STEP_HEIGHT && groundHeight - m_Position.y > MAX_STEP_HEIGHT) {
		// Flew into something taller than a step: stop going sideways and keep falling where we are
		m_Velocity.x = 0.0f;
		m_Velocity.z = 0.0f;
		next.x = m_Position.x;
		next.z = m_Position.z;
		groundHeight = ground(next);
	}

	m_Position = next;

	const bool falling = m_Velocity.y <= 0.0f;
	if (falling && m_Position.y <= groundHeight && m_Elapsed >= MIN_AIRBORNE_TIME) {
		m_Position.y = groundHeight;
		m_Velocity = NiPoint3Constant::ZERO;
		return false;
	}

	// Below the ground before the minimum time is up (or still going up): ride along the ground
	if (m_Position.y < groundHeight) {
		m_Position.y = groundHeight;
		if (m_Velocity.y < 0.0f) m_Velocity.y = 0.0f;
	}

	if (m_Elapsed >= MAX_AIRBORNE_TIME) {
		m_Velocity = NiPoint3Constant::ZERO;
		return false;
	}

	return true;
}

NiPoint3 dpKnockback::Nudge(const NiPoint3& position, const NiPoint3& vector, const float maxDrop, const GroundQuery& ground) {
	auto moved = position + vector;
	const auto groundHeight = ground(moved);
	if (std::abs(moved.y - groundHeight) > maxDrop) return position;
	moved.y = groundHeight;
	return moved;
}
