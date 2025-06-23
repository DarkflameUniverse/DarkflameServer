#ifndef __MOVETOINVENTORYMESSAGE__H__
#define __MOVETOINVENTORYMESSAGE__H__

#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player moves a Behavior A at position B to their inventory. 
 */
class MoveToInventoryMessage : public BehaviorMessageBase {
public:
	MoveToInventoryMessage(const AMFArrayValue& arguments, const LWOOBJID owningPlayerID);
	[[nodiscard]] uint32_t GetBehaviorIndex() const noexcept { return m_BehaviorIndex; };
	[[nodiscard]] LWOOBJID GetOwningPlayerID() const noexcept { return m_OwningPlayerID; };

private:
	uint32_t m_BehaviorIndex;
	LWOOBJID m_OwningPlayerID{};
};

#endif  //!__MOVETOINVENTORYMESSAGE__H__
