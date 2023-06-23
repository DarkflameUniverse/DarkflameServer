#ifndef __DONATIONVENDORCOMPONENT__H__
#define __DONATIONVENDORCOMPONENT__H__

#include "VendorComponent.h"
#include "eReplicaComponentType.h"

class Entity;

class DonationVendorComponent : public VendorComponent {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::DONATION_VENDOR;
	DonationVendorComponent(Entity* parent);

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
	void LoadConfigData() override;

	void SetPercentComplete(float percentComplete){
		if (m_PercentComplete == percentComplete) return;
		m_PercentComplete = percentComplete;
		m_DirtyDonationVendor = true;
	}

	void SetTotalDonated(float totalDonated){
		if (m_TotalDonated == totalDonated) return;
		m_TotalDonated = totalDonated;
		m_DirtyDonationVendor = true;
	}

	void SetTotalRemaining(float totalRemaining){
		if (m_TotalRemaining == totalRemaining) return;
		m_TotalRemaining = totalRemaining;
		m_DirtyDonationVendor = true;
	}

private:
	bool m_DirtyDonationVendor = false;
	float m_PercentComplete = 0.0;
	int32_t m_TotalDonated = 0;
	int32_t m_TotalRemaining = 0;
	uint32_t m_ActivityId = 0;
};


#endif  //!__DONATIONVENDORCOMPONENT__H__
