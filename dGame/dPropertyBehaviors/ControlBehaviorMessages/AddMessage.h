#ifndef __ADDMESSAGE__H__
#define __ADDMESSAGE__H__

#include "BehaviorMessageBase.h"

/**
 * @brief Sent when a player adds a Behavior A from their inventory to a model.
 * 
 */
class AddMessage : public BehaviorMessageBase {
public:
	AddMessage(const AMFArrayValue& arguments, const LWOOBJID _owningPlayerID);
	[[nodiscard]] uint32_t GetBehaviorIndex() const noexcept { return m_BehaviorIndex; };
	[[nodiscard]] LWOOBJID GetOwningPlayerID() const noexcept { return m_OwningPlayerID; };

private:
	uint32_t m_BehaviorIndex{ 0 };
	LWOOBJID m_OwningPlayerID{};
};

#endif  //!__ADDMESSAGE__H__
