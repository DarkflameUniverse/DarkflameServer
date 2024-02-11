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
	RearrangeStripMessage(const AMFArrayValue& arguments);

	[[nodiscard]] int32_t GetSrcActionIndex() const noexcept { return m_SrcActionIndex; }
	[[nodiscard]] int32_t GetDstActionIndex() const noexcept { return m_DstActionIndex; }

	[[nodiscard]] const ActionContext& GetActionContext() const noexcept { return m_ActionContext; }
	[[nodiscard]] ActionContext& GetActionContext() noexcept { return m_ActionContext; }

private:
	int32_t m_SrcActionIndex;
	int32_t m_DstActionIndex;
	ActionContext m_ActionContext;
};

#endif  //!__REARRANGESTRIPMESSAGE__H__
