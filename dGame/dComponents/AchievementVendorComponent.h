#ifndef __ACHIEVEMENTVENDORCOMPONENT__H__
#define __ACHIEVEMENTVENDORCOMPONENT__H__

#include "VendorComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class AchievementVendorComponent final : public VendorComponent {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::ACHIEVEMENT_VENDOR;
	AchievementVendorComponent(Entity* parent);
};


#endif  //!__ACHIEVEMENTVENDORCOMPONENT__H__
