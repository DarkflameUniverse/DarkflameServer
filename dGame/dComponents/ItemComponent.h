#ifndef __ITEMCOMPONENT__H__
#define __ITEMCOMPONENT__H__
#pragma once

#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Component.h"
#include "eReplicaComponentType.h"

class Entity;
enum class eUgcModerationStatus : uint32_t;

class ItemComponent : public Component {
public:
	static const eReplicaComponentType ComponentType = eReplicaComponentType::ITEM;

	ItemComponent(Entity* parent);

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	void SetUgId(LWOOBJID id) { m_UgId = id; m_DirtyItemInfo = true; };
	LWOOBJID GetUgId() { return m_UgId; };

	void SetUgModerationStatus(eUgcModerationStatus status) { m_UgModerationStatus = status; m_DirtyItemInfo = true; };
	eUgcModerationStatus GetUgModerationStatus() { return m_UgModerationStatus; };

	void SetUgDescription(std::u16string description) { m_UgDescription = description; m_DirtyItemInfo = true; };
	std::u16string  GetUgDescription() { return m_UgDescription;};

private:

	/**
	 * If we have change the item info
	 */
	bool m_DirtyItemInfo;

	/**
	 * The ID of the user that made the model
	 */
	LWOOBJID m_UgId;

	/**
	 * Whether or not the description of this item is approved.
	 */
	eUgcModerationStatus m_UgModerationStatus;

	/**
	 * The user generated description
	 */
	std::u16string m_UgDescription;
};

#endif  //!__ITEMCOMPONENT__H__
