#pragma once
#include "dCommonVars.h"
#include "Entity.h"
#include "GUID.h"
#include "Component.h"
#include "eReplicaComponentType.h"

struct MusicCue {
	std::string name;
	uint32_t result;
	float boredomTime;

	MusicCue(std::string name, float boredomTime = -1.0, uint32_t result = 1){
		this->name = name;
		this->result = result;
		this->boredomTime = boredomTime;
	};

	void Serialize(RakNet::BitStream* outBitStream);
};

struct MusicParameter {
	std::string name;
	float value;

	MusicParameter(std::string name, float value = 0.0){
		this->name = name;
		this->value = value;
	}

	void Serialize(RakNet::BitStream* outBitStream);
};

struct GUIDResults{
	GUID guid;
	uint32_t result;

	GUIDResults(std::string guidString, uint32_t result = 1){
		this->guid = GUID(guidString);
		this->result = result;
	}

	void Serialize(RakNet::BitStream* outBitStream);
};

struct MixerProgram{
	std::string name;
	uint32_t result;

	MixerProgram(std::string name, uint32_t result = 0){
		this->name = name;
		this->result = result;
	}

	void Serialize(RakNet::BitStream* outBitStream);
};


class SoundTriggerComponent : public Component {
public:
	static const eReplicaComponentType ComponentType = eReplicaComponentType::SOUND_TRIGGER;
	explicit SoundTriggerComponent(Entity* parent);
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) override;
	void ActivateMusicCue(const std::string& name, float bordemTime = -1.0);
	void DeactivateMusicCue(const std::string& name);

private:

	std::vector<MusicCue> m_MusicCues = {};
	std::vector<MusicParameter> m_MusicParameters = {};
	std::vector<GUIDResults> m_2DAmbientSounds = {};
	std::vector<GUIDResults> m_3DAmbientSounds = {};
	std::vector<MixerProgram> m_MixerPrograms = {};

	bool m_Dirty = false;
};
