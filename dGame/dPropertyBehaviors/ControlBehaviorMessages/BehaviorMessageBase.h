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
	static inline int32_t DefaultBehaviorId = -1;
	const int32_t GetBehaviorId() const { return m_BehaviorId; };
	bool IsDefaultBehaviorId() { return m_BehaviorId == DefaultBehaviorId; };
	BehaviorMessageBase(AMFArrayValue* arguments);
protected:
	int32_t GetBehaviorIdFromArgument(AMFArrayValue* arguments);
	int32_t GetActionIndexFromArgument(AMFArrayValue* arguments, const std::string& keyName = "actionIndex");
	int32_t m_BehaviorId = DefaultBehaviorId;
};

#endif  //!__BEHAVIORMESSAGEBASE__H__
