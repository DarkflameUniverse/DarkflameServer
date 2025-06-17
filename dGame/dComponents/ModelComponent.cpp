#include "ModelComponent.h"
#include "Entity.h"

#include "Game.h"
#include "Logger.h"

#include "BehaviorStates.h"
#include "ControlBehaviorMsgs.h"
#include "tinyxml2.h"
#include "SimplePhysicsComponent.h"

#include "Database.h"
#include "DluAssert.h"

ModelComponent::ModelComponent(Entity* parent) : Component(parent) {
	m_OriginalPosition = m_Parent->GetDefaultPosition();
	m_OriginalRotation = m_Parent->GetDefaultRotation();
	m_IsPaused = false;
	m_NumListeningInteract = 0;

	m_userModelID = m_Parent->GetVarAs<LWOOBJID>(u"userModelID");
	RegisterMsg(MessageType::Game::REQUEST_USE, this, &ModelComponent::OnRequestUse);
	RegisterMsg(MessageType::Game::RESET_MODEL_TO_DEFAULTS, this, &ModelComponent::OnResetModelToDefaults);
}

bool ModelComponent::OnResetModelToDefaults(GameMessages::GameMsg& msg) {
	auto& reset = static_cast<GameMessages::ResetModelToDefaults&>(msg);
	for (auto& behavior : m_Behaviors) behavior.HandleMsg(reset);
	GameMessages::UnSmash unsmash;
	unsmash.target = GetParent()->GetObjectID();
	unsmash.duration = 0.0f;
	unsmash.Send(UNASSIGNED_SYSTEM_ADDRESS);

	m_Parent->SetPosition(m_OriginalPosition);
	m_Parent->SetRotation(m_OriginalRotation);
	m_Parent->SetVelocity(NiPoint3Constant::ZERO);

	m_NumListeningInteract = 0;
	m_NumActiveUnSmash = 0;
	m_Dirty = true;
	Game::entityManager->SerializeEntity(GetParent());

	return true;
}

bool ModelComponent::OnRequestUse(GameMessages::GameMsg& msg) {
	bool toReturn = false;
	if (!m_IsPaused) {
		auto& requestUse = static_cast<GameMessages::RequestUse&>(msg);
		for (auto& behavior : m_Behaviors) behavior.HandleMsg(requestUse);
		toReturn = true;
	}

	return toReturn;
}

void ModelComponent::Update(float deltaTime) {
	if (m_IsPaused) return;

	for (auto& behavior : m_Behaviors) {
		behavior.Update(deltaTime, *this);
	}
}

void ModelComponent::LoadBehaviors() {
	auto behaviors = GeneralUtils::SplitString(m_Parent->GetVar<std::string>(u"userModelBehaviors"), ',');
	for (const auto& behavior : behaviors) {
		if (behavior.empty()) continue;

		const auto behaviorId = GeneralUtils::TryParse<int32_t>(behavior);
		if (!behaviorId.has_value() || behaviorId.value() == 0) continue;

		LOG_DEBUG("Loading behavior %d", behaviorId.value());
		auto& inserted = m_Behaviors.emplace_back();
		inserted.SetBehaviorId(*behaviorId);

		const auto behaviorStr = Database::Get()->GetBehavior(behaviorId.value());

		tinyxml2::XMLDocument behaviorXml;
		auto res = behaviorXml.Parse(behaviorStr.c_str(), behaviorStr.size());
		LOG_DEBUG("Behavior %i %d: %s", res, behaviorId.value(), behaviorStr.c_str());

		const auto* const behaviorRoot = behaviorXml.FirstChildElement("Behavior");
		if (!behaviorRoot) {
			LOG("Failed to load behavior %d due to missing behavior root", behaviorId.value());
			continue;
		}
		inserted.Deserialize(*behaviorRoot);
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

void ModelComponent::UpdatePendingBehaviorId(const int32_t newId) {
	for (auto& behavior : m_Behaviors) if (behavior.GetBehaviorId() == -1) behavior.SetBehaviorId(newId);
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

void ModelComponent::SendBehaviorBlocksToClient(int32_t behaviorToSend, AMFArrayValue& args) const {
	args.Insert("BehaviorID", std::to_string(behaviorToSend));
	args.Insert("objectID", std::to_string(m_Parent->GetObjectID()));
	for (auto& behavior : m_Behaviors) if (behavior.GetBehaviorId() == behaviorToSend) behavior.SendBehaviorBlocksToClient(args);
}

void ModelComponent::AddBehavior(AddMessage& msg) {
	// Can only have 1 of the loot behaviors
	for (auto& behavior : m_Behaviors) if (behavior.GetBehaviorId() == msg.GetBehaviorId()) return;
	m_Behaviors.insert(m_Behaviors.begin() + msg.GetBehaviorIndex(), PropertyBehavior());
	m_Behaviors.at(msg.GetBehaviorIndex()).HandleMsg(msg);
	auto* const simplePhysComponent = m_Parent->GetComponent<SimplePhysicsComponent>();
	if (simplePhysComponent) {
		simplePhysComponent->SetPhysicsMotionState(1);
		Game::entityManager->SerializeEntity(m_Parent);
	}
}

void ModelComponent::MoveToInventory(MoveToInventoryMessage& msg) {
	if (msg.GetBehaviorIndex() >= m_Behaviors.size() || m_Behaviors.at(msg.GetBehaviorIndex()).GetBehaviorId() != msg.GetBehaviorId()) return;
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

std::array<std::pair<int32_t, std::string>, 5> ModelComponent::GetBehaviorsForSave() const {
	std::array<std::pair<int32_t, std::string>, 5> toReturn{};
	for (auto i = 0; i < m_Behaviors.size(); i++) {
		const auto& behavior = m_Behaviors.at(i);
		if (behavior.GetBehaviorId() == -1) continue;
		auto& [id, behaviorData] = toReturn[i];
		id = behavior.GetBehaviorId();

		tinyxml2::XMLDocument doc;
		auto* root = doc.NewElement("Behavior");
		behavior.Serialize(*root);
		doc.InsertFirstChild(root);

		tinyxml2::XMLPrinter printer(0, true, 0);
		doc.Print(&printer);
		behaviorData = printer.CStr();
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

bool ModelComponent::TrySetVelocity(const NiPoint3& velocity) const {
	auto currentVelocity = m_Parent->GetVelocity();

	// If we're currently moving on an axis, prevent the move so only 1 behavior can have control over an axis
	if (velocity != NiPoint3Constant::ZERO) {
		const auto [x, y, z] = velocity;
		if (x != 0.0f) {
			if (currentVelocity.x != 0.0f) return false;
			currentVelocity.x = x;
		} else if (y != 0.0f) {
			if (currentVelocity.y != 0.0f) return false;
			currentVelocity.y = y;
		} else if (z != 0.0f) {
			if (currentVelocity.z != 0.0f) return false;
			currentVelocity.z = z;
		}
	} else {
		currentVelocity = velocity;
	}

	m_Parent->SetVelocity(currentVelocity);
	return true;
}

void ModelComponent::SetVelocity(const NiPoint3& velocity) const {
	m_Parent->SetVelocity(velocity);
}

void ModelComponent::OnChatMessageReceived(const std::string& sMessage) {
	for (auto& behavior : m_Behaviors) behavior.OnChatMessageReceived(sMessage);
}
