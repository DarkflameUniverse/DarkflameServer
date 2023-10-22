#include "Recorder.h"

#include "ControllablePhysicsComponent.h"
#include "GameMessages.h"
#include "InventoryComponent.h"
#include "../dWorldServer/ObjectIDManager.h"
#include "ChatPackets.h"
#include "EntityManager.h"
#include "EntityInfo.h"
#include "ServerPreconditions.hpp"

using namespace Recording;

std::unordered_map<LWOOBJID, Recorder*> m_Recorders = {};

Recorder::Recorder() {
	this->m_StartTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
	this->m_IsRecording = false;
}

Recorder::~Recorder() {
}

void Recorder::AddRecord(Record* record)
{
	if (!this->m_IsRecording) {
		return;
	}

	Game::logger->Log("Recorder", "Adding record");

	// Time since start of recording
	record->m_Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - this->m_StartTime;

	this->m_MovementRecords.push_back(record);
}

void Recorder::Act(Entity* actor) {
	Game::logger->Log("Recorder", "Acting %d steps", m_MovementRecords.size());

	// Loop through all records
	for (auto* record : m_MovementRecords) {
		record->Act(actor);
	}
}

Entity* Recording::Recorder::ActFor(Entity* actorTemplate, Entity* player) {
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

	Act(actor);

	return actor;
}

void Recording::Recorder::StopActingFor(Entity* actor, Entity* actorTemplate, LWOOBJID playerID) {
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

Recording::MovementRecord::MovementRecord(const NiPoint3& position, const NiQuaternion& rotation, const NiPoint3& velocity, const NiPoint3& angularVelocity, bool onGround, bool dirtyVelocity, bool dirtyAngularVelocity) {
	this->position = position;
	this->rotation = rotation;
	this->velocity = velocity;
	this->angularVelocity = angularVelocity;
	this->onGround = onGround;
	this->dirtyVelocity = dirtyVelocity;
	this->dirtyAngularVelocity = dirtyAngularVelocity;
}

void Recording::MovementRecord::Act(Entity* actor) {
	// Calculate the amount of seconds (as a float) since the start of the recording
	float time = m_Timestamp.count() / 1000.0f;
	
	auto r = *this;

	actor->AddCallbackTimer(time, [actor, r] () {
		auto* controllableComponent = actor->GetComponent<ControllablePhysicsComponent>();

		if (controllableComponent) {
			controllableComponent->SetPosition(r.position);
			controllableComponent->SetRotation(r.rotation);
			controllableComponent->SetVelocity(r.velocity);
			controllableComponent->SetAngularVelocity(r.angularVelocity);
			controllableComponent->SetIsOnGround(r.onGround);
			controllableComponent->SetDirtyVelocity(r.dirtyVelocity);
			controllableComponent->SetDirtyAngularVelocity(r.dirtyAngularVelocity);
		}

		Game::entityManager->SerializeEntity(actor);
	});
}

void Recording::MovementRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
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

	element->SetAttribute("t", m_Timestamp.count());

	parent->InsertEndChild(element);
}

void Recording::MovementRecord::Deserialize(tinyxml2::XMLElement* element) {
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

	m_Timestamp = std::chrono::milliseconds(element->Int64Attribute("t"));
}

Recording::SpeakRecord::SpeakRecord(const std::string& text) {
	this->text = text;
}

void Recording::SpeakRecord::Act(Entity* actor) {
	// Calculate the amount of seconds (as a float) since the start of the recording
	float time = m_Timestamp.count() / 1000.0f;
	
	auto r = *this;

	actor->AddCallbackTimer(time, [actor, r] () {
		GameMessages::SendNotifyClientZoneObject(
			actor->GetObjectID(), u"sendToclient_bubble", 0, 0, actor->GetObjectID(), r.text, UNASSIGNED_SYSTEM_ADDRESS);

		Game::entityManager->SerializeEntity(actor);
	});
}

void Recording::SpeakRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("SpeakRecord");

	element->SetAttribute("text", text.c_str());

	element->SetAttribute("t", m_Timestamp.count());

	parent->InsertEndChild(element);
}

void Recording::SpeakRecord::Deserialize(tinyxml2::XMLElement* element) {
	text = element->Attribute("text");

	m_Timestamp = std::chrono::milliseconds(element->Int64Attribute("t"));
}

Recording::AnimationRecord::AnimationRecord(const std::string& animation) {
	this->animation = animation;
}

void Recording::AnimationRecord::Act(Entity* actor) {
	// Calculate the amount of seconds (as a float) since the start of the recording
	float time = m_Timestamp.count() / 1000.0f;
	
	auto r = *this;

	actor->AddCallbackTimer(time, [actor, r] () {
		GameMessages::SendPlayAnimation(actor, GeneralUtils::ASCIIToUTF16(r.animation));

		Game::entityManager->SerializeEntity(actor);
	});
}

void Recording::AnimationRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("AnimationRecord");

	element->SetAttribute("animation", animation.c_str());

	element->SetAttribute("t", m_Timestamp.count());

	parent->InsertEndChild(element);
}

void Recording::AnimationRecord::Deserialize(tinyxml2::XMLElement* element) {
	animation = element->Attribute("animation");

	m_Timestamp = std::chrono::milliseconds(element->Int64Attribute("t"));
}

Recording::EquipRecord::EquipRecord(LOT item) {
	this->item = item;
}

