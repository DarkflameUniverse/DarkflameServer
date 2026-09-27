#include "ModelComponent.h"

#include <cmath>

#include "Entity.h"

#include "Game.h"
#include "Logger.h"
#include "dMath.h"

#include "BehaviorStates.h"
#include "ControlBehaviorMsgs.h"
#include "tinyxml2.h"
#include "InventoryComponent.h"
#include "MissionComponent.h"
#include "SimplePhysicsComponent.h"
#include "eMissionTaskType.h"
#include "eObjectBits.h"
#include "DestroyableComponent.h"

#include "Database.h"
#include "DluAssert.h"

ModelComponent::ModelComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	m_OriginalPosition = m_Parent->GetDefaultPosition();
	m_OriginalRotation = m_Parent->GetDefaultRotation();
	m_IsPaused = false;
	m_NumListeningInteract = 0;

	m_userModelID = m_Parent->GetVarAs<LWOOBJID>(u"userModelID");
	RegisterMsg(&ModelComponent::OnRequestUse);
	RegisterMsg(&ModelComponent::OnResetModelToDefaults);
	RegisterMsg(&ModelComponent::OnGetObjectReportInfo);
}

bool ModelComponent::OnResetModelToDefaults(GameMessages::ResetModelToDefaults& reset) {
	if (reset.bResetBehaviors) for (auto& behavior : m_Behaviors) behavior.HandleMsg(reset);

	if (reset.bUnSmash) {
		GameMessages::UnSmash unsmash;
		unsmash.target = GetParent()->GetObjectID();
		unsmash.duration = 0.0f;
		unsmash.Send(UNASSIGNED_SYSTEM_ADDRESS);
		m_NumActiveUnSmash = 0;
	}

	if (reset.bResetPos) m_Parent->SetPosition(m_OriginalPosition);
	if (reset.bResetRot) m_Parent->SetRotation(m_OriginalRotation);
	m_Parent->SetVelocity(NiPoint3Constant::ZERO);
	m_Move = MoveState{};
	SyncLinearVelocity();
	ResetRotationState(m_Parent->GetRotation());

	m_NumListeningInteract = 0;

	m_NumActiveAttack = 0;
	GameMessages::SetFaction set{};
	set.target = m_Parent->GetObjectID();
	set.factionID = -1; // Default faction for smashables
	set.bIgnoreChecks = true; // Remove the attack faction
	set.Send();

	m_Dirty = true;
	Game::entityManager->SerializeEntity(GetParent());

	return true;
}

bool ModelComponent::OnRequestUse(GameMessages::RequestUse& requestUse) {
	bool toReturn = false;
	if (!m_IsPaused) {
		for (auto& behavior : m_Behaviors) behavior.HandleMsg(requestUse);
		toReturn = true;
	}

	return toReturn;
}

void ModelComponent::Update(float deltaTime) {
	if (m_IsPaused) return;
	m_DamageCooldown -= deltaTime;

	// Arrived once this frame's movement reached or passed the target
	if (m_Move.target && (*m_Move.target - m_Parent->GetPosition()).DotProduct(m_Move.targetDirection) <= 0.0f) {
		m_Parent->SetPosition(*m_Move.target);
		m_Move.target.reset();
	}

	for (auto& behavior : m_Behaviors) {
		behavior.Update(deltaTime, *this);
	}

	// Done after all behaviors so the heading reflects any rotation applied this frame
	SyncLinearVelocity();

	if (!m_RestartAtEndOfFrame) return;

	GameMessages::ResetModelToDefaults reset{};
	OnResetModelToDefaults(reset);
	m_RestartAtEndOfFrame = false;
}

void ModelComponent::LoadBehaviors() {
	auto behaviors = GeneralUtils::SplitString(m_Parent->GetVar<std::string>(u"userModelBehaviors"), ',');
	for (const auto& behavior : behaviors) {
		if (behavior.empty()) continue;

		const auto behaviorId = GeneralUtils::TryParse<LWOOBJID>(behavior);
		if (!behaviorId.has_value() || behaviorId.value() == 0) continue;

		// add behavior at the back
		LoadBehavior(behaviorId.value(), m_Behaviors.size(), false);
	}
}

