#include "EntityCallbackTimer.h"

EntityCallbackTimer::EntityCallbackTimer(float time, std::function<void()> callback) {
	m_Time = time;
	m_Callback = callback;
}

EntityCallbackTimer::~EntityCallbackTimer() {

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
