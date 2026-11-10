#pragma once

#include <vector>
#include <string>
#include "dCommonVars.h"
#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Component that handles the traveling using rails, e.g. the ninjago posts that can be used to travel using Spinjitzu.
 * Credits to https://github.com/UchuServer/Uchu/blob/dev/Uchu.World/Objects/Components/ReplicaComponents/RailActivatorComponent.cs
 */
class RailActivatorComponent final : public Component {
public:
	explicit RailActivatorComponent(Entity* parent, const int32_t componentID);
	~RailActivatorComponent() override;

	void LoadDataFromTemplate() override;
	void LoadConfigData() override;

	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::RAIL_ACTIVATOR;

	/**
	 * Handles a RequestUse from some entity, initiates the rail movement
	 * @param originator the entity that triggered the event
	 */
	bool OnRequestUse(GameMessages::RequestUseEvent& event);

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

	bool GetDamageImmune() const { return m_DamageImmune; }

	/**
	 * Whether the rail can be used (level key rail_activator_active; every rail in the live levels has it set)
	 */
	bool GetActive() const { return m_Active; }

	// The players that used the rail and have not left it yet
	const std::vector<LWOOBJID>& GetEntitiesOnRail() const { return m_EntitiesOnRail; }
	bool GetNoAggro() const { return m_NoAggro; }
	bool GetShowNameBillboard() const { return m_ShowNameBillboard; }
private:
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

	bool m_Active{ true };

	/**
	 * Client flag
	 */
	bool m_NoAggro;

	/**
	 * Client flag
	 */
	bool m_UseDB;

	/**
	 * The row's preconditions a player has to pass to use the rail
	 */
	std::string m_Preconditions;

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
