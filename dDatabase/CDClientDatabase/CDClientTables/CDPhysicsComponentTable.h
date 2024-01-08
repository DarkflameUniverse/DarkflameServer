#pragma once
#include "CDTable.h"
#include <cstdint>
#include <string>

struct CDPhysicsComponent {
	uint32_t id;
	bool bStatic;
	std::string physicsAsset;
	UNUSED(bool jump);
	UNUSED(bool doubleJump);
	float speed;
	UNUSED(float rotSpeed);
	float playerHeight;
	float playerRadius;
	int32_t pcShapeType;
	int32_t collisionGroup;
	UNUSED(float airSpeed);
	UNUSED(std::string boundaryAsset);
	UNUSED(float jumpAirSpeed);
	UNUSED(float friction);
	UNUSED(std::string gravityVolumeAsset);
};

class CDPhysicsComponentTable : public CDTable<CDPhysicsComponentTable> {
public:
	void LoadValuesFromDatabase();

	CDPhysicsComponent* GetByID(const uint32_t componentID);

private:
	std::map<uint32_t, CDPhysicsComponent> m_Entries;
};
