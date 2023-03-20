#pragma once
#include "CDTable.h"
#include <string>

struct CDPhysicsComponent {
	int id;
	bool bStatic;
	std::string physicsAsset;
	UNUSED_COLUMN(bool jump);
	UNUSED_COLUMN(bool doublejump);
	float speed;
	UNUSED_COLUMN(float rotSpeed);
	float playerHeight;
	float playerRadius;
	int pcShapeType;
	int collisionGroup;
	UNUSED_COLUMN(float airSpeed);
	UNUSED_COLUMN(std::string boundaryAsset);
	UNUSED_COLUMN(float jumpAirSpeed);
	UNUSED_COLUMN(float friction);
	UNUSED_COLUMN(std::string gravityVolumeAsset);
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
