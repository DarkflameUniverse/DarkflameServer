#pragma once
#include "dpShapeBase.h"

class dpShapeSphere : public dpShapeBase {
public:
	dpShapeSphere(dpEntity* parentEntity, float radius);
	~dpShapeSphere();

	bool IsColliding(dpShapeBase* other);

	const float GetRadius() const { return m_Radius; }

	void SetScale(float scale) { m_Radius = scale; }

private:
	float m_Radius;
};
