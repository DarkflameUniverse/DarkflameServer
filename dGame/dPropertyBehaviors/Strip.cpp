#include "Strip.h"

#include "Amf3.h"
#include "ControlBehaviorMsgs.h"
#include "tinyxml2.h"
#include "dEntity/EntityInfo.h"
#include "ModelComponent.h"
#include "ChatPackets.h"
#include "PropertyManagementComponent.h"
#include "PlayerManager.h"

#include "dChatFilter.h"

#include "DluAssert.h"

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
}

template<>
void Strip::HandleMsg(GameMessages::RequestUse& msg) {
	if (m_PausedTime > 0.0f || !HasMinimumActions()) return;

	auto& nextAction = GetNextAction();

	if (nextAction.GetType() == "OnInteract") {
		IncrementAction();
		m_WaitingForAction = false;
	}
}

template<>
void Strip::HandleMsg(GameMessages::ResetModelToDefaults& msg) {
	m_WaitingForAction = false;
	m_PausedTime = 0.0f;
	m_NextActionIndex = 0;
	m_InActionMove = NiPoint3Constant::ZERO;
	m_PreviousFramePosition = NiPoint3Constant::ZERO;
}

void Strip::OnChatMessageReceived(const std::string& sMessage) {
	if (m_PausedTime > 0.0f || !HasMinimumActions()) return;

	const auto& nextAction = GetNextAction();
	if (nextAction.GetType() == "OnChat" && nextAction.GetValueParameterString() == sMessage) {
		IncrementAction();
		m_WaitingForAction = false;
	}
}

void Strip::IncrementAction() {
	if (m_Actions.empty()) return;
	m_NextActionIndex++;
	m_NextActionIndex %= m_Actions.size();
}

void Strip::Spawn(LOT lot, Entity& entity) {
	EntityInfo info{};
	info.lot = lot;
	info.pos = entity.GetPosition();
	info.rot = NiQuaternionConstant::IDENTITY;
	info.spawnerID = entity.GetObjectID();
	auto* const spawnedEntity = Game::entityManager->CreateEntity(info, nullptr, &entity);
	spawnedEntity->AddToGroup("SpawnedPropertyEnemies");
	Game::entityManager->ConstructEntity(spawnedEntity);
}

// Spawns a specific drop for all
void Strip::SpawnDrop(LOT dropLOT, Entity& entity) {
	for (auto* const player : PlayerManager::GetAllPlayers()) {
		GameMessages::SendDropClientLoot(player, entity.GetObjectID(), dropLOT, 0, entity.GetPosition());
	}
}

