#include "RemoveStripMessage.h"

RemoveStripMessage::RemoveStripMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_ActionContext{ arguments } {

	Log::Debug("stripId {:d} stateId {:d} behaviorId {:d}", m_ActionContext.GetStripId(), GeneralUtils::ToUnderlying(m_ActionContext.GetStateId()), m_BehaviorId);
}
