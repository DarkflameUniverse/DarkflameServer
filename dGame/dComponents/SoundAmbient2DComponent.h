#ifndef SOUNDAMBIENT2DCOMPONENT_H
#define SOUNDAMBIENT2DCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * A 2D ambient sound the client plays, as the client's LWOSoundAmbient2DComponent; the server sends nothing for it.
 */
class SoundAmbient2DComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::SOUND_AMBIENT_2D;

	SoundAmbient2DComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // SOUNDAMBIENT2DCOMPONENT_H
