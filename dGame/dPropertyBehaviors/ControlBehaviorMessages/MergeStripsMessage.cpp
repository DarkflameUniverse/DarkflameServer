#include "MergeStripsMessage.h"

MergeStripsMessage::MergeStripsMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_DstActionIndex{ GetActionIndexFromArgument(arguments, "dstActionIndex") }
	, m_SourceActionContext{ arguments, "srcStateID", "srcStripID" }
	, m_DestinationActionContext{ arguments, "dstStateID", "dstStripID" } {

	LOG_DEBUG("srcstripId %i dststripId %i srcstateId %i dststateId %i dstactionIndex %i behaviorId %i", m_SourceActionContext.GetStripId(), m_DestinationActionContext.GetStripId(), m_SourceActionContext.GetStateId(), m_DestinationActionContext.GetStateId(), m_DstActionIndex, m_BehaviorId);
}

