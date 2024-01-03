#ifndef __UPDATESTRIPUIMESSAGE__H__
#define __UPDATESTRIPUIMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"
#include "StripUiPosition.h"

class AMFArrayValue;

/**
 * @brief Sent when a player moves the first Action in a Strip
 * 
 */
class UpdateStripUiMessage : public BehaviorMessageBase {
public:
	UpdateStripUiMessage(AMFArrayValue* arguments);
	StripUiPosition GetPosition() const { return position; };
	ActionContext GetActionContext() const { return actionContext; };
private:
	StripUiPosition position;
	ActionContext actionContext;
};

#endif  //!__UPDATESTRIPUIMESSAGE__H__
