#include "Strip.h"

#include "Amf3.h"
#include "ControlBehaviorMsgs.h"
#include "tinyxml2.h"

template <>
void Strip::HandleMsg(AddStripMessage& msg) {
	m_Actions = msg.GetActionsToAdd();
	m_Position = msg.GetPosition();
};

template <>
void Strip::HandleMsg(AddActionMessage& msg) {
	if (msg.GetActionIndex() == -1) return;
	m_Actions.insert(m_Actions.begin() + msg.GetActionIndex(), msg.GetAction());
};

template <>
void Strip::HandleMsg(UpdateStripUiMessage& msg) {
	m_Position = msg.GetPosition();
};

template <>
void Strip::HandleMsg(RemoveStripMessage& msg) {
	m_Actions.clear();
};

template <>
void Strip::HandleMsg(RemoveActionsMessage& msg) {
	if (msg.GetActionIndex() >= m_Actions.size()) return;
	m_Actions.erase(m_Actions.begin() + msg.GetActionIndex(), m_Actions.end());
};

template <>
void Strip::HandleMsg(UpdateActionMessage& msg) {
	if (msg.GetActionIndex() >= m_Actions.size()) return;
	m_Actions.at(msg.GetActionIndex()) = msg.GetAction();
};

template <>
void Strip::HandleMsg(RearrangeStripMessage& msg) {
	if (msg.GetDstActionIndex() >= m_Actions.size() || msg.GetSrcActionIndex() >= m_Actions.size() || msg.GetSrcActionIndex() <= msg.GetDstActionIndex()) return;
	std::rotate(m_Actions.begin() + msg.GetDstActionIndex(), m_Actions.begin() + msg.GetSrcActionIndex(), m_Actions.end());
};

template <>
void Strip::HandleMsg(SplitStripMessage& msg) {
	if (msg.GetTransferredActions().empty() && !m_Actions.empty()) {
		auto startToMove = m_Actions.begin() + msg.GetSrcActionIndex();
		msg.SetTransferredActions(startToMove, m_Actions.end());
		m_Actions.erase(startToMove, m_Actions.end());
	} else {
		m_Actions = msg.GetTransferredActions();
		m_Position = msg.GetPosition();
	}
};

template <>
void Strip::HandleMsg(MergeStripsMessage& msg) {
	if (msg.GetMigratedActions().empty() && !m_Actions.empty()) {
		msg.SetMigratedActions(m_Actions.begin(), m_Actions.end());
		m_Actions.erase(m_Actions.begin(), m_Actions.end());
	} else {
		m_Actions.insert(m_Actions.begin() + msg.GetDstActionIndex(), msg.GetMigratedActions().begin(), msg.GetMigratedActions().end());
	}
};

template <>
void Strip::HandleMsg(MigrateActionsMessage& msg) {
	if (msg.GetMigratedActions().empty() && !m_Actions.empty()) {
		auto startToMove = m_Actions.begin() + msg.GetSrcActionIndex();
		msg.SetMigratedActions(startToMove, m_Actions.end());
		m_Actions.erase(startToMove, m_Actions.end());
	} else {
		m_Actions.insert(m_Actions.begin() + msg.GetDstActionIndex(), msg.GetMigratedActions().begin(), msg.GetMigratedActions().end());
	}
};

void Strip::SendBehaviorBlocksToClient(AMFArrayValue& args) const {
	m_Position.SendBehaviorBlocksToClient(args);

	auto* const actions = args.InsertArray("actions");
	for (const auto& action : m_Actions) {
		action.SendBehaviorBlocksToClient(*actions);
	}
}

void Strip::Serialize(tinyxml2::XMLElement& strip) const {
	auto* const positionElement = strip.InsertNewChildElement("Position");
	m_Position.Serialize(*positionElement);
	for (const auto& action : m_Actions) {
		auto* const actionElement = strip.InsertNewChildElement("Action");
		action.Serialize(*actionElement);
	}
}

void Strip::Deserialize(const tinyxml2::XMLElement& strip) {
	const auto* positionElement = strip.FirstChildElement("Position");
	if (positionElement) {
		m_Position.Deserialize(*positionElement);
	}

	for (const auto* actionElement = strip.FirstChildElement("Action"); actionElement; actionElement = actionElement->NextSiblingElement("Action")) {
		auto& action = m_Actions.emplace_back();
		action.Deserialize(*actionElement);
	}
}
