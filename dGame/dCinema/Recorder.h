#pragma once

#include "Player.h"
#include "Game.h"
#include "EntityManager.h"
#include "tinyxml2.h"
#include "Play.h"

#include <chrono>

namespace Cinema::Recording
{

class Record
{
public:
	virtual void Act(Entity* actor) = 0;

	virtual void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) = 0;

	virtual void Deserialize(tinyxml2::XMLElement* element) = 0;

	float m_Delay;

	std::string m_Name;
};

class Recorder
{
public:
	Recorder();

	~Recorder();

	void AddRecord(Record* record);

	void Act(Entity* actor, Play* variables = nullptr);

	Entity* ActFor(Entity* actorTemplate, Entity* player, Play* variables = nullptr);

	void StopActingFor(Entity* actor, Entity* actorTemplate, LWOOBJID playerID);

	bool IsRecording() const;

	void SaveToFile(const std::string& filename);

	float GetDuration() const;
	
	static Recorder* LoadFromFile(const std::string& filename);

	static void AddRecording(LWOOBJID actorID, Recorder* recorder);

	static void StartRecording(LWOOBJID actorID);

	static void StopRecording(LWOOBJID actorID);

	static Recorder* GetRecorder(LWOOBJID actorID);

	static void RegisterEffectForActor(LWOOBJID actorID, const int32_t& effectId);

	static void LoadRecords(tinyxml2::XMLElement* root, std::vector<Record*>& records);

	static void ActingDispatch(Entity* actor, const std::vector<Record*>& records, size_t index, Play* variables);

	static void PlayerProximityDispatch(Entity* actor, const std::vector<Record*>& records, size_t index, Play* variables, std::shared_ptr<bool> actionTaken);

private:
	std::vector<Record*> m_Records;

	bool m_IsRecording;

	std::chrono::milliseconds m_StartTime;

	std::chrono::milliseconds m_LastRecordTime;
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
	LOT item = LOT_NULL;

	EquipRecord() = default;

	EquipRecord(LOT item);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};

class UnequipRecord : public Record
{
public:
	LOT item = LOT_NULL;

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

class WaitRecord : public Record
{
public:
	WaitRecord() = default;

	WaitRecord(float delay);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};

class ForkRecord : public Record
{
public:
	std::string variable;
	std::string value;

	std::string precondition;

	std::string success;
	std::string failure;

	ForkRecord() = default;

	ForkRecord(const std::string& variable, const std::string& value, const std::string& success, const std::string& failure);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};

class JumpRecord : public Record
{
public:
	std::string label;

	JumpRecord() = default;

	JumpRecord(const std::string& label);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};

class SetVariableRecord : public Record
{
public:
	std::string variable;
	std::string value;

	SetVariableRecord() = default;

	SetVariableRecord(const std::string& variable, const std::string& value);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};

class BarrierRecord : public Record
{
public:
	std::string signal;

	float timeout = 0.0f;

	std::string timeoutLabel;

	BarrierRecord() = default;

	BarrierRecord(const std::string& signal, float timeout, const std::string& timeoutLabel);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;

	void Deserialize(tinyxml2::XMLElement* element) override;
};

class SignalRecord : public Record
{
public:
	std::string signal;

	SignalRecord() = default;

	SignalRecord(const std::string& signal);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;
	
	void Deserialize(tinyxml2::XMLElement* element) override;
};

class ConcludeRecord : public Record
{
public:
	ConcludeRecord() = default;

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;
	
	void Deserialize(tinyxml2::XMLElement* element) override;
};

class PlayerProximityRecord : public Record
{
public:
	float distance = 0.0f;
	
	float timeout = 0.0f;

	std::string timeoutLabel;

	PlayerProximityRecord() = default;

	PlayerProximityRecord(float distance, float timeout, const std::string& timeoutLabel);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;
	
	void Deserialize(tinyxml2::XMLElement* element) override;
};

class VisibilityRecord : public Record
{
public:
	bool visible = false;

	VisibilityRecord() = default;

	VisibilityRecord(bool visible);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;
	
	void Deserialize(tinyxml2::XMLElement* element) override;
};

class PlayEffectRecord : public Record
{
public:
	std::string effect;

	PlayEffectRecord() = default;

	PlayEffectRecord(const std::string& effect);

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;
	
	void Deserialize(tinyxml2::XMLElement* element) override;
};

class CoroutineRecord : public Record
{
public:
	std::vector<Record*> records;

	CoroutineRecord() = default;

	void Act(Entity* actor) override;

	void Serialize(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent) override;
	
	void Deserialize(tinyxml2::XMLElement* element) override;
};


}