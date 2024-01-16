#include "EntityCallbackTimer.h"

EntityCallbackTimer::EntityCallbackTimer(const float time, const std::function<void()> callback) {
	m_Time = time;
	m_Callback = callback;
}

void EntityCallbackTimer::Update(float deltaTime) {
	m_Time -= deltaTime;
}
