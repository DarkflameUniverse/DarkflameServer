#pragma once

#include <string>
#include <functional>

class EntityCallbackTimer {
public:
	EntityCallbackTimer(const float time, const std::function<void()> callback);
	
	std::function<void()> GetCallback() const { return m_Callback; };

	float GetTime() const { return m_Time; };

	void Update(float deltaTime);

private:
	std::function<void()> m_Callback;
	float m_Time;
};
