#ifndef __ADDMESSAGE__H__
#define __ADDMESSAGE__H__

#include "BehaviorMessageBase.h"

class AddMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
	uint32_t behaviorId;
	uint32_t behaviorIndex;
};

#endif  //!__ADDMESSAGE__H__
