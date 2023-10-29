#include "Recorder.h"

#include "ControllablePhysicsComponent.h"
#include "GameMessages.h"
#include "InventoryComponent.h"
#include "../dWorldServer/ObjectIDManager.h"
#include "ChatPackets.h"
#include "EntityManager.h"
#include "EntityInfo.h"
#include "ServerPreconditions.hpp"

using namespace Cinema::Recording;

std::unordered_map<LWOOBJID, Recorder*> m_Recorders = {};

std::unordered_map<int32_t, std::string> m_EffectAnimations = {};

Recorder::Recorder() {
	this->m_StartTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
	this->m_IsRecording = false;
	this->m_LastRecordTime = this->m_StartTime;
}

Recorder::~Recorder() {
}

void Recorder::AddRecord(Record* record)
{
	if (!this->m_IsRecording) {
		return;
	}

	LOG("Adding record");

	const auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

	// Time since the last record time in seconds
	record->m_Delay = (currentTime - this->m_LastRecordTime).count() / 1000.0f;

	m_LastRecordTime = currentTime;

	this->m_Records.push_back(record);
}

void Recorder::Act(Entity* actor, Play* variables) {
	LOG("Acting %d steps", m_Records.size());

	// Loop through all records
	ActingDispatch(actor, 0, variables);
}

