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
	UpdateStripUiMessage(const AMFArrayValue* arguments);

	[[nodiscard]] const StripUiPosition& GetPosition() const noexcept { return m_Position; };

	[[nodiscard]] const ActionContext& GetActionContext() const noexcept { return m_ActionContext; };

private:
	StripUiPosition m_Position;
	ActionContext m_ActionContext;
};

#endif  //!__UPDATESTRIPUIMESSAGE__H__