void Strip::ProcNormalAction(float deltaTime, ModelComponent& modelComponent) {
	auto& entity = *modelComponent.GetParent();
	auto& nextAction = GetNextAction();
	auto number = nextAction.GetValueParameterDouble();
	auto valueStr = nextAction.GetValueParameterString();
	auto numberAsInt = static_cast<int32_t>(number);
	auto nextActionType = GetNextAction().GetType();

	// TODO replace with switch case and nextActionType with enum
	/* BEGIN Move */
	if (nextActionType == "MoveRight" || nextActionType == "MoveLeft") {
		// X axis
		bool isMoveLeft = nextActionType == "MoveLeft";
		// Default velocity is 3 units per second.
		if (modelComponent.TrySetVelocity(NiPoint3{ isMoveLeft ? -3.0f : 3.0f, 0.0f, 0.0f })) {
			m_PreviousFramePosition = entity.GetPosition();
			m_InActionMove.x = isMoveLeft ? -number : number;
		}
	} else if (nextActionType == "FlyUp" || nextActionType == "FlyDown") {
		// Y axis
		bool isFlyDown = nextActionType == "FlyDown";
		// Default velocity is 3 units per second.
		if (modelComponent.TrySetVelocity(NiPoint3{ 0.0f, isFlyDown ? -3.0f : 3.0f, 0.0f })) {
			m_PreviousFramePosition = entity.GetPosition();
			m_InActionMove.y = isFlyDown ? -number : number;
		}

	} else if (nextActionType == "MoveForward" || nextActionType == "MoveBackward") {
		// Z axis
		bool isMoveBackward = nextActionType == "MoveBackward";
		// Default velocity is 3 units per second.
		if (modelComponent.TrySetVelocity(NiPoint3{ 0.0f, 0.0f, isMoveBackward ? -3.0f : 3.0f })) {
			m_PreviousFramePosition = entity.GetPosition();
			m_InActionMove.z = isMoveBackward ? -number : number;
		}
	}
	/* END Move */

	/* BEGIN Action */
	else if (nextActionType == "Smash") {
		if (!modelComponent.IsUnSmashing()) {
			GameMessages::Smash smash{};
			smash.target = entity.GetObjectID();
			smash.killerID = entity.GetObjectID();
			smash.Send(UNASSIGNED_SYSTEM_ADDRESS);
		}
	} else if (nextActionType == "UnSmash") {
		GameMessages::UnSmash unsmash{};
		unsmash.target = entity.GetObjectID();
		unsmash.duration = number;
		unsmash.builderID = LWOOBJID_EMPTY;
		unsmash.Send(UNASSIGNED_SYSTEM_ADDRESS);
		modelComponent.AddUnSmash();

		m_PausedTime = number;
	} else if (nextActionType == "Wait") {
		m_PausedTime = number;
	} else if (nextActionType == "Chat") {
		bool isOk = Game::chatFilter->IsSentenceOkay(valueStr.data(), eGameMasterLevel::CIVILIAN).empty();
		// In case a word is removed from the whitelist after it was approved
		const auto modelName = "%[Objects_" + std::to_string(entity.GetLOT()) + "_name]";
		if (isOk) ChatPackets::SendChatMessage(UNASSIGNED_SYSTEM_ADDRESS, 12, modelName, entity.GetObjectID(), false, GeneralUtils::ASCIIToUTF16(valueStr));
		PropertyManagementComponent::Instance()->OnChatMessageReceived(valueStr.data());
	} else if (nextActionType == "PrivateMessage") {
		PropertyManagementComponent::Instance()->OnChatMessageReceived(valueStr.data());
	} else if (nextActionType == "PlaySound") {
		GameMessages::PlayBehaviorSound sound;
		sound.target = modelComponent.GetParent()->GetObjectID();
		sound.soundID = numberAsInt;
		sound.Send(UNASSIGNED_SYSTEM_ADDRESS);
	}
	/* END Action */
	/* BEGIN Gameplay */
	else if (nextActionType == "SpawnStromling") {
		Spawn(10495, entity); // Stromling property
	} else if (nextActionType == "SpawnPirate") {
		Spawn(10497, entity); // Maelstrom Pirate property
	} else if (nextActionType == "SpawnRonin") {
		Spawn(10498, entity); // Dark Ronin property
	} else if (nextActionType == "DropImagination") {
		for (; numberAsInt > 0; numberAsInt--) SpawnDrop(935, entity); // 1 Imagination powerup
	} else if (nextActionType == "DropHealth") {
		for (; numberAsInt > 0; numberAsInt--) SpawnDrop(177, entity); // 1 Life powerup
	} else if (nextActionType == "DropArmor") {
		for (; numberAsInt > 0; numberAsInt--) SpawnDrop(6431, entity); // 1 Armor powerup
	}
	/* END Gameplay */
	else {
		static std::set<std::string> g_WarnedActions;
		if (!g_WarnedActions.contains(nextActionType.data())) {
			LOG("Tried to play action (%s) which is not supported.", nextActionType.data());
			g_WarnedActions.insert(nextActionType.data());
		}
	}

	IncrementAction();
}

// Decrement references to the previous state if we have progressed to the next one.
void Strip::RemoveStates(ModelComponent& modelComponent) const {
	const auto& prevAction = GetPreviousAction();
	const auto prevActionType = prevAction.GetType();

	if (prevActionType == "OnInteract") {
		modelComponent.RemoveInteract();
		Game::entityManager->SerializeEntity(modelComponent.GetParent());
	} else if (prevActionType == "UnSmash") {
		modelComponent.RemoveUnSmash();
	}
}

