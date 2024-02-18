#include "RearrangeStripMessage.h"

RearrangeStripMessage::RearrangeStripMessage(const AMFArrayValue* arguments)
	: BehaviorMessageBase{ arguments }
	, m_SrcActionIndex{ GetActionIndexFromArgument(arguments, "srcActionIndex") }
	, m_DstActionIndex{ GetActionIndexFromArgument(arguments, "dstActionIndex") }
	, m_ActionContext{ arguments } {

	LOG_DEBUG("srcactionIndex %i dstactionIndex %i stripId %i behaviorId %i stateId %i", m_SrcActionIndex, m_DstActionIndex, m_ActionContext.GetStripId(), m_BehaviorId, m_ActionContext.GetStateId());
}
