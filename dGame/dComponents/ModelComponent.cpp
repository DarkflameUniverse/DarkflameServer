#include "ModelComponent.h"
#include "Entity.h"
#include "PropertyManagementComponent.h"
#include "DestroyableComponent.h"
#include "Character.h"
#include "MovementAIComponent.h"
#include "Database.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "SimplePhysicsComponent.h"
#include "dLogger.h"

ModelComponent::ModelComponent(uint32_t componentID, Entity* parent) : Component(parent)
{
	m_Position = m_Parent->GetDefaultPosition();
	m_Rotation = m_Parent->GetDefaultRotation();

	m_userModelID = m_Parent->GetVarAs<LWOOBJID>(u"userModelID");

	auto loadBehaviorsQuery = Database::CreatePreppedStmt("SELECT concat(behavior_1, \",\" , behavior_2, \",\" , behavior_3, \",\" , behavior_4, \",\" , behavior_5) FROM properties_contents where id = ?;");

	loadBehaviorsQuery->setInt64(1, m_Parent->GetSpawnerID());

	auto result = loadBehaviorsQuery->executeQuery();

	if (!result->next()) return;

	auto behaviorIDList = GeneralUtils::SplitString(std::string(result->getString(1).c_str()), ',');
	for (auto behaviorIDAsStr : behaviorIDList) {
		uint32_t behaviorID = std::stoi(behaviorIDAsStr);
		if (behaviorID != 0) {
			auto behaviorQuery = Database::CreatePreppedStmt("SELECT behavior_info FROM behaviors WHERE id = ?;");

			behaviorQuery->setInt(1, behaviorID);

			auto behaviorQueryResult = behaviorQuery->executeQuery();

			behaviorQueryResult->next();
			tinyxml2::XMLDocument* m_Doc =  new tinyxml2::XMLDocument();
			m_Doc->Parse(behaviorQueryResult->getString(1).c_str());

			LoadBehaviorsFromXml(m_Doc);
		}
	}
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
		if (bIsInitialUpdate) outBitStream->Write0(); // Doesn't seem to affect anything not having this...
}

