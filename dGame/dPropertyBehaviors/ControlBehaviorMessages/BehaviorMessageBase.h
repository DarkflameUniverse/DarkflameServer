#ifndef __BEHAVIORMESSAGEBASE__H__
#define __BEHAVIORMESSAGEBASE__H__

#include <stdexcept>
#include <string>

#include "AMFFormat.h"
#include "dCommonVars.h"

#include "Game.h"
#include "dLogger.h"

enum class BehaviorState : uint32_t;

/**
 * @brief The behaviorID target of this ControlBehaviors message
 * 
 */
class BehaviorMessageBase {
public:
	const uint32_t GetBehaviorId() { return behaviorId; };
protected:
	BehaviorMessageBase(AMFArrayValue* arguments);
	int32_t GetBehaviorIdFromArgument(AMFArrayValue* arguments);
	uint32_t GetActionIndexFromArgument(AMFArrayValue* arguments, const std::string& keyName = "actionIndex");
	int32_t behaviorId = -1; 
};

#endif  //!__BEHAVIORMESSAGEBASE__H__
