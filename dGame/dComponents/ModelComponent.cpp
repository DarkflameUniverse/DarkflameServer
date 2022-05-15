#include "ModelComponent.h"
#include "Entity.h"
#include "PropertyManagementComponent.h"
#include "Character.h"
#include "MovementAIComponent.h"
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
	// Item component:
	outBitStream->Write1();
	if (m_Parent->GetLOT() == 14) outBitStream->Write<LWOOBJID>(m_userModelID);
	else outBitStream->Write<uint64_t>(m_Parent->GetLOT());
	outBitStream->Write<int32_t>(0);
	outBitStream->Write0();
	
	// Actual model component:
	outBitStream->Write1(); // Yes we are writing model info
		outBitStream->Write(m_IsPickable); // Is pickable (can this be interacted with)
		outBitStream->Write<int32_t>(m_ModelType); // Model type - 

		outBitStream->Write(m_Position);
		outBitStream->Write(m_Rotation);

	outBitStream->Write1();
		outBitStream->Write<uint32_t>(behaviors.size());
		outBitStream->Write(m_IsPaused);
		outBitStream->Write0(); // Doesn't seem to affect anything not having this...
}

void ModelComponent::Update(float deltaTime) {
	if (m_ResetOnNextUpdate) {
		m_ResetOnNextUpdate = false;
		m_Parent->CancelCallbackTimers();
		GameMessages::SendUnSmash(m_Parent, m_Parent->GetObjectID(), 0.0f);
		for (auto behavior : behaviors) {
			behavior->FindStarterBlocks();
		}
		EntityManager::Instance()->SerializeEntity(m_Parent);
		return;
	}
	if (m_IsPaused) {
		totalDelta = 0.0f;
		return;
	}

	totalDelta += deltaTime;
	// bleh
	if (totalDelta >= 10.0f && !PropertyManagementComponent::Instance()->GetOwner()->GetCharacter()->GetBuildMode()) {
		for (auto behavior : behaviors) {
			behavior->FindStarterBlocks();
		}
		EntityManager::Instance()->SerializeEntity(m_Parent);
		totalDelta = 0.0f;
	}

	secondDelta += deltaTime;
	if (moveTowardsInteractor && secondDelta >= 0.5f) {
		auto movementAIComponent = m_Parent->GetComponent<MovementAIComponent>();
		if (!movementAIComponent || !interactor) return;
		movementAIComponent->SetDestination(interactor->GetPosition());
		EntityManager::Instance()->SerializeEntity(m_Parent);
		secondDelta = 0.0f;
	}
}

