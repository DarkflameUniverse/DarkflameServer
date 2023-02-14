#ifndef __ACTIONCONTEXT__H__
#define __ACTIONCONTEXT__H__

#include "BehaviorStates.h"
#include "dCommonVars.h"

class AMFArrayValue;

/**
 * @brief Sent if contextual State and Strip informationis needed for a ControlBehaviors message
 * 
 */
class ActionContext {
public:
	ActionContext();
	ActionContext(AMFArrayValue* arguments, std::string customStateKey = "stateID", std::string customStripKey = "stripID");
	const StripId GetStripId() { return stripId; };
	const BehaviorState GetStateId() { return stateId; };
private:
	BehaviorState GetBehaviorStateFromArgument(AMFArrayValue* arguments, const std::string& key);
	StripId GetStripIdFromArgument(AMFArrayValue* arguments, const std::string& key);
	StripId stripId;
	BehaviorState stateId;
};

#endif  //!__ACTIONCONTEXT__H__