void Recorder::ActingDispatch(Entity* actor, size_t index, Play* variables) {
	if (index >= m_Records.size()) {
		return;
	}

	auto* record = m_Records[index];

	// Check if the record is a fork
	auto* forkRecord = dynamic_cast<ForkRecord*>(record);

	if (forkRecord) {
		if (variables == nullptr) {
			// Skip the fork
			ActingDispatch(actor, index + 1, variables);
			return;
		}

		bool success = false;

		if (!forkRecord->variable.empty()) {
			const auto& variable = variables->variables.find(forkRecord->variable);

			if (variable != variables->variables.end()) {
				success = variable->second == forkRecord->value;
			}
		} else if (!forkRecord->precondition.empty()) {
			auto precondtion = Preconditions::CreateExpression(forkRecord->precondition);

			success = precondtion.Check(actor);
		} else {
			success = true;
		}

		if (success) {
			// Find the success record
			for (auto i = 0; i < m_Records.size(); ++i) {
				auto* record = m_Records[i];

				if (record->m_Name == forkRecord->success) {
					ActingDispatch(actor, i, variables);
					return;
				}
			}

			LOG("Failed to find fork label success: %s", forkRecord->success.c_str());

			return;
		}
		else {
			// Find the failure record
			for (auto i = 0; i < m_Records.size(); ++i) {
				auto* record = m_Records[i];

				if (record->m_Name == forkRecord->failure) {
					ActingDispatch(actor, i, variables);
					return;
				}
			}

			LOG("Failed to find fork label failure: %s", forkRecord->failure.c_str());

			return;
		}
	}

	// Check if the record is a jump
	auto* jumpRecord = dynamic_cast<JumpRecord*>(record);

	if (jumpRecord) {
		// Find the jump record
		for (auto i = 0; i < m_Records.size(); ++i) {
			auto* record = m_Records[i];

			if (record->m_Name == jumpRecord->label) {
				ActingDispatch(actor, i, variables);
				return;
			}
		}

		LOG("Failed to find jump label: %s", jumpRecord->label.c_str());

		return;
	}

	// Check if the record is a set variable
	auto* setVariableRecord = dynamic_cast<SetVariableRecord*>(record);

	if (setVariableRecord) {
		if (variables == nullptr) {
			// Skip the set variable
			ActingDispatch(actor, index + 1, variables);
			return;
		}

		variables->variables[setVariableRecord->variable] = setVariableRecord->value;

		ActingDispatch(actor, index + 1, variables);
		return;
	}

	// Check if the record is a barrier
	auto* barrierRecord = dynamic_cast<BarrierRecord*>(record);

	if (barrierRecord) {
		if (variables == nullptr) {
			// Skip the barrier
			ActingDispatch(actor, index + 1, variables);
			return;
		}

		auto actionTaken = std::make_shared<bool>(false);

		variables->SetupBarrier(barrierRecord->signal, [this, actor, index, variables, actionTaken]() {
			if (*actionTaken) {
				return;
			}

			*actionTaken = true;

			ActingDispatch(actor, index + 1, variables);
		});
		
		if (barrierRecord->timeout <= 0.0001f) {
			return;
		}

		Game::entityManager->GetZoneControlEntity()->AddCallbackTimer(barrierRecord->timeout, [this, barrierRecord, actor, index, variables, actionTaken]() {
			if (*actionTaken) {
				return;
			}

			*actionTaken = true;

			for (auto i = 0; i < m_Records.size(); ++i) {
				auto* record = m_Records[i];

				if (record->m_Name == barrierRecord->timeoutLabel) {
					ActingDispatch(actor, i, variables);
					return;
				}
			}
		});

		return;
	}

	// Check if the record is a signal
	auto* signalRecord = dynamic_cast<SignalRecord*>(record);

	if (signalRecord) {
		if (variables != nullptr) {
			variables->SignalBarrier(signalRecord->signal);
		}
	}

	// Check if the record is a conclude
	auto* concludeRecord = dynamic_cast<ConcludeRecord*>(record);

	if (concludeRecord) {
		if (variables != nullptr) {
			variables->Conclude();
		}
	}

	// Check if the record is a visibility record
	auto* visibilityRecord = dynamic_cast<VisibilityRecord*>(record);

	if (visibilityRecord) {
		if (visibilityRecord->visible) {
			ServerPreconditions::AddExcludeFor(actor->GetObjectID(), variables->player);
		} else {
			ServerPreconditions::RemoveExcludeFor(actor->GetObjectID(), variables->player);
		}
	}

	// Check if the record is a player proximity record
	auto* playerProximityRecord = dynamic_cast<PlayerProximityRecord*>(record);

	if (playerProximityRecord) {
		if (variables == nullptr) {
			// Skip the player proximity record
			ActingDispatch(actor, index + 1, variables);
			return;
		}

		auto actionTaken = std::make_shared<bool>(false);

		PlayerProximityDispatch(actor, index, variables, actionTaken);

		if (playerProximityRecord->timeout <= 0.0f) {
			return;
		}

		Game::entityManager->GetZoneControlEntity()->AddCallbackTimer(playerProximityRecord->timeout, [this, playerProximityRecord, actor, index, variables, actionTaken]() {
			if (*actionTaken) {
				return;
			}

			*actionTaken = true;

			for (auto i = 0; i < m_Records.size(); ++i) {
				auto* record = m_Records[i];

				if (record->m_Name == playerProximityRecord->timeoutLabel) {
					ActingDispatch(actor, i, variables);
					return;
				}
			}
		});

		return;
	}

	actor->AddCallbackTimer(record->m_Delay, [this, actor, index, variables]() {
		ActingDispatch(actor, index + 1, variables);
	});

	record->Act(actor);
}

void Cinema::Recording::Recorder::PlayerProximityDispatch(Entity* actor, size_t index, Play* variables, std::shared_ptr<bool> actionTaken) {
	auto* record = dynamic_cast<PlayerProximityRecord*>(m_Records[index]);
	auto* player = Game::entityManager->GetEntity(variables->player);

	if (player == nullptr || record == nullptr) {
		return;
	}

	const auto& playerPosition = player->GetPosition();
	const auto& actorPosition = actor->GetPosition();

	const auto distance = NiPoint3::Distance(playerPosition, actorPosition);

	if (distance <= record->distance) {
		if (*actionTaken) {
			return;
		}

		*actionTaken = true;

		ActingDispatch(actor, index + 1, variables);

		return;
	}
	
	Game::entityManager->GetZoneControlEntity()->AddCallbackTimer(1.0f, [this, actor, index, variables, actionTaken]() {
		PlayerProximityDispatch(actor, index, variables, actionTaken);
	});
}

