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
};

struct MusicParemeter {
	std::string name;
	float value;
};

/**
 * Handles specific music triggers like the instruments in Red Block
 * Credits to https://github.com/SimonNitzsche/OpCrux-Server/blob/master/src/Entity/Components/SoundTriggerComponent.hpp
 */
class SoundTriggerComponent : public Component {
public:
	static const eReplicaComponentType ComponentType = eReplicaComponentType::SOUND_TRIGGER;

	explicit SoundTriggerComponent(Entity* parent);
	~SoundTriggerComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Activates a music cue, making it played by any client in range
	 * @param name the name of the music to play
	 */
	void ActivateMusicCue(const std::string& name);

	/**
	 * Deactivates a music cue (if active)
	 * @param name name of the music to deactivate
	 */
	void DeactivateMusicCue(const std::string& name);

private:

	std::vector<MusicCue> m_MusicCues = {};
	std::vector<MusicParemeter> m_MusicParameters = {};
	std::vector<GUID> m_GUID = {};
	std::vector<GUID> m_GUID2 = {};
	std::vector<std::string> m_MixerPrograms = {};

	bool m_Dirty = false;
};
