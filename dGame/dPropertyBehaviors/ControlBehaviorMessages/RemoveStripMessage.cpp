#include "RemoveStripMessage.h"

RemoveStripMessage::RemoveStripMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_ActionContext{ arguments } {

	LOG_DEBUG("stripId %i stateId %i behaviorId %i", m_ActionContext.GetStripId(), m_ActionContext.GetStateId(), m_BehaviorId);
}
