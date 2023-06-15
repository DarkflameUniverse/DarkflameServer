#pragma once

#include <string>
#include <functional>

class EntityCallbackTimer {
public:
	EntityCallbackTimer(const float& time, const std::function<void()>& callback);

	void ExecuteCallback();
	std::function<void()> GetCallback();
	float GetTime();

	void Update(float deltaTime);

private:
	std::function<void()> m_Callback;
	float m_Time;
};
