#include "RollerComponent.h"

#include "Entity.h"
#include "GeneralUtils.h"

namespace {
	// A level point: the three numbers with character 31 between them
	NiPoint3 LevelPoint(const Entity& entity, const std::u16string& key) {
		const auto parts = GeneralUtils::SplitString(entity.GetVarAsString(key), '\x1f');
		return GeneralUtils::TryParse<NiPoint3>(parts).value_or(NiPoint3Constant::ZERO);
	}
}

void RollerComponent::LoadConfigData() {
	m_CameraPosition = LevelPoint(*m_Parent, u"activity_camera_pos");
	m_CameraLookAt = LevelPoint(*m_Parent, u"activity_camera_lookat_pos");
}
