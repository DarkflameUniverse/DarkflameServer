//This file included for reference only

/*#include <iostream>
#include <chrono>
#include <thread>
#include "dpWorld.h"

#include "NiQuaternion.hpp"
#include "NiPoint3.hpp"

int main() {
	std::cout << "dPhysics test engine" << std::endl;

	//Test rotation code:
	NiPoint3 p(1.0f, 0.0f, 0.0f);

	float angle = 45.0f;
	NiQuaternion q = NiQuaternion::CreateFromAxisAngle(NiPoint3(0.0f, 0.0f, 1.0f), angle);

	NiPoint3 rotated = p.RotateByQuaternion(q);

	std::cout << "OG: " << p.x << ", " << p.y << ", " << p.z << std::endl;
	std::cout << "Quater: " << q.x << ", " << q.y << ", " << q.z << ", " << q.w << " angle: " << angle << std::endl;
	std::cout << "Rotated: " << rotated.x << ", " << rotated.y << ", " << rotated.z << std::endl;

	//Test some collisions:
	dpWorld::GetInstance().Initialize(1000);

	while (true) {
		dpWorld::GetInstance().StepWorld(1.0f/60.0f);
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	return 0;
}*/
