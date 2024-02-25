#ifndef __ADDMESSAGE__H__
#define __ADDMESSAGE__H__

#include "BehaviorMessageBase.h"

/**
 * @brief Sent when a player adds a Behavior A from their inventory to a model.
 * 
 */
class AddMessage : public BehaviorMessageBase {
public:
	AddMessage(const AMFArrayValue* arguments);
	[[nodiscard]] uint32_t GetBehaviorIndex() const noexcept { return m_BehaviorIndex; };

private:
	uint32_t m_BehaviorIndex{ 0 };
};

#endif  //!__ADDMESSAGE__H__
