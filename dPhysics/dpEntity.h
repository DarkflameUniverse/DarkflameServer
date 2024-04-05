#pragma once
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include <vector>
#include <unordered_set>
#include <span>

#include "dCommonVars.h"
#include "dpCommon.h"
#include "dpShapeBase.h"
#include "dpCollisionGroups.h"
#include "dpGrid.h"

class dpEntity {
	friend class dpGrid; //using friend here for now so grid can access everything

public:
	dpEntity(const LWOOBJID& objectID, dpShapeType shapeType, bool isStatic = true);
	dpEntity(const LWOOBJID& objectID, NiPoint3 boxDimensions, bool isStatic = true);
	dpEntity(const LWOOBJID& objectID, float width, float height, float depth, bool isStatic = true);
	dpEntity(const LWOOBJID& objectID, float radius, bool isStatic = true);

	~dpEntity();

	void Update(float deltaTime);

	void CheckCollision(dpEntity* other);

	const NiPoint3& GetPosition() const { return m_Position; }
	const NiQuaternion& GetRotation() const { return m_Rotation; }
	const float GetScale() const { return m_Scale; }

	const NiPoint3& GetVelocity() const { return m_Velocity; }
	const NiPoint3& GetAngularVelocity() const { return m_AngularVelocity; }

	void SetPosition(const NiPoint3& newPos);
	void SetRotation(const NiQuaternion& newRot);
	void SetScale(float newScale);

	void SetVelocity(const NiPoint3& newVelocity);
	void SetAngularVelocity(const NiPoint3& newAngularVelocity);

	dpShapeBase* GetShape() { return m_CollisionShape; }

	bool GetIsStatic() const { return m_IsStatic; }

	uint8_t GetCollisionGroup() const { return m_CollisionGroup; }
	void SetCollisionGroup(uint8_t value) { m_CollisionGroup = value; }

	bool GetSleeping() const { return m_Sleeping; }
	void SetSleeping(bool value) { m_Sleeping = value; }

	std::span<const LWOOBJID> GetNewObjects() const { return m_NewObjects; }
	std::span<const LWOOBJID> GetRemovedObjects() const { return m_RemovedObjects; }
	const std::unordered_set<LWOOBJID>& GetCurrentlyCollidingObjects() const { return m_CurrentlyCollidingObjects; }

	void PreUpdate() { m_NewObjects.clear();  m_RemovedObjects.clear(); }

	const LWOOBJID& GetObjectID() const { return m_ObjectID; }

	void SetGrid(dpGrid* grid);

	bool GetIsGargantuan() const { return m_IsGargantuan; }

private:
	LWOOBJID m_ObjectID;
	dpShapeBase* m_CollisionShape;
	bool m_IsStatic;

	NiPoint3 m_Position;
	NiQuaternion m_Rotation;
	float m_Scale;

	NiPoint3 m_Velocity;
	NiPoint3 m_AngularVelocity;

	dpGrid* m_Grid = nullptr;

	uint8_t m_CollisionGroup;
	bool m_Sleeping = false;

	bool m_IsGargantuan = false;

	std::vector<LWOOBJID> m_NewObjects;
	std::vector<LWOOBJID> m_RemovedObjects;
	std::unordered_set<LWOOBJID> m_CurrentlyCollidingObjects;
};
