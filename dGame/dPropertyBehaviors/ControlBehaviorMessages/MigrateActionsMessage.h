#ifndef __MIGRATEACTIONSMESSAGE__H__
#define __MIGRATEACTIONSMESSAGE__H__

#include "Action.h"
#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player moves an Action after the first Action to a different Strip
 * 
 */
class MigrateActionsMessage : public BehaviorMessageBase {
public:
	MigrateActionsMessage(const AMFArrayValue& arguments);

	[[nodiscard]] int32_t GetSrcActionIndex() const noexcept { return m_SrcActionIndex; }

	[[nodiscard]] int32_t GetDstActionIndex() const noexcept { return m_DstActionIndex; }

	[[nodiscard]] const ActionContext& GetSourceActionContext() const noexcept { return m_SourceActionContext; }

	[[nodiscard]] const ActionContext& GetDestinationActionContext() const noexcept { return m_DestinationActionContext; }

	[[nodiscard]] const std::vector<Action>& GetMigratedActions() const noexcept { return m_MigratedActions; }

	void SetMigratedActions(std::vector<Action>::const_iterator start, std::vector<Action>::const_iterator end) { m_MigratedActions.assign(start, end); }

private:
	int32_t m_SrcActionIndex;
	int32_t m_DstActionIndex;
	std::vector<Action> m_MigratedActions;
	ActionContext m_SourceActionContext;
	ActionContext m_DestinationActionContext;
};

#endif  //!__MIGRATEACTIONSMESSAGE__H__
