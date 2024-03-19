#ifndef __DONATIONVENDORCOMPONENT__H__
#define __DONATIONVENDORCOMPONENT__H__

#include "VendorComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class DonationVendorComponent final : public VendorComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::DONATION_VENDOR;
	DonationVendorComponent(Entity* parent);
	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;
	uint32_t GetActivityID() {return m_ActivityId;};
	void SubmitDonation(uint32_t count);

private:
	bool m_DirtyDonationVendor = false;
	float m_PercentComplete = 0.0;
	int32_t m_TotalDonated = 0;
	int32_t m_TotalRemaining = 0;
	uint32_t m_ActivityId = 0;
	int32_t m_Goal = 0;
};


#endif  //!__DONATIONVENDORCOMPONENT__H__
