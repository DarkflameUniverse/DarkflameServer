#ifndef __ADDMESSAGE__H__
#define __ADDMESSAGE__H__

#include "BehaviorMessageBase.h"

class AddMessage : public BehaviorMessageBase {
public:
	AddMessage(AMFArrayValue* arguments);
	uint32_t GetBehaviorIndex() { return behaviorIndex; };
	uint32_t GetBehaviorId() { return behaviorId; };
private:
	uint32_t behaviorId;
	uint32_t behaviorIndex;
};

#endif  //!__ADDMESSAGE__H__
