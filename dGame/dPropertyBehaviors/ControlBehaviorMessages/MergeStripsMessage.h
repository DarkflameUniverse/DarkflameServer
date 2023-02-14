#ifndef __MERGESTRIPSMESSAGE__H__
#define __MERGESTRIPSMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

class MergeStripsMessage : public BehaviorMessageBase {
public:
	MergeStripsMessage(AMFArrayValue* arguments);
	const uint32_t GetDstActionIndex() { return dstActionIndex; };
	ActionContext GetSourceActionContext() { return sourceActionContext; };
	ActionContext GetDestinationActionContext() { return destinationActionContext; };
private:
	ActionContext sourceActionContext;
	ActionContext destinationActionContext;
	uint32_t dstActionIndex;
};

#endif  //!__MERGESTRIPSMESSAGE__H__
