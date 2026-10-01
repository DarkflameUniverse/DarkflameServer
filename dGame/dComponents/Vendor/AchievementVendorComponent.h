#ifndef __ACHIEVEMENTVENDORCOMPONENT__H__
#define __ACHIEVEMENTVENDORCOMPONENT__H__

#include "BaseVendorComponent.h"
#include "eReplicaComponentType.h"
#include <set>
#include <map>

class Entity;

class AchievementVendorComponent final : public BaseVendorComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ACHIEVEMENT_VENDOR;
	AchievementVendorComponent(Entity* parent, const int32_t componentID);

	void LoadDataFromTemplate() override;
	
	void RefreshInventory(bool isCreation = false) override;
	bool SellsItem(Entity* buyer, const LOT lot);
	void Buy(Entity* buyer, LOT lot, uint32_t count);

private:
	std::map<LWOOBJID,std::set<LOT>> m_PlayerPurchasableItems;
};


#endif  //!__ACHIEVEMENTVENDORCOMPONENT__H__