Entity* Recorder::ActFor(Entity* actorTemplate, Entity* player, Play* variables) {
	EntityInfo info;
	info.lot = actorTemplate->GetLOT();
	info.pos = actorTemplate->GetPosition();
	info.rot = actorTemplate->GetRotation();
	info.scale = 1;
	info.spawner = nullptr;
	info.spawnerID = player->GetObjectID();
	info.spawnerNodeID = 0;
	info.settings = {
		new LDFData<std::vector<std::u16string>>(u"syncLDF", { u"custom_script_client" }),
		new LDFData<std::u16string>(u"custom_script_client", u"scripts\\ai\\SPEC\\MISSION_MINIGAME_CLIENT.lua")
	};

	// Spawn it
	auto* actor = Game::entityManager->CreateEntity(info);

	// Hide the template from the player
	ServerPreconditions::AddExcludeFor(player->GetObjectID(), actorTemplate->GetObjectID());

	// Solo act for the player
	ServerPreconditions::AddSoloActor(actor->GetObjectID(), player->GetObjectID());

	Game::entityManager->ConstructEntity(actor);

	Act(actor, variables);

	return actor;
}

void Recorder::StopActingFor(Entity* actor, Entity* actorTemplate, LWOOBJID playerID) {
	// Remove the exclude for the player
	ServerPreconditions::RemoveExcludeFor(playerID, actorTemplate->GetObjectID());

	Game::entityManager->DestroyEntity(actor);
}

bool Recorder::IsRecording() const {
	return this->m_IsRecording;
}

void Recorder::StartRecording(LWOOBJID actorID) {
	const auto& it = m_Recorders.find(actorID);

	// Delete the old recorder if it exists
	if (it != m_Recorders.end()) {
		delete it->second;
		m_Recorders.erase(it);
	}

	Recorder* recorder = new Recorder();
	m_Recorders.insert_or_assign(actorID, recorder);
	recorder->m_IsRecording = true;
}

void Recorder::StopRecording(LWOOBJID actorID) {
	auto iter = m_Recorders.find(actorID);
	if (iter == m_Recorders.end()) {
		return;
	}

	iter->second->m_IsRecording = false;
}

Recorder* Recorder::GetRecorder(LWOOBJID actorID) {
	auto iter = m_Recorders.find(actorID);
	if (iter == m_Recorders.end()) {
		return nullptr;
	}

	return iter->second;
}

void Cinema::Recording::Recorder::RegisterEffectForActor(LWOOBJID actorID, const int32_t& effectId) {
	auto iter = m_Recorders.find(actorID);
	if (iter == m_Recorders.end()) {
		return;
	}

	auto& recorder = iter->second;

	const auto& effectIter = m_EffectAnimations.find(effectId);

	if (effectIter == m_EffectAnimations.end()) {
		auto statement = CDClientDatabase::CreatePreppedStmt("SELECT animationName FROM BehaviorEffect WHERE effectID = ? LIMIT 1;");

		statement.bind(1, effectId);

		auto result = statement.execQuery();

		if (result.eof()) {
			result.finalize();

			m_EffectAnimations.emplace(effectId, "");
		}
		else {
			const auto animationName = result.getStringField(0);

			m_EffectAnimations.emplace(effectId, animationName);

			result.finalize();

			recorder->AddRecord(new AnimationRecord(animationName));
		}
	}
	else {
		recorder->AddRecord(new AnimationRecord(effectIter->second));
	}

	recorder->AddRecord(new PlayEffectRecord(std::to_string(effectId)));
}

MovementRecord::MovementRecord(const NiPoint3& position, const NiQuaternion& rotation, const NiPoint3& velocity, const NiPoint3& angularVelocity, bool onGround, bool dirtyVelocity, bool dirtyAngularVelocity) {
	this->position = position;
	this->rotation = rotation;
	this->velocity = velocity;
	this->angularVelocity = angularVelocity;
	this->onGround = onGround;
	this->dirtyVelocity = dirtyVelocity;
	this->dirtyAngularVelocity = dirtyAngularVelocity;
}

