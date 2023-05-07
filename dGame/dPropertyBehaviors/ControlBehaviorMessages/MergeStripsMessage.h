#ifndef __MERGESTRIPSMESSAGE__H__
#define __MERGESTRIPSMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player adds the first Action of Strip A to a Strip B
 * 
 */
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
