#include "ModelComponent.h"
#include "Entity.h"
#include "dLogger.h"

ModelComponent::ModelComponent(uint32_t componentID, Entity* parent) : Component(parent)
{
	m_Position = m_Parent->GetDefaultPosition();
	m_Rotation = m_Parent->GetDefaultRotation();

	m_userModelID = m_Parent->GetVarAs<LWOOBJID>(u"userModelID");

	/*
	for (auto set : m_Parent->GetInfo().settings) {
		if (set && set->GetKey() == u"userModelID") {
			m_userModelID = std::stoull(set->GetValueAsString());
		}
	}
	*/
}

ModelComponent::~ModelComponent() {
	for (auto element : behaviors) {
		delete element;
		element = nullptr;
	}
}

void ModelComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	//item component:
	outBitStream->Write1();
	outBitStream->Write<LWOOBJID>(m_userModelID);
	outBitStream->Write<int>(0);
	outBitStream->Write0();
	
	//actual model component:
	outBitStream->Write1(); //yes we are writing model info
	outBitStream->Write0(); //??
	outBitStream->Write<int>(2); //model type, always 2 for BBB

	outBitStream->Write(m_Position);
	outBitStream->Write(m_Rotation);

	outBitStream->Write1(); //second data flag, all unknown. Maybe skip?
	outBitStream->Write<int>(0);
	outBitStream->Write1();
	outBitStream->Write0();
}

void ModelComponent::AddStrip(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, uint32_t &behaviorID, std::string parameterName, std::string actionParameter, double actionParameterValue, 
		std::string callbackID, double xPosition, double yPosition, std::string behaviorName)
{
	Game::logger->Log("ModelComponent", "Adding new action to strip %i in state %i!\n", stripID, stateID);

	auto behavior = FindBehavior(behaviorID);

	behavior->AddStrip(stateID, stripID, actionName, parameterName, actionParameter, actionParameterValue, callbackID, xPosition, yPosition, behaviorID, behaviorName);

	Game::logger->Log("ModelComponent", "Added new action to strip %i in state %i!\n", stripID, stateID);
}

void ModelComponent::AddAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string actionParameter, double actionParameterValue, 
		std::string callbackID, uint32_t actionIndex, uint32_t behaviorID)
{
	Game::logger->Log("ModelComponent", "Adding new action to existing strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);

	auto behavior = FindBehavior(behaviorID);

	behavior->AddAction(stateID, stripID, actionName, parameterName, actionParameter, actionParameterValue, callbackID, actionIndex, behaviorID);

	Game::logger->Log("ModelComponent", "Added new action to existing strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);
}

void ModelComponent::RemoveAction(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t actionIndex, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Removing action(s) from strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);

	auto behavior = FindBehavior(behaviorID);

	behavior->RemoveAction(stateID, stripID, actionIndex, behaviorID);

	Game::logger->Log("ModelComponent", "Removed action(s) from strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);
}

void ModelComponent::RemoveStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Removing strip %i in state %i!\n", stripID, stateID);

	auto behavior = FindBehavior(behaviorID);

	behavior->RemoveStrip(stateID, stripID, behaviorID);

	Game::logger->Log("ModelComponent", "Removed strip %i in state %i!\n", stripID, stateID);
}

void ModelComponent::UpdateUIOfStrip(BEHAVIORSTATE stateID, STRIPID stripID, double xPosition, double yPosition, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Updating position of strip %i in state %i to position x %lf y %lf!\n", stripID, stateID, xPosition, yPosition);

	auto behavior = FindBehavior(behaviorID);

	behavior->UpdateUIOfStrip(stateID, stripID, xPosition, yPosition, behaviorID);

	Game::logger->Log("ModelComponent", "Updated position of strip %i in state %i to position x %lf y %lf!\n", stripID, stateID, xPosition, yPosition);
}

void ModelComponent::Rename(uint32_t behaviorID, std::string newName) {
	Game::logger->Log("ModelComponent", "Renaming behavior %i to %s!\n", behaviorID, newName.c_str());

	auto behavior = FindBehavior(behaviorID);

	behavior->Rename(behaviorID, newName);

	Game::logger->Log("ModelComponent", "Renamed behavior %i to %s!\n", behaviorID, newName.c_str());
}

void ModelComponent::AddBehavior(uint32_t behaviorID, uint32_t behaviorIndex) {
	Game::logger->Log("ModelComponent", "Adding behavior %i in index %i!\n", behaviorID, behaviorIndex);
	if (behaviors.size() >= 5) return;
	for (auto behavior : behaviors) {
		// Don't allow duplicates.  For some reason the client won't render duplicates in the behaviors list.
		if (behavior->GetBehaviorID() == behaviorID) return;
	}
	auto behavior = new ModelBehavior(behaviorID);
	behaviors.insert(behaviors.begin() + behaviorIndex, behavior);
	Game::logger->Log("ModelComponent", "Added behavior %i in index %i!\n", behaviorID, behaviorIndex);
}

ModelBehavior* ModelComponent::FindBehavior(uint32_t& behaviorID) {
	// Drop in here if we are creating a new behavior to create a new behavior with a unique ID
	if (behaviorID == -1) {
		for (uint32_t i = 0; i < 5; i++) {
			if (behaviors.size() == 0) {
				Game::logger->Log("ModelComponent", "Creating first behavior with id %i\n", i);
				behaviorID = i;
				auto newBehavior = new ModelBehavior(i, false);
				behaviors.insert(behaviors.begin(), newBehavior);
				return newBehavior;				
			}
			bool isUniqueId = true;
			for (auto behavior : behaviors) {
				if (behavior->GetBehaviorID() == i) isUniqueId = false;
			}
			if (isUniqueId) {
				Game::logger->Log("ModelComponent", "Creating a new behavior with id %i\n", i);
				behaviorID = i;
				auto newBehavior = new ModelBehavior(i, false);
				behaviors.insert(behaviors.begin(), newBehavior);
				return newBehavior;
			}
		}
	}
	for (auto behavior : behaviors) {
		Game::logger->Log("ModelComponent", "Finding behavior with id %i (compared is %i)\n", behaviorID, behavior->GetBehaviorID());
		if (behavior->GetBehaviorID() == behaviorID) return behavior;
	}
	if (behaviors.size() < 5) {
		Game::logger->Log("ModelComponent", "Creating a new behavior with id %i\n", behaviorID);
		auto behavior = new ModelBehavior(behaviorID);
		behaviors.insert(behaviors.begin(), behavior);
		return behavior;
	}
	Game::logger->Log("ModelComponent", "Couldn't find behavior with id %i\n", behaviorID);
	return nullptr;
}

void ModelComponent::MoveBehaviorToInventory(uint32_t behaviorID, uint32_t behaviorIndex) {
	Game::logger->Log("ModelComponent", "Moving behavior %i at index %i to inventory!\n", behaviorID, behaviorIndex);

	auto behavior = FindBehavior(behaviorID);

	delete behaviors[behaviorIndex];
	behaviors[behaviorIndex] = nullptr;
	behaviors.erase(behaviors.begin() + behaviorIndex);

	Game::logger->Log("ModelComponent", "Moved behavior %i at index %i to inventory!\n", behaviorID, behaviorIndex);
}

void ModelComponent::RearrangeStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t srcActionIndex, uint32_t dstActionIndex, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Rearranging strip in behavior %i at state %i to destination %i!\n", behaviorID, stateID, dstActionIndex);

	auto behavior = FindBehavior(behaviorID);

	behavior->RearrangeStrip(stateID, stripID, srcActionIndex, dstActionIndex, behaviorID);

	Game::logger->Log("ModelComponent", "Rearranged strip in behavior %i at state %i to destination %i!\n", behaviorID, stateID, dstActionIndex);
}

