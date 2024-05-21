#ifndef __ITEMCOMPONENT__H__
#define __ITEMCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"

class ItemComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::ITEM;

	ItemComponent(Entity* entity);

	void Serialize(RakNet::BitStream& bitStream, bool isConstruction) override;

	void UpdateDescription(const std::u16string& description);
private:
	std::u16string m_Description;
	bool m_Dirty = false;
	LWOOBJID m_UgcId = LWOOBJID_EMPTY;
	uint32_t m_ModerationStatus = 0;
};

#endif  //!__ITEMCOMPONENT__H__
