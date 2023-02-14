#ifndef __UPDATESTRIPUIMESSAGE__H__
#define __UPDATESTRIPUIMESSAGE__H__

#include "ActionContext.h"
#include "BehaviorMessageBase.h"
#include "StripUiPosition.h"

class AMFArrayValue;

class UpdateStripUiMessage : public BehaviorMessageBase {
public:
	UpdateStripUiMessage(AMFArrayValue* arguments);
	StripUiPosition GetPosition() { return position; };
	ActionContext GetActionContext() { return actionContext; };
private:
	StripUiPosition position;
	ActionContext actionContext;
};

#endif  //!__UPDATESTRIPUIMESSAGE__H__
