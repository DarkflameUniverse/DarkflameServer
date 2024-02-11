#ifndef __PHYSICSCOMPONENT__H__
#define __PHYSICSCOMPONENT__H__

#include "Component.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"

namespace Raknet {
	class BitStream;
};

class PhysicsComponent : public Component {
public:
	PhysicsComponent(Entity* parent);
	virtual ~PhysicsComponent() = default;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) override;

	const NiPoint3& GetPosition() const { return m_Position; }
	virtual void SetPosition(const NiPoint3& pos) { if (m_Position == pos) return; m_Position = pos; m_DirtyPosition = true; }

	const NiQuaternion& GetRotation() const { return m_Rotation; }
	virtual void SetRotation(const NiQuaternion& rot) { if (m_Rotation == rot) return; m_Rotation = rot; m_DirtyPosition = true; }
protected:
	NiPoint3 m_Position;

	NiQuaternion m_Rotation;

	bool m_DirtyPosition;
};

#endif  //!__PHYSICSCOMPONENT__H__
