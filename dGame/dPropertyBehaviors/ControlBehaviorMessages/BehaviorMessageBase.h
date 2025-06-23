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
	static constexpr LWOOBJID DefaultBehaviorId{ -1 };
	BehaviorMessageBase(const AMFArrayValue& arguments) : m_BehaviorId{ GetBehaviorIdFromArgument(arguments) } {}
	[[nodiscard]] LWOOBJID GetBehaviorId() const noexcept { return m_BehaviorId; }
	[[nodiscard]] bool IsDefaultBehaviorId() const noexcept { return m_BehaviorId == DefaultBehaviorId; }
	[[nodiscard]] bool GetNeedsNewBehaviorID() const noexcept { return m_NeedsNewBehaviorID; }
	void SetNeedsNewBehaviorID(const bool val) noexcept { m_NeedsNewBehaviorID = val; }

protected:
	[[nodiscard]] LWOOBJID GetBehaviorIdFromArgument(const AMFArrayValue& arguments);
	[[nodiscard]] int32_t GetActionIndexFromArgument(const AMFArrayValue& arguments, const std::string_view keyName = "actionIndex") const;
	LWOOBJID m_BehaviorId{ DefaultBehaviorId };
	bool m_NeedsNewBehaviorID{ false };
};

#endif  //!__BEHAVIORMESSAGEBASE__H__
