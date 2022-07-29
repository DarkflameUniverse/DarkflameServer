#pragma once

#include <vector>
#include <string>
#include "dCommonVars.h"
#include "Component.h"

/**
 * Component that handles the traveling using rails, e.g. the ninjago posts that can be used to travel using Spinjitzu.
 * Credits to https://github.com/UchuServer/Uchu/blob/dev/Uchu.World/Objects/Components/ReplicaComponents/RailActivatorComponent.cs
 */
class RailActivatorComponent final : public Component {
public:
	explicit RailActivatorComponent(Entity* parent, int32_t componentID);
	~RailActivatorComponent() override;

	static const uint32_t ComponentType = COMPONENT_TYPE_RAIL_ACTIVATOR;

	/**
	 * Handles the OnUse event from some entity, initiates the rail movement
	 * @param originator the entity that triggered the event
	 */
	void OnUse(Entity* originator) override;

	/**
	 * Event handler that's called when some entity has played the start animation for rail movement and now wants to
	 * start the actual movement.
	 * @param originator the entity that triggered the event
	 */
	void OnRailMovementReady(Entity* originator) const;

	/**
	 * Event handler that's called when some entity has finished traversing the rail and wants to end its interaction
	 * with it
	 * @param originator the entity that triggered the event
	 */
	void OnCancelRailMovement(Entity* originator);
private:

	/**
	 * The ID of this component in the components database
	 */
	int32_t m_ComponentID;

	/**
	 * The entities that are currently traversing the rail
	 */
	std::vector<LWOOBJID> m_EntitiesOnRail{};

	/**
	 * The path the entities will follow when traversing the rail
	 */
	std::u16string m_Path;

	/**
	 * The index of the path that is the start
	 */
	uint32_t m_PathStart;

	/**
	 * The direction on the path
	 */
	bool m_PathDirection;

	/**
	 * The animation that plays when starting the rail
	 */
	std::u16string m_StartAnimation;

	/**
	 * The animation that plays during the rail
	 */
	std::u16string m_LoopAnimation;

	/**
	 * The animation that plays after the rail
	 */
	std::u16string m_StopAnimation;

	/**
	 * The sound that plays at the start of the rail
	 */
	std::u16string m_StartSound;

	/**
	 * The sound that plays during the rail
	 */
	std::u16string m_loopSound;

	/**
	 * The sound that plays at the end of the rail
	 */
	std::u16string m_StopSound;

	/**
	 * The effects that play at the start of the rail
	 */
	std::pair<uint32_t, std::u16string> m_StartEffect;

	/**
	 * The effects that play during the rail
	 */
	std::pair<uint32_t, std::u16string> m_LoopEffect;

	/**
	 * The effects that play at the end of the rail
	 */
	std::pair<uint32_t, std::u16string> m_StopEffect;

	/**
	 * Client flag
	 */
	bool m_DamageImmune;

	/**
	 * Client flag
	 */
	bool m_NoAggro;

	/**
	 * Client flag
	 */
	bool m_UseDB;

	/**
	 * Client flag
	 */
	bool m_CameraLocked;

	/**
	 * Client flag
	 */
	bool m_CollisionEnabled;

	/**
	 * Client flag, notifies the server when the player finished the rail
	 */
	bool m_NotifyArrived;

	/**
	 * Client flag
	 */
	bool m_ShowNameBillboard;
};
