#ifndef __MERGESTRIPSMESSAGE__H__
#define __MERGESTRIPSMESSAGE__H__

#include "Action.h"
#include "ActionContext.h"
#include "BehaviorMessageBase.h"

class AMFArrayValue;

/**
 * @brief Sent when a player adds the first Action of Strip A to a Strip B
 * 
 */
class MergeStripsMessage : public BehaviorMessageBase {
public:
	MergeStripsMessage(const AMFArrayValue& arguments);

	[[nodiscard]] int32_t GetDstActionIndex() const noexcept { return m_DstActionIndex; }

	[[nodiscard]] const ActionContext& GetSourceActionContext() const noexcept { return m_SourceActionContext; }
	[[nodiscard]] ActionContext& GetSourceActionContext() noexcept { return m_SourceActionContext; }

	[[nodiscard]] const ActionContext& GetDestinationActionContext() const noexcept { return m_DestinationActionContext; }
	[[nodiscard]] ActionContext& GetDestinationActionContext() noexcept { return m_DestinationActionContext; }

	[[nodiscard]] const std::vector<Action>& GetMigratedActions() const noexcept { return m_MigratedActions; }
	[[nodiscard]] std::vector<Action>& GetMigratedActions() noexcept { return m_MigratedActions; }

	void SetMigratedActions(std::vector<Action>::const_iterator start, std::vector<Action>::const_iterator end) { m_MigratedActions.assign(start, end); };

private:
	int32_t m_DstActionIndex;
	std::vector<Action> m_MigratedActions;
	ActionContext m_SourceActionContext;
	ActionContext m_DestinationActionContext;
};

#endif  //!__MERGESTRIPSMESSAGE__H__
