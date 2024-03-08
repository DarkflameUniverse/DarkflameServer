#include "SplitStripMessage.h"

SplitStripMessage::SplitStripMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_SrcActionIndex{ GetActionIndexFromArgument(arguments, "srcActionIndex") }
	, m_SourceActionContext{ arguments, "srcStateID", "srcStripID" }
	, m_DestinationActionContext{ arguments, "dstStateID", "dstStripID" }
	, m_DestinationPosition{ arguments, "dstStripUI" } {

	Log::Debug("behaviorId {:d} xPosition {:f} yPosition {:f} sourceStrip {:d} destinationStrip {:d} sourceState {:d} destinationState {:d} srcActindex {:d}", m_BehaviorId, m_DestinationPosition.GetX(), m_DestinationPosition.GetY(), m_SourceActionContext.GetStripId(), m_DestinationActionContext.GetStripId(), GeneralUtils::ToUnderlying(m_SourceActionContext.GetStateId()), GeneralUtils::ToUnderlying(m_DestinationActionContext.GetStateId()), m_SrcActionIndex);
}
