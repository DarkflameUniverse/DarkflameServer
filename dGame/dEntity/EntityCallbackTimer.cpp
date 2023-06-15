#include "EntityCallbackTimer.h"

EntityCallbackTimer::EntityCallbackTimer(const float& time, const std::function<void()>& callback) {
	m_Time = time;
	m_Callback = callback;
}

void EntityCallbackTimer::ExecuteCallback() {
	m_Callback();
}

std::function<void()> EntityCallbackTimer::GetCallback() {
	return m_Callback;
}

float EntityCallbackTimer::GetTime() {
	return m_Time;
}

void EntityCallbackTimer::Update(float deltaTime) {
	m_Time -= deltaTime;
}
