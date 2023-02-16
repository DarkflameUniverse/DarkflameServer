#ifndef __SPLITSTRIPMESSAGE__H__
#define __SPLITSTRIPMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"
#include "StripUiPosition.h"

class AMFArrayValue;

/**
 * @brief Sent when a player takes an Action from Strip A and does not add it to an existing strip
 * 
 */
class SplitStripMessage : public BehaviorMessageBase {
public:
	SplitStripMessage(AMFArrayValue* arguments);
	ActionContext GetSourceActionContext() { return sourceActionContext; };
	ActionContext GetDestinationActionContext() { return destinationActionContext; };
	const uint32_t GetSrcActionIndex() { return srcActionIndex; };
	StripUiPosition GetPosition() { return destinationPosition; };
private:
	ActionContext sourceActionContext;
	ActionContext destinationActionContext;
	uint32_t srcActionIndex;
	StripUiPosition destinationPosition;
};

#endif  //!__SPLITSTRIPMESSAGE__H__
