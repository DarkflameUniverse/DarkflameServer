#include "Strip.h"
#include "CombatMessages.h"

#include "Amf3.h"
#include "ControlBehaviorMsgs.h"
#include "tinyxml2.h"
#include "dEntity/EntityInfo.h"
#include "ModelComponent.h"
#include "ChatPackets.h"
#include "PropertyManagementComponent.h"
#include "PlayerManager.h"
#include "SimplePhysicsComponent.h"

#include "dChatFilter.h"

#include "DluAssert.h"
#include "Loot.h"
#include "PropertyBehaviorActions.h"

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

	if (nextAction.GetType() == PropertyBehaviorActions::ON_INTERACT) {
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
	if (nextAction.GetType() == PropertyBehaviorActions::ON_CHAT && nextAction.GetValueParameterString() == sMessage) {
		IncrementAction();
		m_WaitingForAction = false;
	}
}

void Strip::OnHit() {
	if (m_PausedTime > 0.0f || !HasMinimumActions()) return;

	const auto& nextAction = GetNextAction();
	if (nextAction.GetType() == PropertyBehaviorActions::ON_ATTACK) {
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
	info.rot = QuatUtils::IDENTITY;
	info.spawnerID = entity.GetObjectID();
	auto* const spawnedEntity = Game::entityManager->CreateEntity(info, nullptr, &entity);
	spawnedEntity->AddToGroup("SpawnedPropertyEnemies");
	Game::entityManager->ConstructEntity(spawnedEntity);
}

// Spawns a specific drop for all
void Strip::SpawnDrop(LOT dropLOT, Entity& entity) {
	for (auto* const player : PlayerManager::GetAllPlayers()) {
		GameMessages::DropClientLoot lootMsg{};
		lootMsg.target = player->GetObjectID();
		lootMsg.ownerID = player->GetObjectID();
		lootMsg.sourceID = entity.GetObjectID();
		lootMsg.item = dropLOT;
		lootMsg.count = 1;
		lootMsg.spawnPos = entity.GetPosition();
		Loot::DropItem(*player, lootMsg);
	}
}

void Strip::ProcNormalAction(float deltaTime, ModelComponent& modelComponent, UpdateResult& updateResult) {
	auto& entity = *modelComponent.GetParent();
	auto& nextAction = GetNextAction();
	auto number = nextAction.GetValueParameterDouble();
	auto valueStr = nextAction.GetValueParameterString();
	auto numberAsInt = static_cast<int32_t>(number);
	auto nextActionType = GetNextAction().GetType();
	LOG_DEBUG("Processing Strip Action: %s with number %.2f and string %s", nextActionType.data(), number, valueStr.data());

	// The blocks and what they do are listed in PropertyBehaviorActions (shared with the dashboard's behavior player)
	namespace Actions = PropertyBehaviorActions;
	/* BEGIN Move */
	if (const auto* move = Actions::Find(Actions::MOVES, nextActionType)) {
		const auto& unit = move->axis == 'x' ? NiPoint3Constant::UNIT_X : move->axis == 'y' ? NiPoint3Constant::UNIT_Y : NiPoint3Constant::UNIT_Z;
		// Default velocity is 3 units per second.
		if (modelComponent.TrySetVelocity(unit * move->sign)) {
			m_PreviousFramePosition = entity.GetPosition();
			auto& distance = move->axis == 'x' ? m_InActionMove.x : move->axis == 'y' ? m_InActionMove.y : m_InActionMove.z;
			distance = move->sign < 0 ? -number : number;
		}
	}
	/* END Move */

	/* BEGIN Navigation */
	else if (nextActionType == Actions::SET_SPEED) {
		modelComponent.SetSpeed(number);
	}
	/* END Navigation */

	/* BEGIN Action */
	else if (nextActionType == Actions::SMASH) {
		if (!modelComponent.IsUnSmashing()) {
			GameMessages::Smash smash{};
			smash.target = entity.GetObjectID();
			smash.killerID = entity.GetObjectID();
			smash.Send(UNASSIGNED_SYSTEM_ADDRESS);
		}
	} else if (nextActionType == Actions::UNSMASH) {
		GameMessages::UnSmash unsmash{};
		unsmash.target = entity.GetObjectID();
		unsmash.duration = number;
		unsmash.builderID = LWOOBJID_EMPTY;
		unsmash.Send(UNASSIGNED_SYSTEM_ADDRESS);
		modelComponent.AddUnSmash();

		// since it may take time for the message to relay to clients
		m_PausedTime = number + 0.5f;
	} else if (nextActionType == Actions::WAIT) {
		m_PausedTime = number;
	} else if (nextActionType == Actions::CHAT) {
		bool isOk = Game::chatFilter->IsSentenceOkay(valueStr.data(), eGameMasterLevel::CIVILIAN).empty();
		// In case a word is removed from the whitelist after it was approved
		const auto modelName = "%[Objects_" + std::to_string(entity.GetLOT()) + "_name]";
		if (isOk) {
			ChatPackets::Client::GeneralChatMessage chatMessage;
			chatMessage.chatChannel = 12;
			chatMessage.senderName = LUWString(modelName);
			chatMessage.senderID = entity.GetObjectID();
			chatMessage.message = GeneralUtils::ASCIIToUTF16(valueStr);
			chatMessage.Broadcast();
		}
		PropertyManagementComponent::Instance()->OnChatMessageReceived(valueStr.data());
	} else if (nextActionType == Actions::PRIVATE_MESSAGE) {
		PropertyManagementComponent::Instance()->OnChatMessageReceived(valueStr.data());
	} else if (nextActionType == Actions::PLAY_SOUND) {
		GameMessages::PlayBehaviorSound sound;
		sound.target = modelComponent.GetParent()->GetObjectID();
		sound.soundID = numberAsInt;
		sound.Send(UNASSIGNED_SYSTEM_ADDRESS);
	} else if (nextActionType == Actions::RESTART) {
		modelComponent.RestartAtEndOfFrame();
	}
	/* END Action */
	/* BEGIN Gameplay */
	else if (const auto* spawn = Actions::Find(Actions::SPAWNS, nextActionType)) {
		Spawn(spawn->lot, entity);
	} else if (const auto* drop = Actions::Find(Actions::DROPS, nextActionType)) {
		for (; numberAsInt > 0; numberAsInt--) SpawnDrop(drop->lot, entity);
	}
	/* END Gameplay */
	/* BEGIN StateMachine */
	else if (const auto* stateChange = Actions::Find(Actions::STATE_CHANGES, nextActionType)) {
		updateResult.newState = stateChange->state;
	}
	/* END StateMachine*/
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

	if (prevActionType == PropertyBehaviorActions::ON_INTERACT) {
		modelComponent.RemoveInteract();
		Game::entityManager->SerializeEntity(modelComponent.GetParent());
	} else if (prevActionType == PropertyBehaviorActions::ON_ATTACK) {
		modelComponent.RemoveAttack();
	} else if (prevActionType == PropertyBehaviorActions::UNSMASH) {
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

void Strip::Update(float deltaTime, ModelComponent& modelComponent, UpdateResult& updateResult) {
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
		LOG("Behavior strip started %s", nextAction.GetType().data());
		if (nextAction.GetType() == PropertyBehaviorActions::ON_INTERACT) {
			modelComponent.AddInteract();
		} else if (nextAction.GetType() == PropertyBehaviorActions::ON_CHAT) {
			// logic here if needed
		} else if (nextAction.GetType() == PropertyBehaviorActions::ON_ATTACK) {
			modelComponent.AddAttack();
		}
		Game::entityManager->SerializeEntity(entity);
		m_WaitingForAction = true;
	} else { // should be a normal block
		ProcNormalAction(deltaTime, modelComponent, updateResult);
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