bool Strip::CheckMovement(float deltaTime, ModelComponent& modelComponent) {
	auto& entity = *modelComponent.GetParent();
	const auto& currentPos = entity.GetPosition();
	const auto diff = currentPos - m_PreviousFramePosition;
	const auto [moveX, moveY, moveZ] = m_InActionMove;
	m_PreviousFramePosition = currentPos;

	// Only want to subtract from the move if one is being performed.
	// Starts at true because we may not be doing a move at all.
	// If one is being done, then one of the move_ variables will be non-zero
	bool moveFinished = true;
	NiPoint3 finalPositionAdjustment = NiPoint3Constant::ZERO;
	if (moveX != 0.0f) {
		m_InActionMove.x -= diff.x;
		// If the sign bit is different between the two numbers, then we have finished our move.
		moveFinished = std::signbit(m_InActionMove.x) != std::signbit(moveX);
		finalPositionAdjustment.x = m_InActionMove.x;
	} else if (moveY != 0.0f) {
		m_InActionMove.y -= diff.y;
		// If the sign bit is different between the two numbers, then we have finished our move.
		moveFinished = std::signbit(m_InActionMove.y) != std::signbit(moveY);
		finalPositionAdjustment.y = m_InActionMove.y;
	} else if (moveZ != 0.0f) {
		m_InActionMove.z -= diff.z;
		// If the sign bit is different between the two numbers, then we have finished our move.
		moveFinished = std::signbit(m_InActionMove.z) != std::signbit(moveZ);
		finalPositionAdjustment.z = m_InActionMove.z;
	}

	// Once done, set the in action move & velocity to zero
	if (moveFinished && m_InActionMove != NiPoint3Constant::ZERO) {
		auto entityVelocity = entity.GetVelocity();
		// Zero out only the velocity that was acted on
		if (moveX != 0.0f) entityVelocity.x = 0.0f;
		else if (moveY != 0.0f) entityVelocity.y = 0.0f;
		else if (moveZ != 0.0f) entityVelocity.z = 0.0f;
		modelComponent.SetVelocity(entityVelocity);

		// Do the final adjustment so we will have moved exactly the requested units
		entity.SetPosition(entity.GetPosition() + finalPositionAdjustment);
		m_InActionMove = NiPoint3Constant::ZERO;
	}

	return moveFinished;
}

void Strip::Update(float deltaTime, ModelComponent& modelComponent) {
	// No point in running a strip with only one action.
	// Strips are also designed to have 2 actions or more to run.
	if (!HasMinimumActions()) return;

	// Return if this strip has an active movement action
	if (!CheckMovement(deltaTime, modelComponent)) return;

	// Don't run this strip if we're paused.
	m_PausedTime -= deltaTime;
	if (m_PausedTime > 0.0f) return;

	m_PausedTime = 0.0f;

	// Return here if we're waiting for external interactions to continue.
	if (m_WaitingForAction) return;

	auto& entity = *modelComponent.GetParent();
	auto& nextAction = GetNextAction();

	RemoveStates(modelComponent);

	// Check for trigger blocks and if not a trigger block proc this blocks action
	if (m_NextActionIndex == 0) {
		if (nextAction.GetType() == "OnInteract") {
			modelComponent.AddInteract();
			Game::entityManager->SerializeEntity(entity);
			m_WaitingForAction = true;

		} else if (nextAction.GetType() == "OnChat") {
			Game::entityManager->SerializeEntity(entity);
			m_WaitingForAction = true;
		}
	} else { // should be a normal block
		ProcNormalAction(deltaTime, modelComponent);
	}
}

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

const Action& Strip::GetNextAction() const {
	DluAssert(m_NextActionIndex < m_Actions.size()); return m_Actions[m_NextActionIndex];
}

const Action& Strip::GetPreviousAction() const {
	DluAssert(m_NextActionIndex < m_Actions.size());
	size_t index = m_NextActionIndex == 0 ? m_Actions.size() - 1 : m_NextActionIndex - 1;
	return m_Actions[index];
}
