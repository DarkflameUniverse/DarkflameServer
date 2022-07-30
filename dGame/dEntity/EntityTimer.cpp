#include "EntityTimer.h"

EntityTimer::EntityTimer(std::string name, float time) {
	m_Name = name;
	m_Time = time;
}

EntityTimer::~EntityTimer() {

}

std::string EntityTimer::GetName() {
	return m_Name;
}

float EntityTimer::GetTime() {
	return m_Time;
}

void EntityTimer::Update(float deltaTime) {
	m_Time -= deltaTime;
}