void MovementRecord::Act(Entity* actor) {
	auto* controllableComponent = actor->GetComponent<ControllablePhysicsComponent>();

	if (controllableComponent) {
		controllableComponent->SetPosition(position);
		controllableComponent->SetRotation(rotation);
		controllableComponent->SetVelocity(velocity);
		controllableComponent->SetAngularVelocity(angularVelocity);
		controllableComponent->SetIsOnGround(onGround);
		controllableComponent->SetDirtyVelocity(dirtyVelocity);
		controllableComponent->SetDirtyAngularVelocity(dirtyAngularVelocity);
	}

	Game::entityManager->SerializeEntity(actor);
}

void MovementRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("MovementRecord");

	element->SetAttribute("x", position.x);
	element->SetAttribute("y", position.y);
	element->SetAttribute("z", position.z);

	element->SetAttribute("qx", rotation.x);
	element->SetAttribute("qy", rotation.y);
	element->SetAttribute("qz", rotation.z);
	element->SetAttribute("qw", rotation.w);

	element->SetAttribute("vx", velocity.x);
	element->SetAttribute("vy", velocity.y);
	element->SetAttribute("vz", velocity.z);

	element->SetAttribute("avx", angularVelocity.x);
	element->SetAttribute("avy", angularVelocity.y);
	element->SetAttribute("avz", angularVelocity.z);

	element->SetAttribute("g", onGround);

	element->SetAttribute("dv", dirtyVelocity);

	element->SetAttribute("dav", dirtyAngularVelocity);

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void MovementRecord::Deserialize(tinyxml2::XMLElement* element) {
	position.x = element->FloatAttribute("x");
	position.y = element->FloatAttribute("y");
	position.z = element->FloatAttribute("z");

	rotation.x = element->FloatAttribute("qx");
	rotation.y = element->FloatAttribute("qy");
	rotation.z = element->FloatAttribute("qz");
	rotation.w = element->FloatAttribute("qw");

	velocity.x = element->FloatAttribute("vx");
	velocity.y = element->FloatAttribute("vy");
	velocity.z = element->FloatAttribute("vz");

	angularVelocity.x = element->FloatAttribute("avx");
	angularVelocity.y = element->FloatAttribute("avy");
	angularVelocity.z = element->FloatAttribute("avz");

	onGround = element->BoolAttribute("g");

	dirtyVelocity = element->BoolAttribute("dv");

	dirtyAngularVelocity = element->BoolAttribute("dav");

	m_Delay = element->DoubleAttribute("t");
}

SpeakRecord::SpeakRecord(const std::string& text) {
	this->text = text;
}

void SpeakRecord::Act(Entity* actor) {
	GameMessages::SendNotifyClientZoneObject(
		actor->GetObjectID(), u"sendToclient_bubble", 0, 0, actor->GetObjectID(), text, UNASSIGNED_SYSTEM_ADDRESS);

	Game::entityManager->SerializeEntity(actor);
}

void SpeakRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("SpeakRecord");

	element->SetAttribute("text", text.c_str());

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void SpeakRecord::Deserialize(tinyxml2::XMLElement* element) {
	text = element->Attribute("text");

	m_Delay = element->DoubleAttribute("t");
}

AnimationRecord::AnimationRecord(const std::string& animation) {
	this->animation = animation;
}

void AnimationRecord::Act(Entity* actor) {
	GameMessages::SendPlayAnimation(actor, GeneralUtils::ASCIIToUTF16(animation));

	Game::entityManager->SerializeEntity(actor);
}

void AnimationRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("AnimationRecord");

	element->SetAttribute("animation", animation.c_str());

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void AnimationRecord::Deserialize(tinyxml2::XMLElement* element) {
	animation = element->Attribute("animation");

	m_Delay = element->DoubleAttribute("t");
}

EquipRecord::EquipRecord(LOT item) {
	this->item = item;
}

