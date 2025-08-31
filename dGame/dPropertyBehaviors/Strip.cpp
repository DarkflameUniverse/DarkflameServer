#include "Strip.h"

#include "Amf3.h"
#include "ControlBehaviorMsgs.h"
#include "tinyxml2.h"
#include "dEntity/EntityInfo.h"
#include "ModelComponent.h"
#include "ChatPackets.h"
#include "PropertyManagementComponent.h"
#include "PlayerManager.h"
#include "SimplePhysicsComponent.h"
#include "dMath.h"

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
	m_InActionTranslation = NiPoint3Constant::ZERO;
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

void Strip::OnHit() {
	if (m_PausedTime > 0.0f || !HasMinimumActions()) return;

	const auto& nextAction = GetNextAction();
	if (nextAction.GetType() == "OnAttack") {
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
	LOG("~number: %f, nextActionType: %s", static_cast<float>(number), nextActionType.data());

	// TODO replace with switch case and nextActionType with enum
	/* BEGIN Move */
	if (nextActionType == "MoveRight" || nextActionType == "MoveLeft") {
		m_IsRotating = false;
		// X axis
		bool isMoveLeft = nextActionType == "MoveLeft";
		int negative = isMoveLeft ? -1 : 1;
		// Default velocity is 3 units per second.
		if (modelComponent.TrySetVelocity(NiPoint3Constant::UNIT_X * negative)) {
			m_PreviousFramePosition = entity.GetPosition();
			m_InActionTranslation.x = isMoveLeft ? -number : number;
		}
	} else if (nextActionType == "FlyUp" || nextActionType == "FlyDown") {
		m_IsRotating = false;
		// Y axis
		bool isFlyDown = nextActionType == "FlyDown";
		int negative = isFlyDown ? -1 : 1;
		// Default velocity is 3 units per second.
		if (modelComponent.TrySetVelocity(NiPoint3Constant::UNIT_Y * negative)) {
			m_PreviousFramePosition = entity.GetPosition();
			m_InActionTranslation.y = isFlyDown ? -number : number;
		}

	} else if (nextActionType == "MoveForward" || nextActionType == "MoveBackward") {
		m_IsRotating = false;
		// Z axis
		bool isMoveBackward = nextActionType == "MoveBackward";
		int negative = isMoveBackward ? -1 : 1;
		// Default velocity is 3 units per second.
		if (modelComponent.TrySetVelocity(NiPoint3Constant::UNIT_Z * negative)) {
			m_PreviousFramePosition = entity.GetPosition();
			m_InActionTranslation.z = isMoveBackward ? -number : number;
		}
	}
	/* END Move */

	/* BEGIN Rotate */
	else if (nextActionType == "Spin" || nextActionType == "SpinNegative") {
		const float radians = Math::DegToRad(number);
		bool isSpinNegative = nextActionType == "SpinNegative";
		float negative = isSpinNegative ? -0.261799f : 0.261799f;

		// Default angular velocity is 3 units per second.
		if (modelComponent.TrySetAngularVelocity(NiPoint3Constant::UNIT_Y * negative)) {
			m_IsRotating = true;
			m_InActionTranslation.y = isSpinNegative ? -number : number;
			m_PreviousFrameRotation = entity.GetRotation();
			// d/vi = t
			// radians/velocity = time
			// only care about the time, direction is irrelevant here
		}
	} else if (nextActionType == "Tilt" || nextActionType == "TiltNegative") {
		const float radians = Math::DegToRad(number);
		bool isRotateLeft = nextActionType == "TiltNegative";
		float negative = isRotateLeft ? -0.261799f : 0.261799f;

		// Default angular velocity is 3 units per second.
		if (modelComponent.TrySetAngularVelocity(NiPoint3Constant::UNIT_X * negative)) {
			m_IsRotating = true;
			m_InActionTranslation.x = isRotateLeft ? -number : number;
			m_PreviousFrameRotation = entity.GetRotation();
		}
	} else if (nextActionType == "Roll" || nextActionType == "RollNegative") {
		const float radians = Math::DegToRad(number);
		bool isRotateDown = nextActionType == "RollNegative";
		float negative = isRotateDown ? -0.261799f : 0.261799f;
		
		// Default angular velocity is 3 units per second.
		if (modelComponent.TrySetAngularVelocity(NiPoint3Constant::UNIT_Z * negative)) {
			m_IsRotating = true;
			m_InActionTranslation.z = isRotateDown ? -number : number;
			m_PreviousFrameRotation = entity.GetRotation();
		}
	}
	/* END Rotate */

	/* BEGIN Navigation */
	else if (nextActionType == "SetSpeed") {
		modelComponent.SetSpeed(number);
	}
	/* END Navigation */

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
	} else if (nextActionType == "Restart") {
		modelComponent.RestartAtEndOfFrame();
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
	} else if (prevActionType == "OnAttack") {
		modelComponent.RemoveAttack();
	} else if (prevActionType == "UnSmash") {
		modelComponent.RemoveUnSmash();
	}
}

