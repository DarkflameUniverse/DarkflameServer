#ifndef __PHYSICSCOMPONENT__H__
#define __PHYSICSCOMPONENT__H__

#include "Component.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"

namespace Raknet {
	class BitStream;
};

enum class eReplicaComponentType : uint32_t;

class dpEntity;

class PhysicsComponent : public Component {
public:
	PhysicsComponent(Entity* parent, int32_t componentId);
	virtual ~PhysicsComponent() = default;

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

	const NiPoint3& GetPosition() const noexcept { return m_Position; }
	virtual void SetPosition(const NiPoint3& pos) { if (m_Position == pos) return; m_Position = pos; m_DirtyPosition = true; }

	const NiQuaternion& GetRotation() const { return m_Rotation; }
	virtual void SetRotation(const NiQuaternion& rot) { if (m_Rotation == rot) return; m_Rotation = rot; m_DirtyPosition = true; }

	int32_t GetCollisionGroup() const noexcept { return m_CollisionGroup; }
	void SetCollisionGroup(int32_t group) noexcept { m_CollisionGroup = group; }
protected:
	dpEntity* CreatePhysicsEntity(eReplicaComponentType type);

	dpEntity* CreatePhysicsLnv(const float scale, const eReplicaComponentType type) const;

	void SpawnVertices(dpEntity* entity) const;

	bool OnGetPosition(GameMessages::GameMsg& msg);

	NiPoint3 m_Position;

	NiQuaternion m_Rotation;

	bool m_DirtyPosition;

	int32_t m_CollisionGroup{};
};

#endif  //!__PHYSICSCOMPONENT__H__