void ModelComponent::LoadBehavior(const LWOOBJID behaviorID, const size_t index, const bool isIndexed) {
	LOG_DEBUG("Loading behavior %d", behaviorID);
	auto& inserted = *m_Behaviors.emplace(m_Behaviors.begin() + index, PropertyBehavior(isIndexed));
	inserted.SetBehaviorId(behaviorID);

	const auto behaviorStr = Database::Get()->GetBehavior(behaviorID);

	tinyxml2::XMLDocument behaviorXml;
	auto res = behaviorXml.Parse(behaviorStr.c_str(), behaviorStr.size());
	LOG_DEBUG("Behavior %i %llu: %s", res, behaviorID, behaviorStr.c_str());

	const auto* const behaviorRoot = behaviorXml.FirstChildElement("Behavior");
	if (behaviorRoot) {
		inserted.Deserialize(*behaviorRoot);
	} else {
		LOG("Failed to load behavior %d due to missing behavior root", behaviorID);
	}
}

void ModelComponent::Resume() {
	m_Dirty = true;
	m_IsPaused = false;
}

void ModelComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	// ItemComponent Serialization.  Pets do not get this serialization.
	if (!m_Parent->HasComponent(eReplicaComponentType::PET)) {
		outBitStream.Write1();
		outBitStream.Write<LWOOBJID>(m_userModelID != LWOOBJID_EMPTY ? m_userModelID : m_Parent->GetObjectID());
		outBitStream.Write<int>(0);
		outBitStream.Write0();
	}

	//actual model component:
	outBitStream.Write1(); // Yes we are writing model info
	outBitStream.Write(m_NumListeningInteract > 0); // Is pickable
	outBitStream.Write<uint32_t>(2); // Physics type
	outBitStream.Write(m_OriginalPosition); // Original position
	outBitStream.Write(m_OriginalRotation); // Original rotation

	outBitStream.Write1(); // We are writing behavior info
	outBitStream.Write<uint32_t>(m_Behaviors.size()); // Number of behaviors
	outBitStream.Write(m_IsPaused); // Is this model paused
	if (bIsInitialUpdate) outBitStream.Write0(); // We are not writing model editing info
}

void ModelComponent::UpdatePendingBehaviorId(const LWOOBJID newId, const LWOOBJID oldId) {
	for (auto& behavior : m_Behaviors) {
		if (behavior.GetBehaviorId() != oldId) continue;
		behavior.SetBehaviorId(newId);
		behavior.SetIsLoot(false);
	}
}

void ModelComponent::SendBehaviorListToClient(AMFArrayValue& args) const {
	args.Insert("objectID", std::to_string(m_Parent->GetObjectID()));

	auto* behaviorArray = args.InsertArray("behaviors");
	for (auto& behavior : m_Behaviors) {
		auto* behaviorArgs = behaviorArray->PushArray();
		behavior.SendBehaviorListToClient(*behaviorArgs);
	}
}

void ModelComponent::VerifyBehaviors() {
	for (auto& behavior : m_Behaviors) behavior.VerifyLastEditedState();
}

void ModelComponent::SendBehaviorBlocksToClient(const LWOOBJID behaviorToSend, AMFArrayValue& args) const {
	args.Insert("BehaviorID", std::to_string(behaviorToSend));
	args.Insert("objectID", std::to_string(m_Parent->GetObjectID()));
	for (auto& behavior : m_Behaviors) if (behavior.GetBehaviorId() == behaviorToSend) behavior.SendBehaviorBlocksToClient(args);
}