bool Strip::CheckMovement(float deltaTime, ModelComponent& modelComponent) {
	if (m_IsRotating) return true;

	auto& entity = *modelComponent.GetParent();
	const auto& currentPos = entity.GetPosition();
	const auto diff = currentPos - m_PreviousFramePosition;
	const auto [moveX, moveY, moveZ] = m_InActionTranslation;
	m_PreviousFramePosition = currentPos;
	// Only want to subtract from the move if one is being performed.
	// Starts at true because we may not be doing a move at all.
	// If one is being done, then one of the move_ variables will be non-zero
	bool moveFinished = true;
	NiPoint3 finalPositionAdjustment = NiPoint3Constant::ZERO;
	if (moveX != 0.0f) {
		m_InActionTranslation.x -= diff.x;
		// If the sign bit is different between the two numbers, then we have finished our move.
		moveFinished = std::signbit(m_InActionTranslation.x) != std::signbit(moveX);
		finalPositionAdjustment.x = m_InActionTranslation.x;
	} else if (moveY != 0.0f) {
		m_InActionTranslation.y -= diff.y;
		// If the sign bit is different between the two numbers, then we have finished our move.
		moveFinished = std::signbit(m_InActionTranslation.y) != std::signbit(moveY);
		finalPositionAdjustment.y = m_InActionTranslation.y;
	} else if (moveZ != 0.0f) {
		m_InActionTranslation.z -= diff.z;
		// If the sign bit is different between the two numbers, then we have finished our move.
		moveFinished = std::signbit(m_InActionTranslation.z) != std::signbit(moveZ);
		finalPositionAdjustment.z = m_InActionTranslation.z;
	}

	// Once done, set the in action move & velocity to zero
	if (moveFinished && m_InActionTranslation != NiPoint3Constant::ZERO) {
		auto entityVelocity = entity.GetVelocity();
		// Zero out only the velocity that was acted on
		if (moveX != 0.0f) entityVelocity.x = 0.0f;
		else if (moveY != 0.0f) entityVelocity.y = 0.0f;
		else if (moveZ != 0.0f) entityVelocity.z = 0.0f;
		modelComponent.SetVelocity(entityVelocity);

		// Do the final adjustment so we will have moved exactly the requested units
		entity.SetPosition(entity.GetPosition() + finalPositionAdjustment);
		m_InActionTranslation = NiPoint3Constant::ZERO;
	}

	return moveFinished;
}