void ModelComponent::OnUse(Entity* originator) {
	if (!m_IsPickable) return;
	SetOnInteract(false);

	for (auto behavior : behaviors) {
		behavior->OnInteract(originator);
	}
	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void ModelComponent::AddStrip(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, uint32_t &behaviorID, std::string parameterName, std::string parameterValueString, double parameterValueDouble, 
		std::string callbackID, double xPosition, double yPosition)
{
	Game::logger->Log("ModelComponent", "Adding new action to strip %i in state %i!\n", stripID, stateID);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->AddStrip(stateID, stripID, actionName, parameterName, parameterValueString, parameterValueDouble, callbackID, xPosition, yPosition);

	Game::logger->Log("ModelComponent", "Added new action to strip %i in state %i!\n", stripID, stateID);
}

void ModelComponent::AddAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble, 
		std::string callbackID, uint32_t actionIndex, uint32_t behaviorID)
{
	Game::logger->Log("ModelComponent", "Adding new action to existing strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->AddAction(stateID, stripID, actionName, parameterName, parameterValueString, parameterValueDouble, callbackID, actionIndex);

	Game::logger->Log("ModelComponent", "Added new action to existing strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);
}

void ModelComponent::RemoveAction(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t actionIndex, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Removing action(s) from strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->RemoveAction(stateID, stripID, actionIndex);

	Game::logger->Log("ModelComponent", "Removed action(s) from strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);
}

void ModelComponent::RemoveStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Removing strip %i in state %i!\n", stripID, stateID);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->RemoveStrip(stateID, stripID);

	Game::logger->Log("ModelComponent", "Removed strip %i in state %i!\n", stripID, stateID);
}

void ModelComponent::UpdateUIOfStrip(BEHAVIORSTATE stateID, STRIPID stripID, double xPosition, double yPosition, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Updating position of strip %i in state %i to position x %lf y %lf!\n", stripID, stateID, xPosition, yPosition);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->UpdateUIOfStrip(stateID, stripID, xPosition, yPosition);

	Game::logger->Log("ModelComponent", "Updated position of strip %i in state %i to position x %lf y %lf!\n", stripID, stateID, xPosition, yPosition);
}

void ModelComponent::Rename(uint32_t behaviorID, std::string newName) {
	Game::logger->Log("ModelComponent", "Renaming behavior %i to %s!\n", behaviorID, newName.c_str());

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->Rename(newName);

	Game::logger->Log("ModelComponent", "Renamed behavior %i to %s!\n", behaviorID, newName.c_str());
}

void ModelComponent::AddBehavior(uint32_t behaviorID, uint32_t behaviorIndex) {
	Game::logger->Log("ModelComponent", "Adding behavior %i in index %i!\n", behaviorID, behaviorIndex);

	// There is a client side cap of 5 behaviors
	if (behaviors.size() >= 5) return;

	for (auto behavior : behaviors) {
		// Don't allow duplicates.  For some reason the client won't render duplicate behaviors in the behaviors list
		if (behavior->GetBehaviorID() == behaviorID) return;
	}

	auto behavior = new ModelBehavior(behaviorID, m_Parent);
	behaviors.insert(behaviors.begin() + behaviorIndex, behavior);
	Game::logger->Log("ModelComponent", "Added behavior %i in index %i!\n", behaviorID, behaviorIndex);
}

ModelBehavior* ModelComponent::FindBehavior(uint32_t& behaviorID) {
	// Drop in here if we are creating a new behavior to create a new behavior with a unique ID
	if (behaviorID == -1) {
		for (uint32_t i = 0; i < 5; i++) {
			bool isUniqueId = true;
			for (auto behavior : behaviors) {
				if (behavior->GetBehaviorID() == i) isUniqueId = false;
			}
			if (isUniqueId) {
				Game::logger->Log("ModelComponent", "Creating a new custom behavior with id %i\n", i);
				behaviorID = i;
				auto newBehavior = new ModelBehavior(i, m_Parent, false);
				behaviors.insert(behaviors.begin(), newBehavior);
				return newBehavior;
			}
		}
	}
	for (auto behavior : behaviors) {
		Game::logger->Log("ModelComponent", "Trying to find behavior with id %i.  Candidate is %i\n", behaviorID, behavior->GetBehaviorID());
		if (behavior->GetBehaviorID() == behaviorID) return behavior;
	}
	if (behaviors.size() < 5) {
		Game::logger->Log("ModelComponent", "Creating a new templated behavior with id %i\n", behaviorID);
		auto behavior = new ModelBehavior(behaviorID, m_Parent);
		behaviors.insert(behaviors.begin(), behavior);
		return behavior;
	}
	Game::logger->Log("ModelComponent", "Couldn't find behavior with id %i\n", behaviorID);
	return nullptr;
}

void ModelComponent::MoveBehaviorToInventory(uint32_t behaviorID, uint32_t behaviorIndex) {
	Game::logger->Log("ModelComponent", "Moving behavior %i at index %i to inventory!\n", behaviorID, behaviorIndex);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	delete behaviors[behaviorIndex];
	behaviors[behaviorIndex] = nullptr;
	behaviors.erase(behaviors.begin() + behaviorIndex);

	Game::logger->Log("ModelComponent", "Moved behavior %i at index %i to inventory!\n", behaviorID, behaviorIndex);
}

void ModelComponent::RearrangeStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t srcActionIndex, uint32_t dstActionIndex, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Rearranging strip in behavior %i at state %i to destination %i!\n", behaviorID, stateID, dstActionIndex);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->RearrangeStrip(stateID, stripID, srcActionIndex, dstActionIndex);

	Game::logger->Log("ModelComponent", "Rearranged strip in behavior %i at state %i to destination %i!\n", behaviorID, stateID, dstActionIndex);
}

void ModelComponent::MigrateActions(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, uint32_t dstActionIndex, STRIPID dstStripID, BEHAVIORSTATE dstStateID, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Migrating actions from strip %i in behavior %i at index %i to strip %i in behavior %i at index %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID, behaviorID, dstActionIndex);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->MigrateActions(srcActionIndex, srcStripID, srcStateID, dstActionIndex, dstStripID, dstStateID);

	Game::logger->Log("ModelComponent", "Migrated actions from strip %i in behavior %i at index %i to strip %i in behavior %i at index %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID, behaviorID, dstActionIndex);
}

void ModelComponent::SplitStrip(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, STRIPID dstStripID, BEHAVIORSTATE dstStateID, uint32_t behaviorID, double yPosition, double xPosition) {
	Game::logger->Log("ModelComponent", "Splitting strip %i in behavior %i at index %i to strip %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID);
	
	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->SplitStrip(srcActionIndex, srcStripID, srcStateID, dstStripID, dstStateID, yPosition, xPosition);

	Game::logger->Log("ModelComponent", "Split strip %i in behavior %i at index %i to strip %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID);
}

void ModelComponent::MergeStrips(STRIPID srcStripID, STRIPID dstStripID, BEHAVIORSTATE srcStateID, BEHAVIORSTATE dstStateID, uint32_t behaviorID, uint32_t dstActionIndex) {
	Game::logger->Log("ModelComponent", "Merging strips in behavior %i (source strip %i source state %i) (destination strip %i destination state %i destination index %i)\n", behaviorID, srcStripID, srcStateID, dstStripID, dstStateID, dstActionIndex);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->MergeStrips(srcStripID, dstStripID, srcStateID, dstStateID, dstActionIndex);

	Game::logger->Log("ModelComponent", "Merged strips in behavior %i (source strip %i source state %i) (destination strip %i destination state %i destination index %i)\n", behaviorID, srcStripID, srcStateID, dstStripID, dstStateID, dstActionIndex);
}

void ModelComponent::UpdateAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble, 
		std::string callbackID, uint32_t actionIndex, uint32_t behaviorID) 
{
	Game::logger->Log("ModelComponent", "Updating action %i with parameters %s %s %s %lf in state %i strip %i behavior ID %i\n", actionIndex, actionName.c_str(), parameterName.c_str(), parameterValueString.c_str(), parameterValueDouble, stateID, stripID, behaviorID);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->UpdateAction(stateID, stripID, actionName, parameterName, parameterValueString, parameterValueDouble, callbackID, actionIndex);

	Game::logger->Log("ModelComponent", "Updated action %i with parameters %s %s %s %lf in state %i strip %i behavior ID %i\n", actionIndex, actionName.c_str(), parameterName.c_str(), parameterValueString.c_str(), parameterValueDouble, stateID, stripID, behaviorID);
}

void ModelComponent::MoveTowardsInteractor(Entity* interactor) {
	this->interactor = interactor;
	this->moveTowardsInteractor = true;
}