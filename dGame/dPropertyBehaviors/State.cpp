#include "State.h"

#include "ControlBehaviorMsgs.h"

template<>
void State::HandleMsg(AddStripMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		m_Strips.resize(msg.GetActionContext().GetStripId() + 1);
	}
	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(AddActionMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(UpdateStripUiMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(RemoveActionsMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(RearrangeStripMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(UpdateActionMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(RemoveStripMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(SplitStripMessage& msg) {
	if (msg.GetTransferredActions().empty()) {
		if (m_Strips.size() <= msg.GetSourceActionContext().GetStripId()) {
			return;
		}

		m_Strips.at(msg.GetSourceActionContext().GetStripId()).HandleMsg(msg);
	} else {
		if (m_Strips.size() <= msg.GetDestinationActionContext().GetStripId()) {
			m_Strips.resize(msg.GetDestinationActionContext().GetStripId() + 1);
		}

		m_Strips.at(msg.GetDestinationActionContext().GetStripId()).HandleMsg(msg);
	}
};

template<>
void State::HandleMsg(MergeStripsMessage& msg) {
	if (msg.GetMigratedActions().empty()) {
		if (m_Strips.size() <= msg.GetSourceActionContext().GetStripId()) {
			return;
		}

		m_Strips.at(msg.GetSourceActionContext().GetStripId()).HandleMsg(msg);
	} else {
		if (m_Strips.size() <= msg.GetDestinationActionContext().GetStripId()) {
			m_Strips.resize(msg.GetDestinationActionContext().GetStripId() + 1);
		}

		m_Strips.at(msg.GetDestinationActionContext().GetStripId()).HandleMsg(msg);
	}
};

template<>
void State::HandleMsg(MigrateActionsMessage& msg) {
	if (msg.GetMigratedActions().empty()) {
		if (m_Strips.size() <= msg.GetSourceActionContext().GetStripId()) {
			return;
		}

		m_Strips.at(msg.GetSourceActionContext().GetStripId()).HandleMsg(msg);
	} else {
		if (m_Strips.size() <= msg.GetDestinationActionContext().GetStripId()) {
			m_Strips.resize(msg.GetDestinationActionContext().GetStripId() + 1);
		}

		m_Strips.at(msg.GetDestinationActionContext().GetStripId()).HandleMsg(msg);
	}
};

bool State::IsEmpty() const {
	for (auto& strip : m_Strips) {
		if (!strip.IsEmpty()) return false;
	}
	return true;
}

void State::SendBehaviorBlocksToClient(AMFArrayValue& args) const {
	auto* strips = args.InsertArray("strips");
	for (int32_t stripId = 0; stripId < m_Strips.size(); stripId++) {
		auto& strip = m_Strips.at(stripId);
		if (strip.IsEmpty()) continue;

		auto* stripArgs = strips->PushArray();
		stripArgs->Insert("id", static_cast<double>(stripId));

		strip.SendBehaviorBlocksToClient(*stripArgs);
	}
};
