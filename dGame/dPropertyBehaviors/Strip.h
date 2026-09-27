#ifndef __STRIP__H__
#define __STRIP__H__

#include "Action.h"
#include "StripUiPosition.h"

#include <vector>

namespace tinyxml2 {
	class XMLElement;
}

class AMFArrayValue;
class ModelComponent;
struct UpdateResult;

class Strip {
public:
	template <typename Msg>
	void HandleMsg(Msg& msg);

	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;
	bool IsEmpty() const noexcept { return m_Actions.empty(); }

	void Serialize(tinyxml2::XMLElement& strip) const;
	void Deserialize(const tinyxml2::XMLElement& strip);

	const Action& GetNextAction() const;
	const Action& GetPreviousAction() const;

	void IncrementAction();
	void Spawn(LOT object, Entity& entity);

	// Checks the movement logic for whether or not to proceed
	// Returns true if the movement can continue, false if it needs to wait more.
	bool CheckMovement(float deltaTime, ModelComponent& modelComponent);

	// Checks the rotation logic for whether or not to proceed
	// Returns true if the rotation can continue, false if it needs to wait more.
	bool CheckRotation(float deltaTime, ModelComponent& modelComponent);
	void Update(float deltaTime, ModelComponent& modelComponent, UpdateResult& updateResult);
	void SpawnDrop(LOT dropLOT, Entity& entity);
	void ProcNormalAction(float deltaTime, ModelComponent& modelComponent, UpdateResult& updateResult);
	void RemoveStates(ModelComponent& modelComponent) const;

	// 2 actions are required for strips to work
	bool HasMinimumActions() const { return m_Actions.size() >= 2; }
	
	void OnChatMessageReceived(const std::string& sMessage, const LWOOBJID sender);
	void OnHit(const LWOOBJID attacker);
private:
	// Indicates this Strip is waiting for an action to be taken upon it to progress to its actions
	bool m_WaitingForAction{ false };

	// The amount of time this strip is paused for. Any interactions with this strip should be bounced if this is greater than 0.
	// Actions that do not use time do not use this (ex. positions).
	float m_PausedTime{ 0.0f };
	bool m_PausedFromOnTimer{ false };

	// The index of the next action to be played. This should always be within range of [0, m_Actions.size()).
	size_t m_NextActionIndex{ 0 };

	// The list of actions to be executed on this behavior.
	std::vector<Action> m_Actions;

	// The location of this strip on the UGBehaviorEditor UI
	StripUiPosition m_Position;

	// The current actions remaining translation to the target along the model's local right (x), up (y) and forward (z) axes.
	// Only 1 of these vertexs' will be active at once for any given strip.
	NiPoint3 m_InActionTranslation{};

	// The position of the parent model on the previous frame
	NiPoint3 m_PreviousFramePosition{};

	// The signed target degrees of the current rotation action. Only 1 axis is active at once for any given strip.
	NiPoint3 m_InActionRotation{};

	// The signed degrees the current rotation action has progressed so far
	float m_RotationProgress{ 0.0f };

	// Whether this strip is waiting on a MoveBackToStart to arrive
	bool m_MovingToStart{ false };

	// The model's move interrupt count when this strip's current move started
	uint32_t m_MoveInterruptCount{};

	static constexpr float DEFAULT_SPEED = 3.0f;
	static constexpr float MIN_SPEED = 0.1f;

	// Speed applied to moves and rotations started by this strip
	float m_Speed{ DEFAULT_SPEED };
	
	LWOOBJID m_StripInitiatorID{ LWOOBJID_EMPTY };
};

#endif  //!__STRIP__H__