void EquipRecord::Act(Entity* actor) {
	auto* inventoryComponent = actor->GetComponent<InventoryComponent>();

	const LWOOBJID id = ObjectIDManager::Instance()->GenerateObjectID();

	const auto& info = Inventory::FindItemComponent(item);
	
	if (inventoryComponent) {
		inventoryComponent->UpdateSlot(info.equipLocation, { id, item, 1, 0 });
	}

	Game::entityManager->SerializeEntity(actor);
}

void EquipRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("EquipRecord");

	element->SetAttribute("item", item);

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void EquipRecord::Deserialize(tinyxml2::XMLElement* element) {
	item = element->IntAttribute("item");

	m_Delay = element->DoubleAttribute("t");
}

UnequipRecord::UnequipRecord(LOT item) {
	this->item = item;
}

void UnequipRecord::Act(Entity* actor) {
	auto* inventoryComponent = actor->GetComponent<InventoryComponent>();

	const auto& info = Inventory::FindItemComponent(item);
	
	if (inventoryComponent) {
		inventoryComponent->RemoveSlot(info.equipLocation);
	}

	Game::entityManager->SerializeEntity(actor);
}

void UnequipRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("UnequipRecord");

	element->SetAttribute("item", item);

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void UnequipRecord::Deserialize(tinyxml2::XMLElement* element) {
	item = element->IntAttribute("item");

	m_Delay = element->DoubleAttribute("t");
}

void ClearEquippedRecord::Act(Entity* actor) {
	auto* inventoryComponent = actor->GetComponent<InventoryComponent>();

	if (inventoryComponent) {
		auto equipped = inventoryComponent->GetEquippedItems();

		for (auto entry : equipped) {
			inventoryComponent->RemoveSlot(entry.first);
		}
	}

	Game::entityManager->SerializeEntity(actor);
}

void ClearEquippedRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("ClearEquippedRecord");

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void ClearEquippedRecord::Deserialize(tinyxml2::XMLElement* element) {
	m_Delay = element->DoubleAttribute("t");
}

void Recorder::SaveToFile(const std::string& filename) {
	tinyxml2::XMLDocument document;

	auto* root = document.NewElement("Recorder");

	for (auto* record : m_Records) {
		record->Serialize(document, root);
	}

	document.InsertFirstChild(root);

	document.SaveFile(filename.c_str());
}

float Recorder::GetDuration() const {
	// Return the sum of all the record delays
	float duration = 0.0f;

	for (auto* record : m_Records) {
		duration += record->m_Delay;
	}

	return duration;
}

Recorder* Recorder::LoadFromFile(const std::string& filename) {
	tinyxml2::XMLDocument document;

	if (document.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS) {
		return nullptr;
	}

	auto* root = document.FirstChildElement("Recorder");

	if (!root) {
		return nullptr;
	}

	Recorder* recorder = new Recorder();

	for (auto* element = root->FirstChildElement(); element; element = element->NextSiblingElement()) {
		const std::string name = element->Name();

		if (!element->Attribute("t")) {
			element->SetAttribute("t", 0.0f);
		}

		if (name == "MovementRecord") {
			MovementRecord* record = new MovementRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "SpeakRecord") {
			SpeakRecord* record = new SpeakRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "AnimationRecord") {
			AnimationRecord* record = new AnimationRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "EquipRecord") {
			EquipRecord* record = new EquipRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "UnequipRecord") {
			UnequipRecord* record = new UnequipRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "ClearEquippedRecord") {
			ClearEquippedRecord* record = new ClearEquippedRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "ForkRecord") {
			ForkRecord* record = new ForkRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "WaitRecord") {
			WaitRecord* record = new WaitRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "JumpRecord") {
			JumpRecord* record = new JumpRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "SetVariableRecord") {
			SetVariableRecord* record = new SetVariableRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "BarrierRecord") {
			BarrierRecord* record = new BarrierRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "SignalRecord") {
			SignalRecord* record = new SignalRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "ConcludeRecord") {
			ConcludeRecord* record = new ConcludeRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "PlayerProximityRecord") {
			PlayerProximityRecord* record = new PlayerProximityRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "VisibilityRecord") {
			VisibilityRecord* record = new VisibilityRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		} else if (name == "PlayEffectRecord") {
			PlayEffectRecord* record = new PlayEffectRecord();
			record->Deserialize(element);
			recorder->m_Records.push_back(record);
		}

		if (element->Attribute("name")) {
			recorder->m_Records.back()->m_Name = element->Attribute("name");
		}
	}

	return recorder;
}

