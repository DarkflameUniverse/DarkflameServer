#pragma once

#include <string>

class EntityTimer {
public:
	EntityTimer(const std::string& name, const float time);

	bool operator==(const EntityTimer& other) const {
		return m_Name == other.m_Name;
	}

	bool operator==(const std::string& other) const {
		return m_Name == other;
	}

	std::string GetName();
	float GetTime();

	void Update(float deltaTime);

private:
	std::string m_Name;
	float m_Time;
};