void Recording::EquipRecord::Act(Entity* actor) {
	// Calculate the amount of seconds (as a float) since the start of the recording
	float time = m_Timestamp.count() / 1000.0f;
	
	auto r = *this;

	actor->AddCallbackTimer(time, [actor, r] () {
		auto* inventoryComponent = actor->GetComponent<InventoryComponent>();

		const LWOOBJID id = ObjectIDManager::Instance()->GenerateObjectID();

		const auto& info = Inventory::FindItemComponent(r.item);
		
		if (inventoryComponent) {
			inventoryComponent->UpdateSlot(info.equipLocation, { id, r.item, 1, 0 });
		}

		Game::entityManager->SerializeEntity(actor);
	});
}

void Recording::EquipRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("EquipRecord");

	element->SetAttribute("item", item);

	element->SetAttribute("t", m_Timestamp.count());

	parent->InsertEndChild(element);
}

void Recording::EquipRecord::Deserialize(tinyxml2::XMLElement* element) {
	item = element->IntAttribute("item");

	m_Timestamp = std::chrono::milliseconds(element->Int64Attribute("t"));
}

Recording::UnequipRecord::UnequipRecord(LOT item) {
	this->item = item;
}

void Recording::UnequipRecord::Act(Entity* actor) {
	// Calculate the amount of seconds (as a float) since the start of the recording
	float time = m_Timestamp.count() / 1000.0f;
	
	auto r = *this;

	actor->AddCallbackTimer(time, [actor, r] () {
		auto* inventoryComponent = actor->GetComponent<InventoryComponent>();

		const auto& info = Inventory::FindItemComponent(r.item);
		
		if (inventoryComponent) {
			inventoryComponent->RemoveSlot(info.equipLocation);
		}

		Game::entityManager->SerializeEntity(actor);
	});
}

void Recording::UnequipRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("UnequipRecord");

	element->SetAttribute("item", item);

	element->SetAttribute("t", m_Timestamp.count());

	parent->InsertEndChild(element);
}

void Recording::UnequipRecord::Deserialize(tinyxml2::XMLElement* element) {
	item = element->IntAttribute("item");

	m_Timestamp = std::chrono::milliseconds(element->Int64Attribute("t"));
}

void Recording::ClearEquippedRecord::Act(Entity* actor) {
	// Calculate the amount of seconds (as a float) since the start of the recording
	float time = m_Timestamp.count() / 1000.0f;
	
	auto r = *this;

	actor->AddCallbackTimer(time, [actor, r] () {
		auto* inventoryComponent = actor->GetComponent<InventoryComponent>();

		if (inventoryComponent) {
			auto equipped = inventoryComponent->GetEquippedItems();

			for (auto entry : equipped) {
				inventoryComponent->RemoveSlot(entry.first);
			}
		}

		Game::entityManager->SerializeEntity(actor);
	});
}

void Recording::ClearEquippedRecord::Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) {
	auto* element = document.NewElement("ClearEquippedRecord");

	element->SetAttribute("t", m_Timestamp.count());

	parent->InsertEndChild(element);
}

void Recording::ClearEquippedRecord::Deserialize(tinyxml2::XMLElement* element) {
	m_Timestamp = std::chrono::milliseconds(element->Int64Attribute("t"));
}

void Recording::Recorder::SaveToFile(const std::string& filename) {
	tinyxml2::XMLDocument document;

	auto* root = document.NewElement("Recorder");

	for (auto* record : m_MovementRecords) {
		record->Serialize(document, root);
	}

	document.InsertFirstChild(root);

	document.SaveFile(filename.c_str());
}

float Recording::Recorder::GetDuration() const {
	// Return the highest timestamp
	float duration = 0.0f;

	for (auto* record : m_MovementRecords) {
		duration = std::max(duration, record->m_Timestamp.count() / 1000.0f);
	}

	return duration;
}

Recorder* Recording::Recorder::LoadFromFile(const std::string& filename) {
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

		if (name == "MovementRecord") {
			MovementRecord* record = new MovementRecord();
			record->Deserialize(element);
			recorder->m_MovementRecords.push_back(record);
		} else if (name == "SpeakRecord") {
			SpeakRecord* record = new SpeakRecord();
			record->Deserialize(element);
			recorder->m_MovementRecords.push_back(record);
		} else if (name == "AnimationRecord") {
			AnimationRecord* record = new AnimationRecord();
			record->Deserialize(element);
			recorder->m_MovementRecords.push_back(record);
		} else if (name == "EquipRecord") {
			EquipRecord* record = new EquipRecord();
			record->Deserialize(element);
			recorder->m_MovementRecords.push_back(record);
		} else if (name == "UnequipRecord") {
			UnequipRecord* record = new UnequipRecord();
			record->Deserialize(element);
			recorder->m_MovementRecords.push_back(record);
		} else if (name == "ClearEquippedRecord") {
			ClearEquippedRecord* record = new ClearEquippedRecord();
			record->Deserialize(element);
			recorder->m_MovementRecords.push_back(record);
		}
	}

	return recorder;
}

void Recording::Recorder::AddRecording(LWOOBJID actorID, Recorder* recorder) {
	const auto& it = m_Recorders.find(actorID);

	// Delete the old recorder if it exists
	if (it != m_Recorders.end()) {
		delete it->second;
		m_Recorders.erase(it);
	}

	m_Recorders.insert_or_assign(actorID, recorder);
}