void ModelComponent::AddBehavior(AddMessage& msg) {
	// Can only have 1 of the loot behaviors
	for (auto& behavior : m_Behaviors) if (behavior.GetBehaviorId() == msg.GetBehaviorId()) return;

	// If we're loading a behavior from an ADD, it is from the database.
	// Mark it as not modified by default to prevent wasting persistentIDs.
	LoadBehavior(msg.GetBehaviorId(), msg.GetBehaviorIndex(), true);
	auto& insertedBehavior = m_Behaviors[msg.GetBehaviorIndex()];

	auto* const playerEntity = Game::entityManager->GetEntity(msg.GetOwningPlayerID());
	if (playerEntity) {
		auto* inventoryComponent = playerEntity->GetComponent<InventoryComponent>();
		if (inventoryComponent) {
			// Check if this behavior is able to be found via lot (if so, its a loot behavior).
			insertedBehavior.SetIsLoot(inventoryComponent->FindItemByLot(msg.GetBehaviorId(), eInventoryType::BEHAVIORS));
		}
		ProgressAddBehaviorMission(*playerEntity);
	}

	auto* const simplePhysComponent = m_Parent->GetComponent<SimplePhysicsComponent>();
	if (simplePhysComponent) {
		simplePhysComponent->SetPhysicsMotionState(1);
		Game::entityManager->SerializeEntity(m_Parent);
	}
}

void ModelComponent::ProgressAddBehaviorMission(Entity& playerEntity) {
	auto* const missionComponent = playerEntity.GetComponent<MissionComponent>();
	if (missionComponent) missionComponent->Progress(eMissionTaskType::ADD_BEHAVIOR, 0);
}

std::string ModelComponent::SaveBehavior(const PropertyBehavior& behavior) const {
	tinyxml2::XMLDocument doc;
	auto* root = doc.NewElement("Behavior");
	behavior.Serialize(*root);
	doc.InsertFirstChild(root);

	tinyxml2::XMLPrinter printer(0, true, 0);
	doc.Print(&printer);
	return printer.CStr();
}

void ModelComponent::RemoveBehavior(MoveToInventoryMessage& msg, const bool keepItem) {
	if (msg.GetBehaviorIndex() >= m_Behaviors.size() || m_Behaviors.at(msg.GetBehaviorIndex()).GetBehaviorId() != msg.GetBehaviorId()) return;
	const auto behavior = m_Behaviors[msg.GetBehaviorIndex()];
	if (keepItem) {
		auto* const playerEntity = Game::entityManager->GetEntity(msg.GetOwningPlayerID());
		if (playerEntity) {
			auto* const inventoryComponent = playerEntity->GetComponent<InventoryComponent>();
			if (inventoryComponent && !behavior.GetIsLoot()) {
				// config is owned by the item
				LwoNameValue config;
				config.Insert(u"userModelName", behavior.GetName());
				inventoryComponent->AddItem(7965, 1, eLootSourceType::PROPERTY, eInventoryType::BEHAVIORS, config, LWOOBJID_EMPTY, true, false, msg.GetBehaviorId());
			}
		}
	}

	// save the behavior before deleting it so players can re-add them
	IBehaviors::Info info{};
	info.behaviorId = msg.GetBehaviorId();
	info.behaviorInfo = SaveBehavior(behavior);
	info.characterId = msg.GetOwningPlayerID();

	Database::Get()->AddBehavior(info);

	m_Behaviors.erase(m_Behaviors.begin() + msg.GetBehaviorIndex());
	// TODO move to the inventory
	if (m_Behaviors.empty()) {
		auto* const simplePhysComponent = m_Parent->GetComponent<SimplePhysicsComponent>();
		if (simplePhysComponent) {
			simplePhysComponent->SetPhysicsMotionState(4);
			Game::entityManager->SerializeEntity(m_Parent);
		}
	}
}

std::array<std::pair<LWOOBJID, std::string>, 5> ModelComponent::GetBehaviorsForSave() const {
	std::array<std::pair<LWOOBJID, std::string>, 5> toReturn{};
	for (auto i = 0; i < m_Behaviors.size(); i++) {
		const auto& behavior = m_Behaviors.at(i);
		if (behavior.GetBehaviorId() == -1) continue;
		auto& [id, behaviorData] = toReturn[i];
		id = behavior.GetBehaviorId();
		behaviorData = SaveBehavior(behavior);
	}
	return toReturn;
}

