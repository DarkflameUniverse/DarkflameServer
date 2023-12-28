#ifndef __SPLITSTRIPMESSAGE__H__
#define __SPLITSTRIPMESSAGE__H__

#include "Action.h"
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
	int32_t GetSrcActionIndex() { return srcActionIndex; };
	StripUiPosition GetPosition() { return destinationPosition; };
	const std::vector<Action>& GetTransferredActions() { return transferredActions; };
	void SetTransferredActions(std::vector<Action>::const_iterator begin, std::vector<Action>::const_iterator end) { transferredActions.assign(begin, end); };
private:
	ActionContext sourceActionContext;
	ActionContext destinationActionContext;
	int32_t srcActionIndex;
	StripUiPosition destinationPosition;

	std::vector<Action> transferredActions;
};

#endif  //!__SPLITSTRIPMESSAGE__H__
