#ifndef SOUNDAMBIENT3DCOMPONENT_H
#define SOUNDAMBIENT3DCOMPONENT_H

#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * A 3D ambient sound the client plays, as the client's LWOSoundAmbient3DComponent; the server sends nothing for it.
 */
class SoundAmbient3DComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::SOUND_AMBIENT_3D;

	SoundAmbient3DComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}
};

#endif // SOUNDAMBIENT3DCOMPONENT_H
