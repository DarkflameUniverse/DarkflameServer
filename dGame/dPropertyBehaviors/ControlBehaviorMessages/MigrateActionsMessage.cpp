#include "MigrateActionsMessage.h"

MigrateActionsMessage::MigrateActionsMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_SrcActionIndex{ GetActionIndexFromArgument(arguments, "srcActionIndex") }
	, m_DstActionIndex{ GetActionIndexFromArgument(arguments, "dstActionIndex") }
	, m_SourceActionContext{ arguments, "srcStateID", "srcStripID" }
	, m_DestinationActionContext{ arguments, "dstStateID", "dstStripID" } {

	Log::Debug("srcactionIndex {:d} dstactionIndex {:d} srcstripId {:d} dststripId {:d} srcstateId {:d} dststateId {:d} behaviorId {:d}", m_SrcActionIndex, m_DstActionIndex, m_SourceActionContext.GetStripId(), m_DestinationActionContext.GetStripId(), GeneralUtils::ToUnderlying(m_SourceActionContext.GetStateId()), GeneralUtils::ToUnderlying(m_DestinationActionContext.GetStateId()), m_BehaviorId);
}
