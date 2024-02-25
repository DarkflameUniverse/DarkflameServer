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
	SplitStripMessage(const AMFArrayValue* arguments);
	
	[[nodiscard]] int32_t GetSrcActionIndex() const noexcept { return m_SrcActionIndex; }

	[[nodiscard]] const ActionContext& GetSourceActionContext() const noexcept { return m_SourceActionContext; }

	[[nodiscard]] const ActionContext& GetDestinationActionContext() const noexcept { return m_DestinationActionContext; }

	[[nodiscard]] const StripUiPosition& GetPosition() const noexcept { return m_DestinationPosition; }
	
	[[nodiscard]] const std::vector<Action>& GetTransferredActions() const noexcept { return m_TransferredActions; }

	void SetTransferredActions(std::vector<Action>::const_iterator begin, std::vector<Action>::const_iterator end) { m_TransferredActions.assign(begin, end); };

private:
	int32_t m_SrcActionIndex;
	ActionContext m_SourceActionContext;
	ActionContext m_DestinationActionContext;
	StripUiPosition m_DestinationPosition;

	std::vector<Action> m_TransferredActions;
};

#endif  //!__SPLITSTRIPMESSAGE__H__
