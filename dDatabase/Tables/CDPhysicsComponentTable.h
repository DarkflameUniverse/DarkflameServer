#pragma once
#include "CDTable.h"
#include <string>

struct CDPhysicsComponent {
	int id;
	bool bStatic;
	std::string physicsAsset;
	UNUSED(bool jump);
	UNUSED(bool doublejump);
	float speed;
	UNUSED(float rotSpeed);
	float playerHeight;
	float playerRadius;
	int pcShapeType;
	int collisionGroup;
	UNUSED(float airSpeed);
	UNUSED(std::string boundaryAsset);
	UNUSED(float jumpAirSpeed);
	UNUSED(float friction);
	UNUSED(std::string gravityVolumeAsset);
};

class CDPhysicsComponentTable : public CDTable<CDPhysicsComponentTable> {
public:
	CDPhysicsComponentTable();
	~CDPhysicsComponentTable();

	static const std::string GetTableName() { return "PhysicsComponent"; };
	CDPhysicsComponent* GetByID(unsigned int componentID);

private:
	std::map<unsigned int, CDPhysicsComponent*> m_entries;
};
