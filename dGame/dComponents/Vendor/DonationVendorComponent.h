#ifndef __DONATIONVENDORCOMPONENT__H__
#define __DONATIONVENDORCOMPONENT__H__

#include "BaseVendorComponent.h"
#include "eReplicaComponentType.h"

struct ObjectLoadState;

class Entity;

class DonationVendorComponent final : public BaseVendorComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::DONATION_VENDOR;

	// The client's allocator for this type (ObjectLoader2::DoObjectComponentLoad calls it in load order)
	static void Allocator(Entity& entity, ObjectLoadState& state);
	DonationVendorComponent(Entity* parent, const int32_t componentID);

	void LoadConfigData() override;
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
