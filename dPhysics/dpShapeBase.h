#pragma once
#include "dpCommon.h"

class dpEntity;

class dpShapeBase {
public:
	dpShapeBase(dpEntity* parentEntity);
	virtual ~dpShapeBase();

	virtual bool IsColliding(dpShapeBase* other) = 0;

	const dpShapeType& GetShapeType() const { return m_ShapeType; }

	dpEntity* GetParentEntity() const { return m_ParentEntity; }

protected:
	dpEntity* m_ParentEntity;
	dpShapeType m_ShapeType = dpShapeType::Invalid;
};
