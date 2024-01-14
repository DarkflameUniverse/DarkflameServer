#ifndef __RACINGSOUNDTRIGGERCOMPONENT__H__
#define __RACINGSOUNDTRIGGERCOMPONENT__H__

#include "SoundTriggerComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class RacingSoundTriggerComponent : public SoundTriggerComponent {
public:
	constexpr static const eReplicaComponentType ComponentType = eReplicaComponentType::RACING_SOUND_TRIGGER;
	RacingSoundTriggerComponent(const LWOOBJID& parentEntityId) : SoundTriggerComponent(parentEntityId) {};
};

#endif  //!__RACINGSOUNDTRIGGERCOMPONENT__H__
