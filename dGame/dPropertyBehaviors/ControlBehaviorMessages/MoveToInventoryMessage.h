#ifndef __MOVETOINVENTORYMESSAGE__H__
#define __MOVETOINVENTORYMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player moves a Behavior A at position B to their inventory. 
 */
#pragma warning("This Control Behavior Message does not have a test yet. Non-developers can ignore this warning.")
class MoveToInventoryMessage : public BehaviorMessageBase {
public:
	MoveToInventoryMessage(const AMFArrayValue* arguments);
	[[nodiscard]] uint32_t GetBehaviorIndex() const noexcept { return m_BehaviorIndex; };

private:
	uint32_t m_BehaviorIndex;
};

#endif  //!__MOVETOINVENTORYMESSAGE__H__
