#include "MountControlComponent.h"

#include "Entity.h"
#include "PossessorComponent.h"

#include "tinyxml2.h"

void MountControlComponent::LoadFromSaveData(const tinyxml2::XMLDocument& doc) {
	const auto* const obj = doc.FirstChildElement("obj");
	const auto* const mount = obj ? obj->FirstChildElement("mnt") : nullptr;
	if (mount) mount->QueryInt64Attribute("a", &m_SavedMountItemID);
}

void MountControlComponent::WriteSaveData(tinyxml2::XMLDocument& doc) {
	auto* const obj = doc.FirstChildElement("obj");
	if (!obj) return;

	auto* mount = obj->FirstChildElement("mnt");
	if (!mount) mount = obj->InsertNewChildElement("mnt");

	const auto* const possessor = m_Parent->GetComponent<PossessorComponent>();
	mount->SetAttribute("a", possessor ? possessor->GetMountItemID() : LWOOBJID_EMPTY);
}
