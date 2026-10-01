#ifndef SOUNDREPEATERCOMPONENT_H
#define SOUNDREPEATERCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * A repeating sound the client plays (SoundRepeaterComponent row), as the client's LWOSoundRepeaterComponent; the server sends nothing for it.
 */
class SoundRepeaterComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::SOUND_REPEATER;

	SoundRepeaterComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // SOUNDREPEATERCOMPONENT_H
