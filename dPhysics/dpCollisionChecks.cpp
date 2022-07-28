#include "dpCollisionChecks.h"
#include "dpEntity.h"
#include "dpShapeBase.h"
#include "dpShapeSphere.h"
#include "dpShapeBox.h"

#include <iostream>
#include <algorithm>

using namespace dpCollisionChecks;

bool dpCollisionChecks::AreColliding(dpEntity* a, dpEntity* b) {
	auto shapeA = a->GetShape();
	auto shapeB = b->GetShape();

	//Sphere to sphere collision
	if (shapeA->GetShapeType() == dpShapeType::Sphere && shapeB->GetShapeType() == dpShapeType::Sphere) {
		return CheckSpheres(a, b);
	}

	return false;
}

bool dpCollisionChecks::CheckSpheres(dpEntity* a, dpEntity* b) {
	if (!a || !b) return false;

	auto posA = a->GetPosition();
	auto distance = Vector3::DistanceSquared(posA, b->GetPosition());

	auto sphereA = static_cast<dpShapeSphere*>(a->GetShape());
	auto sphereB = static_cast<dpShapeSphere*>(b->GetShape());
	const auto radius = sphereA->GetRadius() + sphereB->GetRadius();

	if (distance <= radius * radius)
		return true;

	return false;
}

bool dpCollisionChecks::CheckBoxes(dpEntity* a, dpEntity* b) {
	if (!a || !b) return false;

	auto boxA = static_cast<dpShapeBox*>(a->GetShape());
	auto boxB = static_cast<dpShapeBox*>(b->GetShape());

	const auto& posA = a->GetPosition();
	const auto& posB = b->GetPosition();

	for (const auto& vert : boxA->GetVertices()) {
		if (boxB->IsVertInBox(vert))
			return true;
	}

	/*//Check if we're overlapping on X/Z:
	if ((boxA->GetMaxWidth() >= boxB->GetMinWidth()) && //If our max width is greater than starting X of b
		(boxA->GetMinWidth() <= boxB->GetMaxWidth()) && //If our start x is less than b's max width
		(boxA->GetMaxDepth() >= boxB->GetMinDepth()) &&
		(boxA->GetMinDepth() <= boxB->GetMaxDepth())) {

		//Check if we're in the right height
		if (boxA->GetTop() <= boxB->GetTop() && boxA->GetTop() >= boxB->GetBottom() || //If our top Y is within their minY/maxY bounds
			boxA->GetBottom() <= boxB->GetTop() && boxA->GetBottom() >= boxB->GetBottom()) //Or our bottom Y
			return true; //We definitely are colliding.
	}*/

	/*//Check if we're overlapping on X/Z:
	if ((boxA->GetMaxWidth() >= posB.x) && //If our max width is greater than starting X of b
		(posA.x <= boxB->GetMaxWidth()) && //If our start x is less than b's max width
		(boxA->GetMaxDepth() >= posB.z) &&
		(posA.z <= boxB->GetMaxDepth())) {

		//Check if we're in the right height
		if (boxA->GetTop() <= boxB->GetTop() && boxA->GetTop() >= posB.y || //If our top Y is within their minY/maxY bounds
			posA.y <= boxB->GetTop() && posA.y >= posB.y) //Or our bottom Y
			return true; //We definitely are colliding.
	}*/

	return false;
}

bool dpCollisionChecks::CheckSphereBox(dpEntity* a, dpEntity* b) {
	if (!a || !b) return false;

	NiPoint3 boxPos;
	dpShapeBox* box;

	NiPoint3 spherePos;
	dpShapeSphere* sphere;

	//Figure out which is the box and which is the sphere
	if (a->GetShape()->GetShapeType() == dpShapeType::Box) {
		box = static_cast<dpShapeBox*>(a->GetShape());
		sphere = static_cast<dpShapeSphere*>(b->GetShape());
		boxPos = a->GetPosition();
		spherePos = b->GetPosition();
	} else {
		box = static_cast<dpShapeBox*>(b->GetShape());
		sphere = static_cast<dpShapeSphere*>(a->GetShape());
		boxPos = b->GetPosition();
		spherePos = a->GetPosition();
	}

	//Get closest point from the box to the sphere center by clamping
	float x = std::max(box->m_MinX, std::min(spherePos.x, box->m_MaxX));
	float y = std::max(box->m_MinY, std::min(spherePos.y, box->m_MaxY));
	float z = std::max(box->m_MinZ, std::min(spherePos.z, box->m_MaxZ));

	//Check the distance between that point & our sphere
	float dX = x - spherePos.x;
	float dY = y - spherePos.y;
	float dZ = z - spherePos.z;
	float distanceSquared = (dX * dX) + (dY * dY) + (dZ * dZ);
	const float radius = sphere->GetRadius();

	return distanceSquared < radius* radius;
}
