#ifndef RACINGSOUNDTRIGGERCOMPONENT_H
#define RACINGSOUNDTRIGGERCOMPONENT_H

#include "SoundTriggerComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class RacingSoundTriggerComponent : public SoundTriggerComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::RACING_SOUND_TRIGGER;
	RacingSoundTriggerComponent(Entity* parent) : SoundTriggerComponent(parent){};
};

#endif  //!RACINGSOUNDTRIGGERCOMPONENT_H
