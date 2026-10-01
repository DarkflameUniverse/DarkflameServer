#ifndef __RACINGSOUNDTRIGGERCOMPONENT__H__
#define __RACINGSOUNDTRIGGERCOMPONENT__H__

#include "SoundTriggerComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class RacingSoundTriggerComponent : public SoundTriggerComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::RACING_SOUND_TRIGGER;
	RacingSoundTriggerComponent(Entity* parent, const int32_t componentID) : SoundTriggerComponent(parent, componentID){};
};

#endif  //!__RACINGSOUNDTRIGGERCOMPONENT__H__
