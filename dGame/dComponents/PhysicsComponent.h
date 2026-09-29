#ifndef __PHYSICSCOMPONENT__H__
#define __PHYSICSCOMPONENT__H__

#include "Component.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"

namespace GameMessages {
	struct GetObjectReportInfo;
	struct GetPosition;
};

namespace Raknet {
	class BitStream;
};

enum class eReplicaComponentType : uint32_t;

class dpEntity;

class PhysicsComponent : public Component {
public:
	PhysicsComponent(Entity* parent, const int32_t componentID);
	virtual ~PhysicsComponent();

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

	const NiPoint3& GetPosition() const noexcept { return m_Position; }
	virtual void SetPosition(const NiPoint3& pos) { if (m_Position == pos) return; m_Position = pos; m_DirtyPosition = true; }

	const NiQuaternion& GetRotation() const { return m_Rotation; }
	virtual void SetRotation(const NiQuaternion& rot) { if (m_Rotation == rot) return; m_Rotation = rot; m_DirtyPosition = true; }

	int32_t GetCollisionGroup() const noexcept { return m_CollisionGroup; }
	void SetCollisionGroup(int32_t group) noexcept { m_CollisionGroup = group; }
protected:
	bool OnGetObjectReportInfo(GameMessages::GetObjectReportInfo& msg);

	// isFallback, when given, says whether the asset had no known shape and got a stand in cube
	dpEntity* CreatePhysicsEntity(eReplicaComponentType type, bool* isFallback = nullptr);

	/**
	 * Makes this object a wall the server's movers can't walk through when its data says it is one
	 * (dpMovementBlockers::BlockingFilter: a navmesh carver, or a solid object only enemies collide with). Only
	 * shapes the server knows are used; a stand in cube would be a guess.
	 */
	void RegisterMovementBlocker(eReplicaComponentType type, bool solid, float scale);

	dpEntity* CreatePhysicsLnv(const float scale, const eReplicaComponentType type) const;

	void SpawnVertices(dpEntity* entity) const;

	bool OnGetPosition(GameMessages::GetPosition& msg);

	NiPoint3 m_Position;

	NiQuaternion m_Rotation = QuatUtils::IDENTITY;

	bool m_DirtyPosition;

	int32_t m_CollisionGroup{};

	// This object's shape as a movement blocker, owned here and not stepped with the physics world
	dpEntity* m_MovementBlocker{};
};

#endif  //!__PHYSICSCOMPONENT__H__
