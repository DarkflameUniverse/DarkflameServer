#pragma once

#include <string>
#include <functional>

class EntityCallbackTimer {
public:
	EntityCallbackTimer(float time, std::function<void()> callback);
	~EntityCallbackTimer();

	std::function<void()> GetCallback();
	float GetTime();

	void Update(float deltaTime);

private:
	std::function<void()> m_Callback;
	float m_Time;
};