bool Strip::CheckRotation(float deltaTime, ModelComponent& modelComponent) {
	if (!m_IsRotating) return true;
	GameMessages::GetAngularVelocity getAngVel{};
	getAngVel.target = modelComponent.GetParent()->GetObjectID();
	getAngVel.Send();
	const auto curRotation = modelComponent.GetParent()->GetRotation();
	const auto diff = m_PreviousFrameRotation.Diff(curRotation).GetEulerAngles();
	LOG("Diff: x=%f, y=%f, z=%f", std::abs(Math::RadToDeg(diff.x)), std::abs(Math::RadToDeg(diff.y)), std::abs(Math::RadToDeg(diff.z)));
	LOG("Velocity: x=%f, y=%f, z=%f", Math::RadToDeg(getAngVel.angVelocity.x) * deltaTime, Math::RadToDeg(getAngVel.angVelocity.y) * deltaTime, Math::RadToDeg(getAngVel.angVelocity.z) * deltaTime);
	m_PreviousFrameRotation = curRotation;

	// Convert frame delta (radians) to absolute degrees moved this frame per axis.
	// Use the reported angular velocity (radians/sec) * deltaTime instead of extracting
	// Euler angles from the quaternion difference. Extracting Euler angles from a
	// combined-axis quaternion won't produce per-axis rotations when axes rotate
	// simultaneously, which caused late stopping. Using angular velocity is consistent
	// with how velocity is applied in SimplePhysicsComponent.
	NiPoint3 angMovedDegrees = NiPoint3(std::abs(Math::RadToDeg(getAngVel.angVelocity.x) * deltaTime),
									   std::abs(Math::RadToDeg(getAngVel.angVelocity.y) * deltaTime),
									   std::abs(Math::RadToDeg(getAngVel.angVelocity.z) * deltaTime));

	const auto [rotateX, rotateY, rotateZ] = m_InActionTranslation;
	bool rotateFinished = true; // assume finished until an axis proves otherwise
	NiPoint3 finalRotationAdjustment = NiPoint3Constant::ZERO;

	// Use a small epsilon to avoid missing the exact-zero case due to floating point
	constexpr float EPS_DEG = 1e-3f;

	// Handle each axis independently so we can rotate on multiple axes at once.
	if (rotateX != 0.0f) {
		m_InActionTranslation.x -= angMovedDegrees.x;
		// Finished if we crossed zero or are within epsilon
		if (std::signbit(m_InActionTranslation.x) != std::signbit(rotateX) || std::abs(m_InActionTranslation.x) <= EPS_DEG) {
			finalRotationAdjustment.x = Math::DegToRad(m_InActionTranslation.x);
			m_InActionTranslation.x = 0.0f;
		} else {
			rotateFinished = false;
		}
	}

	if (rotateY != 0.0f) {
		m_InActionTranslation.y -= angMovedDegrees.y;
		if (std::signbit(m_InActionTranslation.y) != std::signbit(rotateY) || std::abs(m_InActionTranslation.y) <= EPS_DEG) {
			finalRotationAdjustment.y = Math::DegToRad(m_InActionTranslation.y);
			m_InActionTranslation.y = 0.0f;
		} else {
			rotateFinished = false;
		}
	}

	if (rotateZ != 0.0f) {
		m_InActionTranslation.z -= angMovedDegrees.z;
		if (std::signbit(m_InActionTranslation.z) != std::signbit(rotateZ) || std::abs(m_InActionTranslation.z) <= EPS_DEG) {
			finalRotationAdjustment.z = Math::DegToRad(m_InActionTranslation.z);
			m_InActionTranslation.z = 0.0f;
		} else {
			rotateFinished = false;
		}
	}

	if (rotateFinished && (finalRotationAdjustment != NiPoint3Constant::ZERO)) {
		LOG("Rotation finished, zeroing angVel for finished axes");

		// Zero only the angular velocity channels that have just finished.
		if (rotateX != 0.0f) getAngVel.angVelocity.x = 0.0f;
		if (rotateY != 0.0f) getAngVel.angVelocity.y = 0.0f;
		if (rotateZ != 0.0f) getAngVel.angVelocity.z = 0.0f;

		GameMessages::SetAngularVelocity setAngVel{};
		setAngVel.target = modelComponent.GetParent()->GetObjectID();
		setAngVel.angVelocity = getAngVel.angVelocity;
		setAngVel.Send();

		// Do the final adjustment so we will have rotated exactly the requested units
		auto currentRot = modelComponent.GetParent()->GetRotation();
		NiQuaternion finalAdjustment = NiQuaternion::FromEulerAngles(finalRotationAdjustment);
		currentRot *= finalAdjustment;
		currentRot.Normalize();
		modelComponent.GetParent()->SetRotation(currentRot);

		// If all axes are zeroed out then stop rotating
		if (m_InActionTranslation == NiPoint3Constant::ZERO) {
			m_IsRotating = false;
		}
	}

	LOG("angVel: x=%f, y=%f, z=%f", m_InActionTranslation.x, m_InActionTranslation.y, m_InActionTranslation.z);
	return rotateFinished;
}

void Strip::Update(float deltaTime, ModelComponent& modelComponent) {
	// No point in running a strip with only one action.
	// Strips are also designed to have 2 actions or more to run.
	if (!HasMinimumActions()) return;

	// Return if this strip has an active movement or rotation action
	if (!CheckMovement(deltaTime, modelComponent)) return;
	if (!CheckRotation(deltaTime, modelComponent)) return;

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
		} else if (nextAction.GetType() == "OnChat") {
			// logic here if needed
		} else if (nextAction.GetType() == "OnAttack") {
			modelComponent.AddAttack();
		}
		Game::entityManager->SerializeEntity(entity);
		m_WaitingForAction = true;
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
