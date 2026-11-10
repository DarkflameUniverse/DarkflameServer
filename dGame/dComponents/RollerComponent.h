#ifndef ROLLERCOMPONENT_H
#define ROLLERCOMPONENT_H

#include "BitStream.h"
#include "Component.h"
#include "NiPoint3.h"
#include "dCommonVars.h"
#include "eReplicaComponentType.h"

/**
 * Roller (a launcher a player fires from: LOTs 2714, 13951), as the client's LWORollerComponent makes it from the
 * registry. Its construction carries the activity camera and the projectile skill; nobody uses one on the server
 * yet, so the block about its user is never sent.
 */
class RollerComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ROLLER;

	// The projectile skill the client's constructor starts with (LWORollerComponent::Initialize 0x00cc7e30)
	static constexpr uint32_t DEFAULT_PROJECTILE_SKILL = 1348;

	RollerComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}

	// The level's activity_camera_pos and activity_camera_lookat_pos (LWORollerComponent::LoadConfigData 0x00c31820);
	// both stay at the origin without the key, as in the client
	void LoadConfigData() override;

	// LWORollerComponent::Deserialize (0x00c15920) reads on construction: the camera position and what it looks at
	// (3 floats each) and the projectile skill (u32). Then, in constructions and updates alike, a flag in front of the
	// block about the player using it (3 x u64, 3 floats, a float, 3 floats, the player u64, a float): never set here.
	void Serialize(RakNet::BitStream& outBitStream, const bool isConstruction) override {
		if (isConstruction) {
			outBitStream.Write(m_CameraPosition.x);
			outBitStream.Write(m_CameraPosition.y);
			outBitStream.Write(m_CameraPosition.z);
			outBitStream.Write(m_CameraLookAt.x);
			outBitStream.Write(m_CameraLookAt.y);
			outBitStream.Write(m_CameraLookAt.z);
			outBitStream.Write(m_ProjectileSkill);
		}
		outBitStream.Write0();
	}

	const NiPoint3& GetCameraPosition() const { return m_CameraPosition; }
	const NiPoint3& GetCameraLookAt() const { return m_CameraLookAt; }
	uint32_t GetProjectileSkill() const { return m_ProjectileSkill; }

private:
	NiPoint3 m_CameraPosition = NiPoint3Constant::ZERO;
	NiPoint3 m_CameraLookAt = NiPoint3Constant::ZERO;
	uint32_t m_ProjectileSkill = DEFAULT_PROJECTILE_SKILL;
};

#endif // ROLLERCOMPONENT_H
