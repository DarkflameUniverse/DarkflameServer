#ifndef BEHAVIORMESSAGEBASE_H
#define BEHAVIORMESSAGEBASE_H

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
	static constexpr int32_t DefaultBehaviorId{ -1 };
	BehaviorMessageBase(const AMFArrayValue& arguments) : m_BehaviorId{ GetBehaviorIdFromArgument(arguments) } {}
	[[nodiscard]] int32_t GetBehaviorId() const noexcept { return m_BehaviorId; }
	[[nodiscard]] bool IsDefaultBehaviorId() const noexcept { return m_BehaviorId == DefaultBehaviorId; }

protected:
	[[nodiscard]] int32_t GetBehaviorIdFromArgument(const AMFArrayValue& arguments);
	[[nodiscard]] int32_t GetActionIndexFromArgument(const AMFArrayValue& arguments, const std::string& keyName = "actionIndex") const;
	int32_t m_BehaviorId{ DefaultBehaviorId };
};

#endif  //!BEHAVIORMESSAGEBASE_H
