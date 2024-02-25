#include "UpdateStripUiMessage.h"

UpdateStripUiMessage::UpdateStripUiMessage(const AMFArrayValue* arguments)
	: BehaviorMessageBase{ arguments } 
	, m_Position{ arguments }
	, m_ActionContext{ arguments } {

	LOG_DEBUG("xPosition %f yPosition %f stripId %i stateId %i behaviorId %i", m_Position.GetX(), m_Position.GetY(), m_ActionContext.GetStripId(), m_ActionContext.GetStateId(), m_BehaviorId);
}
