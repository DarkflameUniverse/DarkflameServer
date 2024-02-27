#pragma once
#ifndef VENDORCOMPONENT_H
#define VENDORCOMPONENT_H

#include "CDClientManager.h"
#include "Component.h"
#include "Entity.h"
#include "GameMessages.h"
#include "RakNetTypes.h"
#include "eReplicaComponentType.h"

struct SoldItem {
	SoldItem(const LOT lot, const int32_t sortPriority) {
		this->lot = lot;
		this->sortPriority = sortPriority;
	};
	LOT lot = 0;
	int32_t sortPriority = 0;
};

class VendorComponent : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::VENDOR;
	VendorComponent(Entity* parent);

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

	void OnUse(Entity* originator) override;
	void RefreshInventory(bool isCreation = false);
	void SetupConstants();
	bool SellsItem(const LOT item) const;
	float GetBuyScalar() const { return m_BuyScalar; }
	float GetSellScalar() const { return m_SellScalar; }
	void SetBuyScalar(const float value) { m_BuyScalar = value; }
	void SetSellScalar(const float value) { m_SellScalar = value; }
	const std::vector<SoldItem>& GetInventory() { return m_Inventory; }

	void SetHasMultiCostItems(const bool hasMultiCostItems) {
		if (m_HasMultiCostItems == hasMultiCostItems) return;
		m_HasMultiCostItems = hasMultiCostItems;
		m_DirtyVendor = true;
	}

	void SetHasStandardCostItems(const bool hasStandardCostItems) {
		if (m_HasStandardCostItems == hasStandardCostItems) return;
		m_HasStandardCostItems = hasStandardCostItems;
		m_DirtyVendor = true;
	}

	void Buy(Entity* buyer, LOT lot, uint32_t count);

private:
	void SetupMaxCustomVendor();
	void HandleMrReeCameras();
	float m_BuyScalar = 0.0f;
	float m_SellScalar = 0.0f;
	float m_RefreshTimeSeconds = 0.0f;
	uint32_t m_LootMatrixID = 0;
	std::vector<SoldItem> m_Inventory;
	bool m_DirtyVendor = false;
	bool m_HasStandardCostItems = false;
	bool m_HasMultiCostItems = false;
};

#endif // VENDORCOMPONENT_H
