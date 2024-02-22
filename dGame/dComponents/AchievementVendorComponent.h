#ifndef __ACHIEVEMENTVENDORCOMPONENT__H__
#define __ACHIEVEMENTVENDORCOMPONENT__H__

#include "VendorComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class AchievementVendorComponent final : public VendorComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ACHIEVEMENT_VENDOR;
	AchievementVendorComponent(Entity* parent) : VendorComponent(parent) {};
	bool SellsItem(const LOT item) const override;
private:

};


#endif  //!__ACHIEVEMENTVENDORCOMPONENT__H__
