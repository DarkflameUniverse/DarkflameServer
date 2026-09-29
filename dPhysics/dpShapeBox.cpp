#include "dpShapeBase.h"
#include "dpShapeBox.h"
#include "dpShapeSphere.h"
#include "dpCollisionChecks.h"
#include "dpEntity.h"

#include "NiPoint3.h"
#include "NiQuaternion.h"

#include <algorithm>
#include <cmath>
#include <iostream>

dpShapeBox::dpShapeBox(dpEntity* parentEntity, float width, float height, float depth) :
	dpShapeBase(parentEntity),
	m_Width(width / 2),
	m_Height(height / 2),
	m_Depth(depth / 2),
	m_Scale(1.0f) {
	m_ShapeType = dpShapeType::Box;

	InitVertices();
}

dpShapeBox::~dpShapeBox() {
}

bool dpShapeBox::IsColliding(dpShapeBase* other) {
	if (!other) return false;

	switch (other->GetShapeType()) {
	case dpShapeType::Sphere:
		return dpCollisionChecks::CheckSphereBox(m_ParentEntity, other->GetParentEntity());

	case dpShapeType::Box:
		return dpCollisionChecks::CheckBoxes(m_ParentEntity, other->GetParentEntity());

	default:
		LOG("No collision detection for: %i-to-%i collision!", static_cast<int32_t>(m_ShapeType), static_cast<int32_t>(other->GetShapeType()));
	}

	return false;
}

const float dpShapeBox::GetMaxWidth() {
	return m_ParentEntity->GetPosition().x + m_Width;
}

const float dpShapeBox::GetTop() {
	return m_ParentEntity->GetPosition().y + (m_Height * 2);
}

const float dpShapeBox::GetMaxDepth() {
	return m_ParentEntity->GetPosition().z + m_Depth;
}

const float dpShapeBox::GetMinWidth() {
	return m_ParentEntity->GetPosition().x - m_Width;
}

const float dpShapeBox::GetBottom() {
	return m_ParentEntity->GetPosition().y; //- m_Height;
}

const float dpShapeBox::GetMinDepth() {
	return m_ParentEntity->GetPosition().z - m_Depth;
}

void dpShapeBox::SetScale(float scale) {
	if (isScaled) return;
	isScaled = true;

	m_Width *= scale;
	m_Height *= scale;
	m_Depth *= scale;

	InitVertices();
}

void dpShapeBox::SetRotation(const NiQuaternion& rotation) {
	if (m_HasBeenRotated) return; //Boxes cannot be rotated more than once.
	m_HasBeenRotated = true;
	m_Orientation = rotation;

	m_TopMinLeft = m_TopMinLeft.RotateByQuaternion(rotation);
	m_TopMaxLeft = m_TopMaxLeft.RotateByQuaternion(rotation);
	m_TopMinRight = m_TopMinRight.RotateByQuaternion(rotation);
	m_TopMaxRight = m_TopMaxRight.RotateByQuaternion(rotation);

	m_BottomMinLeft = m_BottomMinLeft.RotateByQuaternion(rotation);
	m_BottomMinRight = m_BottomMinRight.RotateByQuaternion(rotation);
	m_BottomMaxLeft = m_BottomMaxLeft.RotateByQuaternion(rotation);
	m_BottomMaxRight = m_BottomMaxRight.RotateByQuaternion(rotation);

	InsertVertices();
}

bool dpShapeBox::IsVertInBox(const NiPoint3& vert) {
	return SquaredDistanceTo(vert) <= 0.0f;
}

float dpShapeBox::SquaredDistanceTo(const NiPoint3& point) const {
	// Into the box's frame: its origin is the middle of its bottom face
	const auto local = (point - m_Origin).RotateByQuaternion(glm::conjugate(m_Orientation));

	const float dX = local.x - std::clamp(local.x, -m_Width, m_Width);
	const float dY = local.y - std::clamp(local.y, 0.0f, m_Height * 2.0f);
	const float dZ = local.z - std::clamp(local.z, -m_Depth, m_Depth);
	return dX * dX + dY * dY + dZ * dZ;
}