void Recorder::AddRecording(LWOOBJID actorID, Recorder* recorder) {
	const auto& it = m_Recorders.find(actorID);

	// Delete the old recorder if it exists
	if (it != m_Recorders.end()) {
		delete it->second;
		m_Recorders.erase(it);
	}

	m_Recorders.insert_or_assign(actorID, recorder);
}

Cinema::Recording::ForkRecord::ForkRecord(const std::string& variable, const std::string& value, const std::string& success, const std::string& failure) {
	this->variable = variable;
	this->value = value;
	this->success = success;
	this->failure = failure;
}

void Cinema::Recording::ForkRecord::Act(Entity* actor) {
}

void Cinema::Recording::ForkRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("ForkRecord");

	if (!variable.empty()) {
		element->SetAttribute("variable", variable.c_str());
		element->SetAttribute("value", value.c_str());
	}

	element->SetAttribute("success", success.c_str());
	element->SetAttribute("failure", failure.c_str());

	if (!precondition.empty()) {
		element->SetAttribute("precondtion", precondition.c_str());
	}

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void Cinema::Recording::ForkRecord::Deserialize(tinyxml2::XMLElement* element) {
	if (element->Attribute("variable")) {
		variable = element->Attribute("variable");
		value = element->Attribute("value");
	}

	success = element->Attribute("success");
	failure = element->Attribute("failure");

	if (element->Attribute("precondition")) {
		precondition = element->Attribute("precondition");
	}

	m_Delay = element->DoubleAttribute("t");
}

Cinema::Recording::WaitRecord::WaitRecord(float delay) {
	this->m_Delay = delay;
}

void Cinema::Recording::WaitRecord::Act(Entity* actor) {
}

void Cinema::Recording::WaitRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("WaitRecord");

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void Cinema::Recording::WaitRecord::Deserialize(tinyxml2::XMLElement* element) {
	m_Delay = element->DoubleAttribute("t");
}

Cinema::Recording::JumpRecord::JumpRecord(const std::string& label) {
	this->label = label;
}

void Cinema::Recording::JumpRecord::Act(Entity* actor) {
}

void Cinema::Recording::JumpRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("JumpRecord");

	element->SetAttribute("label", label.c_str());

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void Cinema::Recording::JumpRecord::Deserialize(tinyxml2::XMLElement* element) {
	label = element->Attribute("label");

	m_Delay = element->DoubleAttribute("t");
}

Cinema::Recording::SetVariableRecord::SetVariableRecord(const std::string& variable, const std::string& value) {
	this->variable = variable;
	this->value = value;
}

void Cinema::Recording::SetVariableRecord::Act(Entity* actor) {
}

void Cinema::Recording::SetVariableRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("SetVariableRecord");

	element->SetAttribute("variable", variable.c_str());
	element->SetAttribute("value", value.c_str());

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void Cinema::Recording::SetVariableRecord::Deserialize(tinyxml2::XMLElement* element) {
	variable = element->Attribute("variable");
	value = element->Attribute("value");

	m_Delay = element->DoubleAttribute("t");
}

Cinema::Recording::BarrierRecord::BarrierRecord(const std::string& signal, float timeout, const std::string& timeoutLabel) {
	this->signal = signal;
	this->timeout = timeout;
	this->timeoutLabel = timeoutLabel;
}

void Cinema::Recording::BarrierRecord::Act(Entity* actor) {
}