void ModelComponent::AddInteract() {
	LOG_DEBUG("Adding interact %i", m_NumListeningInteract);
	m_Dirty = true;
	m_NumListeningInteract++;
}

void ModelComponent::RemoveInteract() {
	DluAssert(m_NumListeningInteract > 0);
	LOG_DEBUG("Removing interact %i", m_NumListeningInteract);
	m_Dirty = true;
	m_NumListeningInteract--;
}

void ModelComponent::AddUnSmash() {
	LOG_DEBUG("Adding UnSmash %i", m_NumActiveUnSmash);
	m_NumActiveUnSmash++;
}

void ModelComponent::RemoveUnSmash() {
	// Players can assign an UnSmash without a Smash so an assert would be bad here
	if (m_NumActiveUnSmash == 0) return;
	LOG_DEBUG("Removing UnSmash %i", m_NumActiveUnSmash);
	m_NumActiveUnSmash--;
}

bool ModelComponent::TryStartMove(const int axis, const float direction, const float speed) {
	if (axis < 0 || axis > 2 || direction == 0.0f || speed <= 0.0f) return false;
	if (m_Move.velocity[axis] != 0.0f) return false;

	m_Move.target.reset();
	m_Move.velocity[axis] = std::copysign(speed, direction);
	return true;
}

void ModelComponent::StopMove(const int axis) {
	if (axis < 0 || axis > 2) return;
	m_Move.velocity[axis] = 0.0f;
}

void ModelComponent::StartMoveTo(const NiPoint3& target, const float speed) {
	m_Move.velocity = NiPoint3Constant::ZERO;
	m_Move.interruptCount++;
	m_Move.target = target;
	m_Move.targetSpeed = speed;
}

void ModelComponent::SyncLinearVelocity() {
	const auto& rotation = m_Parent->GetRotation();
	m_Move.basis = { QuatUtils::Right(rotation), QuatUtils::Up(rotation), QuatUtils::Forward(rotation) };

	NiPoint3 velocity = NiPoint3Constant::ZERO;
	for (int axis = 0; axis < 3; axis++) velocity += m_Move.basis[axis] * m_Move.velocity[axis];

	if (m_Move.target) {
		const auto toTarget = *m_Move.target - m_Parent->GetPosition();
		const float distance = toTarget.Length();
		m_Move.targetDirection = distance > 0.0f ? toTarget / distance : NiPoint3Constant::ZERO;
		velocity += m_Move.targetDirection * m_Move.targetSpeed;
	}

	// Leave velocity alone unless a move owns it, e.g. pets are driven elsewhere
	const bool isMoving = velocity != NiPoint3Constant::ZERO;
	if (!isMoving && !m_Move.wasMoving) return;
	m_Move.wasMoving = isMoving;

	// Setting velocity always marks it dirty for serialization
	if (velocity != m_Parent->GetVelocity()) m_Parent->SetVelocity(velocity);
}

bool ModelComponent::TryStartRotation(const int axis, const float direction, const float speed) {
	if (axis < 0 || axis > 2 || direction == 0.0f || speed <= 0.0f) return false;
	if (m_Rotation.velocity[axis] != 0.0f) return false;

	// Rebase only when nothing is rotating so simultaneous rotations stay relative to the same base
	if (m_Rotation.velocity == NiPoint3Constant::ZERO) ResetRotationState(m_Parent->GetRotation());

	m_Rotation.degrees[axis] = std::fmod(m_Rotation.degrees[axis], 360.0f);
	m_Rotation.actionStart[axis] = m_Rotation.degrees[axis];
	m_Rotation.velocity[axis] = std::copysign(BASE_ANGULAR_SPEED * speed, direction);
	SyncAngularVelocity();
	return true;
}

