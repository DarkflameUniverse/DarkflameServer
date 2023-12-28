#ifndef __REARRANGESTRIPMESSAGE__H__
#define __REARRANGESTRIPMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"

/**
 * @brief Sent when a player moves an Action around in the same strip
 * 
 */
class RearrangeStripMessage : public BehaviorMessageBase {
public:
	RearrangeStripMessage(AMFArrayValue* arguments);
	int32_t GetSrcActionIndex() { return srcActionIndex; };
	int32_t GetDstActionIndex() { return dstActionIndex; };
	ActionContext GetActionContext() { return actionContext; };
private:
	ActionContext actionContext;
	int32_t srcActionIndex;
	int32_t dstActionIndex;
};

#endif  //!__REARRANGESTRIPMESSAGE__H__
