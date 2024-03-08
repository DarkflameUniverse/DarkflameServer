#include "UpdateStripUiMessage.h"

UpdateStripUiMessage::UpdateStripUiMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments } 
	, m_Position{ arguments }
	, m_ActionContext{ arguments } {

	Log::Debug("xPosition {:f} yPosition {:f} stripId {:d} stateId {:d} behaviorId {:d}", m_Position.GetX(), m_Position.GetY(), m_ActionContext.GetStripId(), GeneralUtils::ToUnderlying(m_ActionContext.GetStateId()), m_BehaviorId);
}
