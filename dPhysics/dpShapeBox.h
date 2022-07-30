#pragma once
#include "dpShapeBase.h"
#include <vector>
#include "NiPoint3.h"
#include "NiQuaternion.h"

class dpShapeBox : public dpShapeBase {
public:
	dpShapeBox(dpEntity* parentEntity, float width, float height, float depth);
	~dpShapeBox();

	bool IsColliding(dpShapeBase* other);

	const float GetScale() const { return m_Scale; }

	const float GetWidth() const { return m_Width; }
	const float GetHeight() const { return m_Height; }
	const float GetDepth() const { return m_Depth; }

	const float GetMaxWidth();
	const float GetTop();
	const float GetMaxDepth();

	const float GetMinWidth();
	const float GetBottom();
	const float GetMinDepth();

	void SetScale(float scale);

	void SetRotation(const NiQuaternion& rotation);

	const std::vector<NiPoint3>& GetVertices() const { return m_Vertices; }

	bool IsVertInBox(const NiPoint3& vert);

	void InitVertices();

	void SetPosition(const NiPoint3& position);

	//idc atm
	float m_MinX = 9999.0f;
	float m_MaxX = -9999.0f;
	float m_MinY = 9999.0f;
	float m_MaxY = -9999.0f;
	float m_MinZ = 9999.0f;
	float m_MaxZ = -9999.0f;

private:
	float m_Width; //X
	float m_Height; //Y
	float m_Depth; //Z

	std::vector<NiPoint3> m_Vertices;

	NiPoint3 m_TopMinLeft;
	NiPoint3 m_TopMinRight;
	NiPoint3 m_TopMaxLeft;
	NiPoint3 m_TopMaxRight;

	NiPoint3 m_BottomMinLeft;
	NiPoint3 m_BottomMinRight;
	NiPoint3 m_BottomMaxLeft;
	NiPoint3 m_BottomMaxRight;

	float m_Scale;

	bool m_HasBeenRotated = false;
	bool isScaled = false;
	bool isTransformed = false;

	void InsertVertices();
};
