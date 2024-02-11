#include "SplitStripMessage.h"

SplitStripMessage::SplitStripMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_SrcActionIndex{ GetActionIndexFromArgument(arguments, "srcActionIndex") }
	, m_SourceActionContext{ arguments, "srcStateID", "srcStripID" }
	, m_DestinationActionContext{ arguments, "dstStateID", "dstStripID" }
	, m_DestinationPosition{ arguments, "dstStripUI" } {

	LOG_DEBUG("behaviorId %i xPosition %f yPosition %f sourceStrip %i destinationStrip %i sourceState %i destinationState %i srcActindex %i", m_BehaviorId, m_DestinationPosition.GetX(), m_DestinationPosition.GetY(), m_SourceActionContext.GetStripId(), m_DestinationActionContext.GetStripId(), m_SourceActionContext.GetStateId(), m_DestinationActionContext.GetStateId(), m_SrcActionIndex);
}
