#ifndef __REARRANGESTRIPMESSAGE__H__
#define __REARRANGESTRIPMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class RearrangeStripMessage : public BehaviorMessageBase {
public:
	RearrangeStripMessage(AMFArrayValue* arguments);
	const uint32_t GetSrcActionIndex() { return srcActionIndex; };
	const uint32_t GetDstActionIndex() { return dstActionIndex; };
	ActionContext GetActionContext() { return actionContext; };
private:
	ActionContext actionContext;
	uint32_t srcActionIndex;
	uint32_t dstActionIndex;
};

#endif  //!__REARRANGESTRIPMESSAGE__H__
