#include "MergeStripsMessage.h"

MergeStripsMessage::MergeStripsMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_DstActionIndex{ GetActionIndexFromArgument(arguments, "dstActionIndex") }
	, m_SourceActionContext{ arguments, "srcStateID", "srcStripID" }
	, m_DestinationActionContext{ arguments, "dstStateID", "dstStripID" } {

	Log::Debug("srcstripId {:d} dststripId {:d} srcstateId {:d} dststateId {:d} dstactionIndex {:d} behaviorId {:d}", m_SourceActionContext.GetStripId(), m_DestinationActionContext.GetStripId(), GeneralUtils::ToUnderlying(m_SourceActionContext.GetStateId()), GeneralUtils::ToUnderlying(m_DestinationActionContext.GetStateId()), m_DstActionIndex, m_BehaviorId);
}

