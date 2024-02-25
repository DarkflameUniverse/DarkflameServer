#ifndef __ACHIEVEMENTVENDORCOMPONENT__H__
#define __ACHIEVEMENTVENDORCOMPONENT__H__

#include "VendorComponent.h"
#include "eReplicaComponentType.h"
#include <set>
#include <map>

class Entity;

class AchievementVendorComponent final : public VendorComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ACHIEVEMENT_VENDOR;
	AchievementVendorComponent(Entity* parent) : VendorComponent(parent) {};
	bool SellsItem(Entity* buyer, const LOT lot);
	void Buy(Entity* buyer, LOT lot, uint32_t count);

private:
	std::map<LWOOBJID,std::set<LOT>> m_PlayerPurchasableItems;
};


#endif  //!__ACHIEVEMENTVENDORCOMPONENT__H__
