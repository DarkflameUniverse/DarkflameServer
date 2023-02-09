#ifndef __REMOVESTRIPMESSAGE__H__
#define __REMOVESTRIPMESSAGE__H__

#include "BehaviorMessageBase.h"

class RemoveStripMessage : public BehaviorMessageBase {
public:
	void Parse(AMFArrayValue* arguments);
	StripId stripId;
	BehaviorState behaviorState;
	uint32_t behaviorId;
};

#endif  //!__REMOVESTRIPMESSAGE__H__
