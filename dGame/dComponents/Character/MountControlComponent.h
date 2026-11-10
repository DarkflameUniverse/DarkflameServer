#ifndef MOUNTCONTROLCOMPONENT_H
#define MOUNTCONTROLCOMPONENT_H

#include "Component.h"
#include "dCommonVars.h"
#include "eReplicaComponentType.h"

/**
 * Mount control (111): the character's riding of mounts, as the client's LWOMountControlComponent: the client makes it (componentID -1) with every character
 * (LWOCharacterComponent::Factory). It keeps the mount item in the character's save, as live did (<mnt a="item id"/>,
 * 0 when not riding: all 450 live saves looked at were 0).
 */
class MountControlComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MOUNT_CONTROL;

	MountControlComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}

	// LWOMountControlComponent::LoadFromSaveData (0x00bf99c0): <mnt a>, the mount item of the save
	void LoadFromSaveData(const tinyxml2::XMLDocument& doc) override;

	// Writes <mnt a>: the item of the mount being ridden (PossessorComponent's), 0 when on foot
	void WriteSaveData(tinyxml2::XMLDocument& doc) override;

	// The mount item the save named. Riding does not go on over a world change, so nothing is mounted from it.
	LWOOBJID GetSavedMountItemID() const { return m_SavedMountItemID; }

private:
	LWOOBJID m_SavedMountItemID = LWOOBJID_EMPTY;
};

#endif // MOUNTCONTROLCOMPONENT_H
