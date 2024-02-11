#ifndef __BEHAVIORMESSAGEBASE__H__
#define __BEHAVIORMESSAGEBASE__H__

#include <stdexcept>
#include <string>

#include "Amf3.h"
#include "dCommonVars.h"

enum class BehaviorState : uint32_t;

/**
 * @brief The behaviorID target of this ControlBehaviors message
 * 
 */
class BehaviorMessageBase {
public:
	static constexpr int32_t DefaultBehaviorId = -1;
	[[nodiscard]] int32_t GetBehaviorId() const { return behaviorId; };
	[[nodiscard]] bool IsDefaultBehaviorId() { return behaviorId == DefaultBehaviorId; };
	BehaviorMessageBase(AMFArrayValue* const arguments);
protected:
	[[nodiscard]] int32_t GetBehaviorIdFromArgument(AMFArrayValue* const arguments);
	[[nodiscard]] int32_t GetActionIndexFromArgument(AMFArrayValue* const arguments, const std::string& keyName = "actionIndex") const;
	int32_t behaviorId = DefaultBehaviorId;
};

#endif  //!__BEHAVIORMESSAGEBASE__H__
