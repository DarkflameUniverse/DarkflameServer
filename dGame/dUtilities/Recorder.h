#pragma once

#include "Player.h"
#include "Game.h"
#include "EntityManager.h"
#include "tinyxml2.h"

#include <chrono>

namespace Recording
{

class Record
{
public:
	virtual void Act(Entity* actor) = 0;

	virtual void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) = 0;

	virtual void Deserialize(tinyxml2::XMLElement* element) = 0;

	std::chrono::milliseconds m_Timestamp;
};


class MovementRecord : public Record
{
public:
	NiPoint3 position;
	NiQuaternion rotation;
	NiPoint3 velocity;
	NiPoint3 angularVelocity;
	bool onGround;
	bool dirtyVelocity;
	bool dirtyAngularVelocity;

	MovementRecord() = default;

	MovementRecord(
		const NiPoint3& position,
		const NiQuaternion& rotation,
		const NiPoint3& velocity,
		const NiPoint3& angularVelocity,
		bool onGround, bool dirtyVelocity, bool dirtyAngularVelocity
	);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};

class SpeakRecord : public Record
{
public:
	std::string text;

	SpeakRecord() = default;

	SpeakRecord(const std::string& text);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};

class AnimationRecord : public Record
{
public:
	std::string animation;

	AnimationRecord() = default;

	AnimationRecord(const std::string& animation);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};

class EquipRecord : public Record
{
public:
	LOT item;

	EquipRecord() = default;

	EquipRecord(LOT item);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};

class UnequipRecord : public Record
{
public:
	LOT item;

	UnequipRecord() = default;

	UnequipRecord(LOT item);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};

class ClearEquippedRecord : public Record
{
public:
	ClearEquippedRecord() = default;

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};


class Recorder
{
public:
	Recorder();

	~Recorder();

	void AddRecord(Record* record);

	void Act(Entity* actor);

	Entity* ActFor(Entity* actorTemplate, Entity* player);

	void StopActingFor(Entity* actor, Entity* actorTemplate, LWOOBJID playerID);

	bool IsRecording() const;

	void SaveToFile(const std::string& filename);

	float GetDuration() const;

	static Recorder* LoadFromFile(const std::string& filename);

	static void AddRecording(LWOOBJID actorID, Recorder* recorder);

	static void StartRecording(LWOOBJID actorID);

	static void StopRecording(LWOOBJID actorID);

	static Recorder* GetRecorder(LWOOBJID actorID);

private:
	std::vector<Record*> m_MovementRecords;

	bool m_IsRecording;

	std::chrono::milliseconds m_StartTime;
};

}