void Cinema::Recording::BarrierRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("BarrierRecord");

	element->SetAttribute("signal", signal.c_str());
	if (timeout > 0.0f) {
		element->SetAttribute("timeout", timeout);
	}

	if (!timeoutLabel.empty()) {
		element->SetAttribute("timeoutLabel", timeoutLabel.c_str());
	}

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void Cinema::Recording::BarrierRecord::Deserialize(tinyxml2::XMLElement* element) {
	signal = element->Attribute("signal");
	
	if (element->Attribute("timeout")) {
		timeout = element->FloatAttribute("timeout");
	}

	if (element->Attribute("timeoutLabel")) {
		timeoutLabel = element->Attribute("timeoutLabel");
	}

	m_Delay = element->DoubleAttribute("t");
}

Cinema::Recording::SignalRecord::SignalRecord(const std::string& signal) {
	this->signal = signal;
}

void Cinema::Recording::SignalRecord::Act(Entity* actor) {
}

void Cinema::Recording::SignalRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("SignalRecord");

	element->SetAttribute("signal", signal.c_str());

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void Cinema::Recording::SignalRecord::Deserialize(tinyxml2::XMLElement* element) {
	signal = element->Attribute("signal");

	m_Delay = element->DoubleAttribute("t");
}

Cinema::Recording::PlayerProximityRecord::PlayerProximityRecord(float distance, float timeout, const std::string& timeoutLabel) {
	this->distance = distance;
	this->timeout = timeout;
	this->timeoutLabel = timeoutLabel;
}

void Cinema::Recording::PlayerProximityRecord::Act(Entity* actor) {
}

void Cinema::Recording::PlayerProximityRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("PlayerProximityRecord");

	element->SetAttribute("distance", distance);
	if (timeout > 0.0f) {
		element->SetAttribute("timeout", timeout);
	}

	if (!timeoutLabel.empty()) {
		element->SetAttribute("timeoutLabel", timeoutLabel.c_str());
	}

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void Cinema::Recording::PlayerProximityRecord::Deserialize(tinyxml2::XMLElement* element) {
	distance = element->FloatAttribute("distance");

	if (element->Attribute("timeout")) {
		timeout = element->FloatAttribute("timeout");
	}

	if (element->Attribute("timeoutLabel")) {
		timeoutLabel = element->Attribute("timeoutLabel");
	}

	m_Delay = element->DoubleAttribute("t");
}

void Cinema::Recording::ConcludeRecord::Act(Entity* actor) {
}

void Cinema::Recording::ConcludeRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("ConcludeRecord");

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void Cinema::Recording::ConcludeRecord::Deserialize(tinyxml2::XMLElement* element) {
	m_Delay = element->DoubleAttribute("t");
}

Cinema::Recording::VisibilityRecord::VisibilityRecord(bool visible) {
	this->visible = visible;
}

void Cinema::Recording::VisibilityRecord::Act(Entity* actor) {
}

void Cinema::Recording::VisibilityRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("VisibilityRecord");

	element->SetAttribute("visible", visible);

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void Cinema::Recording::VisibilityRecord::Deserialize(tinyxml2::XMLElement* element) {
	visible = element->BoolAttribute("visible");

	m_Delay = element->DoubleAttribute("t");
}

Cinema::Recording::PlayEffectRecord::PlayEffectRecord(const std::string& effect) {
	this->effect = effect;
}

void Cinema::Recording::PlayEffectRecord::Act(Entity* actor) {
	int32_t effectID = 0;

	if (!GeneralUtils::TryParse(effect, effectID)) {
		return;
	}

	GameMessages::SendPlayFXEffect(
		actor->GetObjectID(),
		effectID,
		u"cast",
		std::to_string(ObjectIDManager::GenerateRandomObjectID())
	);

	Game::entityManager->SerializeEntity(actor);
}

void Cinema::Recording::PlayEffectRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("PlayEffectRecord");

	element->SetAttribute("effect", effect.c_str());

	element->SetAttribute("t", m_Delay);

	parent->InsertEndChild(element);
}

void Cinema::Recording::PlayEffectRecord::Deserialize(tinyxml2::XMLElement* element) {
	effect = element->Attribute("effect");

	m_Delay = element->DoubleAttribute("t");
}