void ModelComponent::Update(float deltaTime) {
    if (m_ResetOnNextUpdate) {
        m_ResetOnNextUpdate = false;
        m_Parent->CancelCallbackTimers();
        GameMessages::SendUnSmash(m_Parent, m_Parent->GetObjectID(), 0.0f);
        m_Smashed = false;
        for (auto behavior : behaviors) {
            behavior->FindStarterBlocks();
        }
        EntityManager::Instance()->SerializeEntity(m_Parent);
        return;
    }

    if (m_IsPaused)
        return;
	
    // bleh
    if (checkStarterBlocks) {
		Game::logger->Log("ModelComponent", "Default position %f %f %f\n", m_Position.x, m_Position.y, m_Position.z);
        for (auto behavior : behaviors) {
            behavior->FindStarterBlocks();
        }
		checkStarterBlocks = false;
        EntityManager::Instance()->SerializeEntity(m_Parent);
		if (onAttack) {
			auto destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();
			if (destroyableComponent) destroyableComponent->SetFaction(6);
		}
		if (onStartup) {
			for (auto behavior : behaviors) {
				behavior->OnStartup(this);
			}
			onStartup = false;
		}
		if (onTimer) {
			for (auto behavior : behaviors) {
				behavior->OnTimer(this);
			}
			onTimer = false;
		}
	}

    auto movementAIComponent = m_Parent->GetComponent<MovementAIComponent>();
	auto simplePhysicsComponent = m_Parent->GetComponent<SimplePhysicsComponent>();

    if (m_Smashed) {
		if (simplePhysicsComponent) {
			simplePhysicsComponent->SetVelocity(NiPoint3::ZERO);
			simplePhysicsComponent->SetAngularVelocity(NiPoint3::ZERO);
		}
		return;
	}

    if (simplePhysicsComponent && (distanceToTravelY != 0.0f || distanceToTravelX != 0.0f || distanceToTravelZ != 0.0f)) {
        NiPoint3 velocityVector = NiPoint3::ZERO;

        // Vector calculations
        if (distanceToTravelX != 0.0f) velocityVector.x = distanceToTravelX > 0.0f ? 1 : -1;
        if (distanceToTravelY != 0.0f) velocityVector.y = distanceToTravelY > 0.0f ? 1 : -1;
        if (distanceToTravelZ != 0.0f) velocityVector.z = distanceToTravelZ > 0.0f ? 1 : -1;
        // Set the velocity
        simplePhysicsComponent->SetVelocity(velocityVector * GetSpeed());
        // Subtract distance from requested travel distance
        distanceToTravelX -= velocityVector.x * GetSpeed() * deltaTime;
        distanceToTravelY -= velocityVector.y * GetSpeed() * deltaTime;
        distanceToTravelZ -= velocityVector.z * GetSpeed() * deltaTime;

        EntityManager::Instance()->SerializeEntity(m_Parent);
		std::vector<std::function<void()>> finishedActions;
        if (((velocityVector.x < 0 && distanceToTravelX > 0.0f) || (velocityVector.x > 0 && distanceToTravelX < 0.0f))) {
            for (auto callback : xPositionCallbacks) {
                finishedActions.push_back(callback);
            }
			xPositionCallbacks.clear();
			if (xPositionCallbacks.size() == 0) {
				float localDistX = distanceToTravelX;
				m_Parent->AddCallbackTimer(0.0f, [simplePhysicsComponent, localDistX, this](){
					Game::logger->Log("ModelComponent", "x Position is x %f y %f z %f, dist %f\n", simplePhysicsComponent->GetPosition().x, simplePhysicsComponent->GetPosition().y, simplePhysicsComponent->GetPosition().z, localDistX);
					simplePhysicsComponent->SetPosition(simplePhysicsComponent->GetPosition() + NiPoint3::UNIT_X * localDistX);
					Game::logger->Log("ModelComponent", "x Position is x %f y %f z %f, dist %f\n", simplePhysicsComponent->GetPosition().x, simplePhysicsComponent->GetPosition().y, simplePhysicsComponent->GetPosition().z, localDistX);
				});
				distanceToTravelX = 0.0f;
			}
        }
        if (((velocityVector.y < 0 && distanceToTravelY > 0.0f) || (velocityVector.y > 0 && distanceToTravelY < 0.0f))) {
            for (auto callback : yPositionCallbacks) {
                finishedActions.push_back(callback);
            }
			yPositionCallbacks.clear();
			if (yPositionCallbacks.size() == 0) {
				float localDistY = distanceToTravelY;
				m_Parent->AddCallbackTimer(0.0f, [simplePhysicsComponent, localDistY, this](){
					Game::logger->Log("ModelComponent", "y Position is x %f y %f z %f, dist %f\n", simplePhysicsComponent->GetPosition().x, simplePhysicsComponent->GetPosition().y, simplePhysicsComponent->GetPosition().z, localDistY);
					simplePhysicsComponent->SetPosition(simplePhysicsComponent->GetPosition() + NiPoint3::UNIT_Y * localDistY);
					Game::logger->Log("ModelComponent", "y Position is x %f y %f z %f, dist %f\n", simplePhysicsComponent->GetPosition().x, simplePhysicsComponent->GetPosition().y, simplePhysicsComponent->GetPosition().z, localDistY);
				});
				distanceToTravelY = 0.0f;
			}
        }
        if (((velocityVector.z < 0 && distanceToTravelZ > 0.0f) || (velocityVector.z > 0 && distanceToTravelZ < 0.0f))) {
            for (auto callback : zPositionCallbacks) {
                finishedActions.push_back(callback);
            }
			zPositionCallbacks.clear();
			if (zPositionCallbacks.size() == 0) {
				float localDistZ = distanceToTravelZ;
				m_Parent->AddCallbackTimer(0.0f, [simplePhysicsComponent, localDistZ, this](){
					Game::logger->Log("ModelComponent", "z Position is x %f y %f z %f, dist %f\n", simplePhysicsComponent->GetPosition().x, simplePhysicsComponent->GetPosition().y, simplePhysicsComponent->GetPosition().z, localDistZ);
					simplePhysicsComponent->SetPosition(simplePhysicsComponent->GetPosition() + NiPoint3::UNIT_Z * localDistZ);
					Game::logger->Log("ModelComponent", "z Position is x %f y %f z %f, dist %f\n", simplePhysicsComponent->GetPosition().x, simplePhysicsComponent->GetPosition().y, simplePhysicsComponent->GetPosition().z, localDistZ);
				});
				distanceToTravelZ = 0.0f;
			}
        }
		if (finishedActions.size() > 0) {
			for (auto callback : finishedActions) {
				callback();
			}
		}
    } else if (simplePhysicsComponent) {
        simplePhysicsComponent->SetVelocity(NiPoint3::ZERO);
        EntityManager::Instance()->SerializeEntity(m_Parent);
    }
	
	if (simplePhysicsComponent && (degreesToRotateByX != 0.0f || degreesToRotateByY != 0.0f || degreesToRotateByZ != 0.0f)) {

		NiPoint3 rotationVector = NiPoint3::ZERO;

		if (degreesToRotateByX != 0.0f) rotationVector.x = degreesToRotateByX > 0.0f ? 1 : -1;
		if (degreesToRotateByY != 0.0f) rotationVector.y = degreesToRotateByY > 0.0f ? 1 : -1;
		if (degreesToRotateByZ != 0.0f) rotationVector.z = degreesToRotateByZ > 0.0f ? 1 : -1;

		simplePhysicsComponent->SetAngularVelocity(rotationVector * GetSpeed());

		degreesToRotateByX -= rotationVector.x * deltaTime * 180 / 3.14 * GetSpeed();
		degreesToRotateByY -= rotationVector.y * deltaTime * 180 / 3.14 * GetSpeed();
		degreesToRotateByZ -= rotationVector.z * deltaTime * 180 / 3.14 * GetSpeed();

		EntityManager::Instance()->SerializeEntity(m_Parent);

		std::vector<std::function<void()>> finishedActions;
        if (((rotationVector.x < 0 && degreesToRotateByX > 0.0f) || (rotationVector.x > 0 && degreesToRotateByX < 0.0f))) {
            for (auto callback : xRotationCallbacks) {
                finishedActions.push_back(callback);
            }
			xRotationCallbacks.clear();
			if (xRotationCallbacks.size() == 0) degreesToRotateByX = 0.0f;
        }
		if (((rotationVector.y < 0 && degreesToRotateByY > 0.0f) || (rotationVector.y > 0 && degreesToRotateByY < 0.0f))) {
            for (auto callback : yRotationCallbacks) {
                finishedActions.push_back(callback);
            }
			yRotationCallbacks.clear();
			if (yRotationCallbacks.size() == 0) degreesToRotateByY = 0.0f;
        }
		if (((rotationVector.z < 0 && degreesToRotateByZ > 0.0f) || (rotationVector.z > 0 && degreesToRotateByZ < 0.0f))) {
            for (auto callback : zRotationCallbacks) {
                finishedActions.push_back(callback);
            }
			zRotationCallbacks.clear();
			if (zRotationCallbacks.size() == 0) degreesToRotateByZ = 0.0f;
        }
		if (finishedActions.size() > 0) {
			for (auto callback : finishedActions) {
				callback();
			}
		}
	} else if (simplePhysicsComponent) {
        simplePhysicsComponent->SetAngularVelocity(NiPoint3::ZERO);
        EntityManager::Instance()->SerializeEntity(m_Parent);
    }
	if (moveTowardsInteractor && interactor && !m_Smashed) {
		auto toRotate = NiQuaternion::LookAt(m_Parent->GetPosition(), interactor->GetPosition());
		if (simplePhysicsComponent) {
			// TODO look at the coordinate we want to look at and turn towards it with the quaternion and caluclate positional difference using delta between two positions and moving towards the players.
			// My brain hurts
		}
    }
}

