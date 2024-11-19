#ifndef __ACTIONCONTEXT__H__
#define __ACTIONCONTEXT__H__

#include "BehaviorStates.h"
#include "dCommonVars.h"

#include <string_view>

class AMFArrayValue;

/**
 * @brief Sent if contextual State and Strip informationis needed for a ControlBehaviors message
 * 
 */
class ActionContext {
public:
	ActionContext() noexcept = default;
	ActionContext(const AMFArrayValue& arguments, const std::string_view customStateKey = "stateID", const std::string_view customStripKey = "stripID");
	[[nodiscard]] StripId GetStripId() const noexcept { return m_StripId; };
	[[nodiscard]] BehaviorState GetStateId() const noexcept { return m_StateId; };

private:
	[[nodiscard]] BehaviorState GetBehaviorStateFromArgument(const AMFArrayValue& arguments, const std::string_view key) const;
	[[nodiscard]] StripId GetStripIdFromArgument(const AMFArrayValue& arguments, const std::string_view key) const;
	StripId m_StripId{ 0 };
	BehaviorState m_StateId{ BehaviorState::HOME_STATE };
};

#endif  //!__ACTIONCONTEXT__H__
