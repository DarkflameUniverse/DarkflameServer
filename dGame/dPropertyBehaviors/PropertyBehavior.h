#ifndef __PROPERTYBEHAVIOR__H__
#define __PROPERTYBEHAVIOR__H__

#include "State.h"

namespace tinyxml2 {
	class XMLElement;
}

enum class BehaviorState : uint32_t;

class AMFArrayValue;

/**
 * Represents the Entity of a Property Behavior and holds data associated with the behavior
 */
class PropertyBehavior {
public:
	PropertyBehavior();

	template <typename Msg>
	void HandleMsg(Msg& msg);

	// If the last edited state has no strips, this method will set the last edited state to the first state that has strips.
	void VerifyLastEditedState();
	void SendBehaviorListToClient(AMFArrayValue& args) const;
	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;

	[[nodiscard]] int32_t GetBehaviorId() const noexcept { return m_BehaviorId; }
	void SetBehaviorId(int32_t id) noexcept { m_BehaviorId = id; }

	void Serialize(tinyxml2::XMLElement& behavior) const;
	void Deserialize(const tinyxml2::XMLElement& behavior);
private:

	// The states this behavior has.
	std::map<BehaviorState, State> m_States;

	// The name of this behavior.
	std::string m_Name = "New Behavior";

	// Whether this behavior is locked and cannot be edited.
	bool isLocked = false;

	// Whether this behavior is custom or pre-fab.
	bool isLoot = false;

	// The last state that was edited. This is used so when the client re-opens the behavior editor, it will open to the last edited state.
	// If the last edited state has no strips, it will open to the first state that has strips.
	BehaviorState m_LastEditedState;

	// The behavior id for this behavior. This is expected to be fully unique, however an id of -1 means this behavior was just created
	// and needs to be assigned an id.
	int32_t m_BehaviorId = -1;
};

#endif  //!__PROPERTYBEHAVIOR__H__
