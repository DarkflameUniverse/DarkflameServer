#include "RearrangeStripMessage.h"

RearrangeStripMessage::RearrangeStripMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_SrcActionIndex{ GetActionIndexFromArgument(arguments, "srcActionIndex") }
	, m_DstActionIndex{ GetActionIndexFromArgument(arguments, "dstActionIndex") }
	, m_ActionContext{ arguments } {

	Log::Debug("srcactionIndex {:d} dstactionIndex {:d} stripId {:d} behaviorId {:d} stateId {:d}", m_SrcActionIndex, m_DstActionIndex, m_ActionContext.GetStripId(), m_BehaviorId, GeneralUtils::ToUnderlying(m_ActionContext.GetStateId()));
}
