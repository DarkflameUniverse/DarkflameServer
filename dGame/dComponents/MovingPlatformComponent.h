/*
 * Darkflame Universe
 * Copyright 2019
 */

#ifndef MOVINGPLATFORMCOMPONENT_H
#define MOVINGPLATFORMCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"

#include <string>
#include <memory>

class Path;
class Entity;
class PlatformSubComponent;
class RotatorSubComponent;

/**
 * Different types of available platform subcomponents
 */
enum class eMoverSubComponentType : uint32_t {
	mover = 4,
	simpleMover = 5,
	rotator = 6,
};

/**
 * Represents entities that may be moving platforms, indicating how they should move through the world.
 * The server simulates platform movement each tick to maintain authoritative state for all players.
 *
 * An entity can have multiple subcomponents (mover + rotator), matching the client's architecture
 * where SetupPlatform creates subcomponents based on platformIsMover/platformIsSimpleMover/platformIsRotater.
 */
class MovingPlatformComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MOVING_PLATFORM;

	MovingPlatformComponent(Entity* parent, const int32_t componentID, const std::string& pathName);
	~MovingPlatformComponent() override;

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;
	void Update(float deltaTime) override;

	void OnQuickBuildInitilized();
	void OnCompleteQuickBuild();

	void GotoWaypoint(uint32_t index, bool stopAtWaypoint = true);
	void StartPathing();
	void StopPathing();

	void SetSerialized(bool value);
	bool GetNoAutoStart() const;
	void SetNoAutoStart(bool value);

	void WarpToWaypoint(size_t index);
	size_t GetLastWaypointIndex() const;

	PlatformSubComponent* GetMoverSubComponent() const;
	void SetMovementState(uint32_t state);

private:
	void SetupPlatformSubComponents();

	const Path* m_Path = nullptr;
	std::u16string m_PathName;
	bool m_PathingStopped = false;
	eMoverSubComponentType m_MoverSubComponentType;

	std::unique_ptr<PlatformSubComponent> m_MoverSubComponent;
	std::unique_ptr<RotatorSubComponent> m_RotatorSubComponent;

	bool m_NoAutoStart = false;
	bool m_Serialize = false;

	std::string m_PlatformSoundStart;
	std::string m_PlatformSoundTravel;
	std::string m_PlatformSoundStop;
};

#endif // MOVINGPLATFORMCOMPONENT_H