void ModelComponent::OnAttack(Entity* attacker) {
	if (!onAttack) return;

	SetOnAttack(false);

	for (auto behavior : behaviors) {
		behavior->OnAttack(this, attacker);
	}
	auto destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();

	if (destroyableComponent) {
		destroyableComponent->SetFaction(-1, true);
	}

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void ModelComponent::OnUse(Entity* originator) {
	if (!m_IsPickable) return;
	SetOnInteract(false);

	for (auto behavior : behaviors) {
		behavior->OnInteract(originator);
	}
	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void ModelComponent::OnChatMessage(Entity* originator, std::string& message) {
	if (!onChatMessage && !m_IsPaused) return;

	SetOnChatMessage(false);

	for (auto behavior : behaviors) {
		behavior->OnChatMessage(this, originator, message);
	}
	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void ModelComponent::AddStrip(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, int32_t &behaviorID, std::string parameterName, std::string parameterValueString, double parameterValueDouble, 
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
		std::string callbackID, uint32_t actionIndex, int32_t behaviorID)
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

void ModelComponent::RemoveAction(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t actionIndex, int32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Removing action(s) from strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->RemoveAction(stateID, stripID, actionIndex);

	Game::logger->Log("ModelComponent", "Removed action(s) from strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);
}

void ModelComponent::RemoveStrip(BEHAVIORSTATE stateID, STRIPID stripID, int32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Removing strip %i in state %i!\n", stripID, stateID);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->RemoveStrip(stateID, stripID);

	Game::logger->Log("ModelComponent", "Removed strip %i in state %i!\n", stripID, stateID);
}

void ModelComponent::UpdateUIOfStrip(BEHAVIORSTATE stateID, STRIPID stripID, double xPosition, double yPosition, int32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Updating position of strip %i in state %i to position x %lf y %lf!\n", stripID, stateID, xPosition, yPosition);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->UpdateUIOfStrip(stateID, stripID, xPosition, yPosition);

	Game::logger->Log("ModelComponent", "Updated position of strip %i in state %i to position x %lf y %lf!\n", stripID, stateID, xPosition, yPosition);
}

void ModelComponent::Rename(int32_t behaviorID, std::string newName) {
	Game::logger->Log("ModelComponent", "Renaming behavior %i to %s!\n", behaviorID, newName.c_str());

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->Rename(newName);

	Game::logger->Log("ModelComponent", "Renamed behavior %i to %s!\n", behaviorID, newName.c_str());
}

void ModelComponent::AddBehavior(int32_t behaviorID, uint32_t behaviorIndex, Entity* modelOwner) {
	Game::logger->Log("ModelComponent", "Adding behavior %i in index %i!\n", behaviorID, behaviorIndex);

	// There is a client side cap of 5 behaviors
	if (behaviors.size() >= 5) return;

	for (auto behavior : behaviors) {
		// Don't allow duplicates.  For some reason the client won't render duplicate behaviors in the behaviors list
		if (behavior->GetBehaviorID() == behaviorID) return;
	}

	auto inventoryComponent = modelOwner->GetComponent<InventoryComponent>();
	bool isLoot = false;
	if (inventoryComponent) {
		auto item = inventoryComponent->FindItemByLot(behaviorID, eInventoryType::BEHAVIORS);
		if (item && item->GetSubKey() == LWOOBJID_EMPTY) {
			isLoot = true;
		}
	}
	if (!isLoot) {
		auto behaviorQuery = Database::CreatePreppedStmt("SELECT behavior_info FROM behaviors WHERE id = ?;");
		
		behaviorQuery->setInt(1, behaviorID);

		auto result = behaviorQuery->executeQuery();

		if (result->next()) {
			tinyxml2::XMLDocument* m_Doc =  new tinyxml2::XMLDocument();
			m_Doc->Parse(result->getString(1).c_str());

			LoadBehaviorsFromXml(m_Doc, behaviorIndex);
			FindBehavior(behaviorID)->SetIsTemplated(true);
		}
	} else {
		auto behavior = new ModelBehavior(behaviorID, this, isLoot, "", true);
		behaviors.insert(behaviors.begin() + behaviorIndex, behavior);
	}
	Game::logger->Log("ModelComponent", "Added behavior %i in index %i!\n", behaviorID, behaviorIndex);
}

ModelBehavior* ModelComponent::FindBehavior(int32_t behaviorID) {
	// Drop in here if we are creating a new behavior to create a new behavior with a unique ID
	for (auto behavior : behaviors) {
		Game::logger->Log("ModelComponent", "Trying to find behavior with id %i.  Candidate is %i\n", behaviorID, behavior->GetBehaviorID());
		if (behavior->GetBehaviorID() == behaviorID) return behavior;
	}
	if (behaviors.size() < 5) {
		Game::logger->Log("ModelComponent", "Creating a new templated behavior with id %i\n", behaviorID);
		auto behavior = new ModelBehavior(behaviorID, this, behaviorID != -1);
		behaviors.insert(behaviors.begin(), behavior);
		return behavior;
	}
	Game::logger->Log("ModelComponent", "Couldn't find behavior with id %i\n", behaviorID);
	return nullptr;
}

void ModelComponent::MoveBehaviorToInventory(int32_t behaviorID, uint32_t behaviorIndex, Entity* modelOwner) {
	Game::logger->Log("ModelComponent", "Moving behavior %i at index %i to inventory!\n", behaviorID, behaviorIndex);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}
	PropertyManagementComponent::Instance()->Save();
	if (!behavior->GetIsLoot()) {
		auto inventoryComponent = modelOwner->GetComponent<InventoryComponent>();
		if (inventoryComponent) {
			auto item = inventoryComponent->FindItemBySubKey(behaviorID, eInventoryType::BEHAVIORS);
			if (!item) {
				inventoryComponent->AddItem(7965, 1, eLootSourceType::LOOT_SOURCE_PROPERTY, eInventoryType::BEHAVIORS, {}, 0LL, false, false, behaviorID);
			}
		}
	}
	
	delete behaviors[behaviorIndex];
	behaviors[behaviorIndex] = nullptr;
	behaviors.erase(behaviors.begin() + behaviorIndex);

	Game::logger->Log("ModelComponent", "Moved behavior %i at index %i to inventory!\n", behaviorID, behaviorIndex);
}

void ModelComponent::RearrangeStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t srcActionIndex, uint32_t dstActionIndex, int32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Rearranging strip in behavior %i at state %i to destination %i!\n", behaviorID, stateID, dstActionIndex);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}
	if (srcActionIndex < dstActionIndex) return;
	behavior->RearrangeStrip(stateID, stripID, srcActionIndex, dstActionIndex);

	Game::logger->Log("ModelComponent", "Rearranged strip in behavior %i at state %i to destination %i!\n", behaviorID, stateID, dstActionIndex);
}

void ModelComponent::MigrateActions(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, uint32_t dstActionIndex, STRIPID dstStripID, BEHAVIORSTATE dstStateID, int32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Migrating actions from strip %i in behavior %i at index %i to strip %i in behavior %i at index %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID, behaviorID, dstActionIndex);

	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->MigrateActions(srcActionIndex, srcStripID, srcStateID, dstActionIndex, dstStripID, dstStateID);

	Game::logger->Log("ModelComponent", "Migrated actions from strip %i in behavior %i at index %i to strip %i in behavior %i at index %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID, behaviorID, dstActionIndex);
}

void ModelComponent::SplitStrip(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, STRIPID dstStripID, BEHAVIORSTATE dstStateID, int32_t behaviorID, double yPosition, double xPosition) {
	Game::logger->Log("ModelComponent", "Splitting strip %i in behavior %i at index %i to strip %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID);
	
	auto behavior = FindBehavior(behaviorID);

	if (!behavior) {
		Game::logger->Log("ModelComponent", "No behavior with id %i found!\n", behaviorID);
		return;
	}

	behavior->SplitStrip(srcActionIndex, srcStripID, srcStateID, dstStripID, dstStateID, yPosition, xPosition);

	Game::logger->Log("ModelComponent", "Split strip %i in behavior %i at index %i to strip %i!\n", srcStripID, behaviorID, srcActionIndex, dstStripID);
}

void ModelComponent::MergeStrips(STRIPID srcStripID, STRIPID dstStripID, BEHAVIORSTATE srcStateID, BEHAVIORSTATE dstStateID, int32_t behaviorID, uint32_t dstActionIndex) {
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
		std::string callbackID, uint32_t actionIndex, int32_t behaviorID) 
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

void ModelComponent::ResetStarterBlocks() {
    onStartup = false;
    onAttack = false;
    onInteract = false;
    onProximityEnter = false;
    onProximityLeave = false;
    onImpact = false;
    onChatMessage = false;
    onTimer = false;
	m_IsPickable = false;
	EntityManager::Instance()->SerializeEntity(m_Parent);
	checkStarterBlocks = true;
}

void ModelComponent::Reset() {
    onStartup = false;
    onAttack = false;
    onInteract = false;
    onProximityEnter = false;
    onProximityLeave = false;
    onImpact = false;
    onChatMessage = false;
    onTimer = false;
    m_IsPickable = false;
    interactor = nullptr;
    moveTowardsInteractor = false;
    EntityManager::Instance()->SerializeEntity(m_Parent);
    m_ResetOnNextUpdate = true;
    m_MoveSpeed = 3.0f;
    distanceToTravelX = 0.0f;
    distanceToTravelY = 0.0f;
    distanceToTravelZ = 0.0f;
    xPositionCallbacks.clear();
    yPositionCallbacks.clear();
    zPositionCallbacks.clear();
    degreesToRotateByX = 0.0f;
    degreesToRotateByY = 0.0f;
    degreesToRotateByZ = 0.0f;
    xRotationCallbacks.clear();
    yRotationCallbacks.clear();
    zRotationCallbacks.clear();
    checkStarterBlocks = true;
    for (auto behavior : behaviors) {
        behavior->SetState(eStates::HOME_STATE);
		behavior->ResetStrips();
    }
	auto destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();
	if (destroyableComponent) {
		destroyableComponent->SetFaction(-1, true);
	}
}

void ModelComponent::CheckStarterBlocks() {
	checkStarterBlocks = true;
}

void ModelComponent::LoadBehaviorsFromXml(tinyxml2::XMLDocument* doc, int32_t behaviorIndex) {
	// This method will setup the modelBehavior and pass along the states, strips and actions to the behavior to handle it.
	auto behaviorInfo = doc->FirstChildElement("Behavior");

	if (behaviorInfo != nullptr) {
		uint32_t behaviorID = 0;
		bool isLoot = false;
		const char* behaviorName = "";
		behaviorInfo->QueryAttribute("behaviorID", &behaviorID);
		Game::logger->Log("ModelComponent", "Adding behavior %i\n", behaviorID);
		behaviorInfo->QueryAttribute("isLoot", &isLoot);
		behaviorInfo->QueryAttribute("behaviorName", &behaviorName);
		ModelBehavior* behavior = new ModelBehavior(behaviorID, this, isLoot, std::string(behaviorName));
	
		behavior->LoadStatesFromXml(behaviorInfo);

		if (behaviorIndex == -1) {
			behaviors.push_back(behavior);
		}
		else {
			behaviors.insert(behaviors.begin() + behaviorIndex, behavior);
		}
		Game::logger->Log("ModelComponent", "Added behavior %i\n", behaviorID);
	}

	Game::logger->Log("ModelComponent", "behaviors size %i\n", behaviors.size());
}