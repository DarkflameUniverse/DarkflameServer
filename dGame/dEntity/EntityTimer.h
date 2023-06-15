#pragma once

#include <string>

class EntityTimer {
public:
	EntityTimer(const std::string& name, const float& time);

	std::string GetName();
	float GetTime();

	void Update(float deltaTime);

private:
	std::string m_Name;
	float m_Time;
};