void ModelComponent::MigrateActions(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, uint32_t dstActionIndex, STRIPID dstStripID, BEHAVIORSTATE dstStateID, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Migrating actions from strip %i in behavior %i at index %i to strip %i in behavior %i at index %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID, behaviorID, dstActionIndex);

	auto behavior = FindBehavior(behaviorID);

	behavior->MigrateActions(srcActionIndex, srcStripID, srcStateID, dstActionIndex, dstStripID, dstStateID, behaviorID);

	Game::logger->Log("ModelComponent", "Migrated actions from strip %i in behavior %i at index %i to strip %i in behavior %i at index %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID, behaviorID, dstActionIndex);
}

void ModelComponent::SplitStrip(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, STRIPID dstStripID, BEHAVIORSTATE dstStateID, uint32_t behaviorID, double yPosition, double xPosition) {
	Game::logger->Log("ModelComponent", "Splitting strip %i in behavior %i at index %i to strip %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID);
	
	auto behavior = FindBehavior(behaviorID);

	behavior->SplitStrip(srcActionIndex, srcStripID, srcStateID, dstStripID, dstStateID, behaviorID, yPosition, xPosition);

	Game::logger->Log("ModelComponent", "Split strip %i in behavior %i at index %i to strip %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID);
}

void ModelComponent::MergeStrips(STRIPID srcStripID, STRIPID dstStripID, BEHAVIORSTATE srcStateID, BEHAVIORSTATE dstStateID, uint32_t behaviorID, uint32_t dstActionIndex) {
	Game::logger->Log("ModelComponent", "Merging strips in behavior %i (source strip %i source state %i) (destination strip %i destination state %i destination index %i)\n", behaviorID, srcStripID, srcStateID, dstStripID, dstStateID, dstActionIndex);

	auto behavior = FindBehavior(behaviorID);

	behavior->MergeStrips(srcStripID, dstStripID, srcStateID, dstStateID, behaviorID, dstActionIndex);

	Game::logger->Log("ModelComponent", "Merged strips in behavior %i (source strip %i source state %i) (destination strip %i destination state %i destination index %i)\n", behaviorID, srcStripID, srcStateID, dstStripID, dstStateID, dstActionIndex);
}

void ModelComponent::UpdateAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string actionParameter, double actionParameterValue, 
		std::string callbackID, uint32_t actionIndex, uint32_t behaviorID) 
{
	Game::logger->Log("ModelComponent", "Updating action %i with parameters %s %s %lf in state %i strip %i behavior ID %i\n", actionIndex, actionName.c_str(), parameterName.c_str(), actionParameter.c_str(), actionParameterValue, stateID, stripID, behaviorID);

	auto behavior = FindBehavior(behaviorID);

	behavior->UpdateAction(stateID, stripID, actionName, parameterName, actionParameter, actionParameterValue, callbackID, actionIndex, behaviorID);

	Game::logger->Log("ModelComponent", "Updated action %i with parameters %s %s %lf in state %i strip %i behavior ID %i\n", actionIndex, actionName.c_str(), parameterName.c_str(), actionParameter.c_str(), actionParameterValue, stateID, stripID, behaviorID);
}