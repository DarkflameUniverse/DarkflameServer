#include "ModelComponent.h"
#include "Entity.h"

#include "Game.h"
#include "Logger.h"

#include "BehaviorStates.h"
#include "ControlBehaviorMsgs.h"
#include "tinyxml2.h"

#include "Database.h"

ModelComponent::ModelComponent(Entity* parent) : Component(parent) {
	m_OriginalPosition = m_Parent->GetDefaultPosition();
	m_OriginalRotation = m_Parent->GetDefaultRotation();

	m_userModelID = m_Parent->GetVarAs<LWOOBJID>(u"userModelID");
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
	outBitStream.Write0(); // Is pickable
	outBitStream.Write<uint32_t>(2); // Physics type
	outBitStream.Write(m_OriginalPosition); // Original position
	outBitStream.Write(m_OriginalRotation); // Original rotation

	outBitStream.Write1(); // We are writing behavior info
	outBitStream.Write<uint32_t>(0); // Number of behaviors
	outBitStream.Write1(); // Is this model paused
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
}

void ModelComponent::MoveToInventory(MoveToInventoryMessage& msg) {
	if (msg.GetBehaviorIndex() >= m_Behaviors.size() || m_Behaviors.at(msg.GetBehaviorIndex()).GetBehaviorId() != msg.GetBehaviorId()) return;
	m_Behaviors.erase(m_Behaviors.begin() + msg.GetBehaviorIndex());
	// TODO move to the inventory
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
