#include "AddStripMessage.h"

#include "Action.h"

AddStripMessage::AddStripMessage(const AMFArrayValue& arguments)
	: BehaviorMessageBase{ arguments }
	, m_Position{ arguments }
	, m_ActionContext{ arguments } {

	const auto* const strip = arguments.GetArray("strip");
	if (!strip) return;

	const auto* const actions = strip->GetArray("actions");
	if (!actions) return;

	for (size_t actionNumber = 0; actionNumber < actions->GetDense().size(); ++actionNumber) {
		const auto* const actionValue = actions->GetArray(actionNumber);
		if (!actionValue) continue;

		m_ActionsToAdd.emplace_back(*actionValue);

		Log::Debug("xPosition {:f} yPosition {:f} stripId {:d} stateId {:d} behaviorId {:d} t {:s} valueParameterName {:s} valueParameterString {:s} valueParameterDouble {:f}", m_Position.GetX(), m_Position.GetY(), m_ActionContext.GetStripId(), GeneralUtils::ToUnderlying(m_ActionContext.GetStateId()), m_BehaviorId, m_ActionsToAdd.back().GetType(), m_ActionsToAdd.back().GetValueParameterName(), m_ActionsToAdd.back().GetValueParameterString(), m_ActionsToAdd.back().GetValueParameterDouble());
	}
	Log::Debug("number of actions {:d}", m_ActionsToAdd.size());
}
