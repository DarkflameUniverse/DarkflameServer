#pragma once
class dpEntity;

namespace dpCollisionChecks {
	bool AreColliding(dpEntity* a, dpEntity* b);

	bool CheckSpheres(dpEntity* a, dpEntity* b);

	bool CheckBoxes(dpEntity* a, dpEntity* b);

	bool CheckSphereBox(dpEntity* a, dpEntity* b);
};
