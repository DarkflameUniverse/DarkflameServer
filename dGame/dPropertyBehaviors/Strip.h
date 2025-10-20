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
	void Update(float deltaTime, ModelComponent& modelComponent, UpdateResult& updateResult);
	void SpawnDrop(LOT dropLOT, Entity& entity);
	void ProcNormalAction(float deltaTime, ModelComponent& modelComponent, UpdateResult& updateResult);
	void RemoveStates(ModelComponent& modelComponent) const;

	// 2 actions are required for strips to work
	bool HasMinimumActions() const { return m_Actions.size() >= 2; }
	
	void OnChatMessageReceived(const std::string& sMessage);
	void OnHit();
private:
	// Indicates this Strip is waiting for an action to be taken upon it to progress to its actions
	bool m_WaitingForAction{ false };

	// The amount of time this strip is paused for. Any interactions with this strip should be bounced if this is greater than 0.
	// Actions that do not use time do not use this (ex. positions).
	float m_PausedTime{ 0.0f };

	// The index of the next action to be played. This should always be within range of [0, m_Actions.size()).
	size_t m_NextActionIndex{ 0 };

	// The list of actions to be executed on this behavior.
	std::vector<Action> m_Actions;

	// The location of this strip on the UGBehaviorEditor UI
	StripUiPosition m_Position;

	// The current actions remaining distance to the target
	// Only 1 of these vertexs' will be active at once for any given strip.
	NiPoint3 m_InActionMove{};

	// The position of the parent model on the previous frame
	NiPoint3 m_PreviousFramePosition{};

	NiPoint3 m_SavedVelocity{};
};

#endif  //!__STRIP__H__
