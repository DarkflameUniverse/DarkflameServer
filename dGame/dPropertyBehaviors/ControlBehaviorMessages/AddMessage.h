#ifndef __ADDMESSAGE__H__
#define __ADDMESSAGE__H__

#include "BehaviorMessageBase.h"

class AddMessage : public BehaviorMessageBase {
public:
	AddMessage(AMFArrayValue* arguments);
	const uint32_t GetBehaviorIndex() { return behaviorIndex; };
private:
	uint32_t behaviorIndex;
};

#endif  //!__ADDMESSAGE__H__
