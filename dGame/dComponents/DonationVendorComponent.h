#ifndef __DONATIONVENDORCOMPONENT__H__
#define __DONATIONVENDORCOMPONENT__H__

#include "VendorComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class DonationVendorComponent final : public VendorComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::DONATION_VENDOR;
	DonationVendorComponent(Entity* parent, const int32_t componentID);
	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;
	uint32_t GetActivityID() {return m_ActivityId;};
	void SubmitDonation(uint32_t count);

	// AddDonationItem from the player at sysAddr: moves count of the item into their donation inventory.
	void AddDonationItem(const SystemAddress& sysAddr, LWOOBJID itemObjID, uint32_t count);

	// ConfirmDonationOnPlayer: the player donates everything in their donation inventory to this vendor.
	void ConfirmDonation(Entity& player);

private:
	bool m_DirtyDonationVendor = false;
	float m_PercentComplete = 0.0;
	int32_t m_TotalDonated = 0;
	int32_t m_TotalRemaining = 0;
	uint32_t m_ActivityId = 0;
	int32_t m_Goal = 0;
};


#endif  //!__DONATIONVENDORCOMPONENT__H__
