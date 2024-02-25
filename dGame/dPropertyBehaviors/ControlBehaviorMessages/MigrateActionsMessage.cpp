#include "MigrateActionsMessage.h"

MigrateActionsMessage::MigrateActionsMessage(const AMFArrayValue* arguments)
	: BehaviorMessageBase{ arguments }
	, m_SrcActionIndex{ GetActionIndexFromArgument(arguments, "srcActionIndex") }
	, m_DstActionIndex{ GetActionIndexFromArgument(arguments, "dstActionIndex") }
	, m_SourceActionContext{ arguments, "srcStateID", "srcStripID" }
	, m_DestinationActionContext{ arguments, "dstStateID", "dstStripID" } {

	LOG_DEBUG("srcactionIndex %i dstactionIndex %i srcstripId %i dststripId %i srcstateId %i dststateId %i behaviorId %i", m_SrcActionIndex, m_DstActionIndex, m_SourceActionContext.GetStripId(), m_DestinationActionContext.GetStripId(), m_SourceActionContext.GetStateId(), m_DestinationActionContext.GetStateId(), m_BehaviorId);
}
