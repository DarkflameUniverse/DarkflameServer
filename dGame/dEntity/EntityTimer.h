#pragma once

#include <string>

class EntityTimer {
public:
	EntityTimer(std::string name, float time);
	~EntityTimer();

	std::string GetName();
	float GetTime();

	void Update(float deltaTime);

private:
	std::string m_Name;
	float m_Time;
};
