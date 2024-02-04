#ifndef __ADDMESSAGE__H__
#define __ADDMESSAGE__H__

#include "BehaviorMessageBase.h"

/**
 * @brief Sent when a player adds a Behavior A from their inventory to a model.
 * 
 */
class AddMessage : public BehaviorMessageBase {
public:
	AddMessage(AMFArrayValue* arguments);
	const uint32_t GetBehaviorIndex() const { return m_BehaviorIndex; };
private:
	uint32_t m_BehaviorIndex;
};

#endif  //!__ADDMESSAGE__H__