std::optional<float> dpShapeBox::SegmentEntry(const NiPoint3& a, const NiPoint3& b) const {
	// Into the box's frame (origin the middle of its bottom face), then clip the segment against each pair of faces
	const auto inverse = glm::conjugate(m_Orientation);
	const auto localA = (a - m_Origin).RotateByQuaternion(inverse);
	const auto localB = (b - m_Origin).RotateByQuaternion(inverse);

	const float start[3]{ localA.x, localA.y, localA.z };
	const float delta[3]{ localB.x - localA.x, localB.y - localA.y, localB.z - localA.z };
	const float min[3]{ -m_Width, 0.0f, -m_Depth };
	const float max[3]{ m_Width, m_Height * 2.0f, m_Depth };

	bool startsInside = true;
	float enter = 0.0f;
	float exit = 1.0f;
	for (int axis = 0; axis < 3; axis++) {
		if (start[axis] < min[axis] || start[axis] > max[axis]) startsInside = false;
		if (std::abs(delta[axis]) < 1e-6f) {
			if (start[axis] < min[axis] || start[axis] > max[axis]) return std::nullopt;
			continue;
		}
		float tNear = (min[axis] - start[axis]) / delta[axis];
		float tFar = (max[axis] - start[axis]) / delta[axis];
		if (tNear > tFar) std::swap(tNear, tFar);
		enter = std::max(enter, tNear);
		exit = std::min(exit, tFar);
		if (enter > exit) return std::nullopt;
	}
	if (startsInside) return std::nullopt;
	return enter;
}

void dpShapeBox::InitVertices() {
	//The four top verts
	m_TopMinLeft = NiPoint3(GetMinWidth(), GetTop(), GetMinDepth());
	m_TopMaxLeft = NiPoint3(GetMinWidth(), GetTop(), GetMaxDepth());

	m_TopMinRight = NiPoint3(GetMaxWidth(), GetTop(), GetMinDepth());
	m_TopMaxRight = NiPoint3(GetMaxWidth(), GetTop(), GetMaxDepth());

	//The four bottom verts
	m_BottomMinLeft = NiPoint3(GetMinWidth(), GetBottom(), GetMinDepth());
	m_BottomMaxLeft = NiPoint3(GetMinWidth(), GetBottom(), GetMaxDepth());

	m_BottomMinRight = NiPoint3(GetMaxWidth(), GetBottom(), GetMinDepth());
	m_BottomMaxRight = NiPoint3(GetMaxWidth(), GetBottom(), GetMaxDepth());

	InsertVertices();
}

void dpShapeBox::SetPosition(const NiPoint3& position) {
	if (isTransformed) return;
	isTransformed = true;
	m_Origin = position;

	for (auto& vert : m_Vertices) {
		vert.x += position.x;
		vert.y += position.y;
		vert.z += position.z;
	}

	m_TopMinLeft = m_Vertices[0];
	m_TopMaxLeft = m_Vertices[1];
	m_TopMinRight = m_Vertices[2];
	m_TopMaxRight = m_Vertices[3];

	m_BottomMinLeft = m_Vertices[4];
	m_BottomMaxLeft = m_Vertices[5];
	m_BottomMinRight = m_Vertices[6];
	m_BottomMaxRight = m_Vertices[7];

	for (auto& vert : m_Vertices) {
		if (m_MinX >= vert.x) m_MinX = vert.x;
		if (m_MinY >= vert.y) m_MinY = vert.y;
		if (m_MinZ >= vert.z) m_MinZ = vert.z;

		if (m_MaxX <= vert.x) m_MaxX = vert.x;
		if (m_MaxY <= vert.y) m_MaxY = vert.y;
		if (m_MaxZ <= vert.z) m_MaxZ = vert.z;
	}
}

void dpShapeBox::InsertVertices() {
	//Insert into our vector:
	m_Vertices.clear();
	m_Vertices.push_back(m_TopMinLeft);
	m_Vertices.push_back(m_TopMaxLeft);
	m_Vertices.push_back(m_TopMinRight);
	m_Vertices.push_back(m_TopMaxRight);

	m_Vertices.push_back(m_BottomMinLeft);
	m_Vertices.push_back(m_BottomMaxLeft);
	m_Vertices.push_back(m_BottomMinRight);
	m_Vertices.push_back(m_BottomMaxRight);
}
