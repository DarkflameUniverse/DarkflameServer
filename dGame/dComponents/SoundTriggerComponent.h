#pragma once
#include "dCommonVars.h"
#include "Entity.h"
#include "GUID.h"
#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * Music that should be played by the client
 */
struct MusicCue {
	std::string name;
	uint32_t result;
	float boredomTime;

	MusicCue(std::string name, float boredomTime){
		name = name;
		result = 1;
		boredomTime = boredomTime;
	};

	void Serialize(RakNet::BitStream* outBitStream){
		outBitStream->Write<uint8_t>(name.size());
		outBitStream->Write(name.c_str(), name.size());
		outBitStream->Write(result);
		outBitStream->Write(boredomTime);
	}

};

struct MusicParemeter {
	std::string name;
	float value;

	MusicParemeter(std::string name, float value){
		name = name;
		value = value;
	}

	void Serialize(RakNet::BitStream* outBitStream){
		outBitStream->Write<uint8_t>(name.size());
		outBitStream->Write(name.c_str(), name.size());
		outBitStream->Write(value);
	}

};

struct GUIDResults{
	GUID guid;
	uint32_t result;

	GUIDResults(std::string guidString){
		guid = GUID(guidString);
		result = 1;
	}

	void Serialize(RakNet::BitStream* outBitStream){
		guid.Serialize(outBitStream);
		outBitStream->Write(result);
	}
};

struct MixerProgram{
	std::string name;
	uint32_t result;

	MixerProgram(std::string name){
		name = name;
		result = 1;
	}

	void Serialize(RakNet::BitStream* outBitStream){
		outBitStream->Write<uint8_t>(name.size());
		outBitStream->Write(name.c_str(), name.size());
		outBitStream->Write(result);
	}
};


class SoundTriggerComponent : public Component {
public:
	static const eReplicaComponentType ComponentType = eReplicaComponentType::SOUND_TRIGGER;

	explicit SoundTriggerComponent(Entity* parent);
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
	void ActivateMusicCue(const std::string& name);
	void DeactivateMusicCue(const std::string& name);

private:

	std::vector<MusicCue> m_MusicCues = {};
	std::vector<MusicParemeter> m_MusicParameters = {};
	std::vector<GUIDResults> m_2DAmbientSounds = {};
	std::vector<GUIDResults> m_3DAmbientSounds = {};
	std::vector<MixerProgram> m_MixerPrograms = {};

	bool m_Dirty = false;
};