void ModelComponent::SetRotationProgress(const int axis, const float degrees) {
	if (axis < 0 || axis > 2) return;
	m_Rotation.degrees[axis] = m_Rotation.actionStart[axis] + degrees;

	// Whole turns wrap to exactly 0 so e.g. 720 degrees yields exactly the base rotation
	const NiPoint3 radians(
		Math::DegToRad(std::fmod(m_Rotation.degrees.x, 360.0f)),
		Math::DegToRad(std::fmod(m_Rotation.degrees.y, 360.0f)),
		Math::DegToRad(std::fmod(m_Rotation.degrees.z, 360.0f))
	);
	m_Parent->SetRotation(QuatUtils::FromEuler(radians) * m_Rotation.base);
}

void ModelComponent::StopRotation(const int axis) {
	if (axis < 0 || axis > 2) return;
	m_Rotation.velocity[axis] = 0.0f;
	SyncAngularVelocity();
}

void ModelComponent::SyncAngularVelocity() const {
	GameMessages::SetAngularVelocity setAngVel{};
	setAngVel.target = m_Parent->GetObjectID();
	setAngVel.angVelocity = m_Rotation.velocity * Math::DegToRad(1.0f);
	setAngVel.Send();
}

void ModelComponent::ResetRotationState(const NiQuaternion& newBase) {
	m_Rotation = RotationState{ .base = newBase };
	SyncAngularVelocity();
}

void ModelComponent::OnChatMessageReceived(const std::string& sMessage, const LWOOBJID sender) {
	for (auto& behavior : m_Behaviors) behavior.OnChatMessageReceived(sMessage, sender);
}

void ModelComponent::OnHit(const LWOOBJID attacker) {
	for (auto& behavior : m_Behaviors) {
		behavior.OnHit(attacker);
	}
}

void ModelComponent::AddAttack() {
	LOG_DEBUG("Adding attack %i", m_NumActiveAttack);
	m_Dirty = true;
	if (m_NumActiveAttack == 0) {
		GameMessages::SetFaction set{};
		set.target = m_Parent->GetObjectID();
		set.factionID = 6; // Default faction for smashables
		set.Send();
	}
	m_NumActiveAttack++;
}

void ModelComponent::RemoveAttack() {
	LOG_DEBUG("Removing attack %i", m_NumActiveAttack);
	DluAssert(m_NumActiveAttack > 0);
	m_Dirty = true;
	m_NumActiveAttack--;
	if (m_NumActiveAttack == 0) {
		GameMessages::SetFaction set{};
		set.target = m_Parent->GetObjectID();
		set.factionID = -1; // Default faction for smashables
		set.bIgnoreChecks = true; // Remove the attack faction
		set.Send();
	}
}

bool ModelComponent::OnGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportInfo) {
	if (!reportInfo.info) return false;
	auto& cmptInfo = reportInfo.info->PushDebug("Model Behaviors (Mutable)");
	cmptInfo.PushDebug<AMFIntValue>("Component ID") = GetComponentID();

	cmptInfo.PushDebug<AMFStringValue>("Name") = "Objects_" + std::to_string(m_Parent->GetLOT()) + "_name";
	cmptInfo.PushDebug<AMFBoolValue>("Has Unique Name") = false;
	cmptInfo.PushDebug<AMFStringValue>("UGID (from item)", "LWOOBJID") = std::to_string(m_userModelID);
	cmptInfo.PushDebug<AMFStringValue>("UGID", "LWOOBJID") = std::to_string(m_userModelID);
	cmptInfo.PushDebug<AMFStringValue>("Description") = "";
	cmptInfo.PushDebug<AMFIntValue>("Behavior Count") = m_Behaviors.size();

	return true;
}

void ModelComponent::DoDamage(const LWOOBJID target) {
	// dont do damage if we've done damage recently
	if (m_DamageCooldown > 0.0f) return;
	m_DamageCooldown = 1.0f;
	auto* const initiator = Game::entityManager->GetEntity(target);
	if (initiator) {
		auto* const destComp = initiator->GetComponent<DestroyableComponent>();
		if (destComp) destComp->Damage(1, GetParent()->GetObjectID());
	}
}
