#include "dpShapeBase.h"
#include "dpShapeBox.h"
#include "dpShapeSphere.h"
#include "dpCollisionChecks.h"
#include "dpEntity.h"

#include "NiPoint3.h"
#include "NiQuaternion.h"

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
		std::cout << "No collision detection for: " << (int)m_ShapeType << "-to-" << (int)other->GetShapeType() << " collision!" << std::endl;
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

	//fuuuckkk yoouu
	InitVertices();

	//SetRotation(m_ParentEntity->GetRotation());
}

void dpShapeBox::SetRotation(const NiQuaternion& rotation) {
	if (m_HasBeenRotated) return; //Boxes cannot be rotated more than once.
	m_HasBeenRotated = true;

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
	//if we are in the correct height
	if (vert.y >= m_MinY && vert.y <= m_MaxY) {

		//if we're inside the x bounds
		if (vert.x >= m_MinX && vert.x <= m_MaxX) {

			//if we're inside the z bounds
			if (vert.z >= m_MinZ && vert.z <= m_MaxZ)
				return true;
		}
	}

	return false;
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
