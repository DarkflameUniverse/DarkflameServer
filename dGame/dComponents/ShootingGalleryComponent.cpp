#include "ShootingGalleryComponent.h"
#include "EntityManager.h"
#include "ScriptedActivityComponent.h"

ShootingGalleryComponent::ShootingGalleryComponent(Entity* parent) : Component(parent) {
}

ShootingGalleryComponent::~ShootingGalleryComponent() = default;

void ShootingGalleryComponent::SetStaticParams(const StaticShootingGalleryParams& params) {
	m_StaticParams = params;
}

void ShootingGalleryComponent::SetDynamicParams(const DynamicShootingGalleryParams& params) {
	m_DynamicParams = params;
	m_Dirty = true;
	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void ShootingGalleryComponent::Serialize(RakNet::BitStream* outBitStream, bool isInitialUpdate, uint32_t& flags) const {
	// Start ScriptedActivityComponent
	outBitStream->Write<bool>(true);
	if (m_CurrentPlayerID == LWOOBJID_EMPTY) {
		outBitStream->Write<uint32_t>(0);
	} else {
		outBitStream->Write<uint32_t>(1);
		for (size_t i = 0; i < 10; i++) {
			outBitStream->Write<float_t>(0.0f);
		}

	}
	// End ScriptedActivityComponent

	if (isInitialUpdate) {
		outBitStream->Write<float_t>(m_StaticParams.cameraPosition.GetX());
		outBitStream->Write<float_t>(m_StaticParams.cameraPosition.GetY());
		outBitStream->Write<float_t>(m_StaticParams.cameraPosition.GetZ());

		outBitStream->Write<float_t>(m_StaticParams.cameraLookatPosition.GetX());
		outBitStream->Write<float_t>(m_StaticParams.cameraLookatPosition.GetY());
		outBitStream->Write<float_t>(m_StaticParams.cameraLookatPosition.GetZ());
	}

	outBitStream->Write<bool>(m_Dirty || isInitialUpdate);
	if (m_Dirty || isInitialUpdate) {
		outBitStream->Write<double_t>(m_DynamicParams.cannonVelocity);
		outBitStream->Write<double_t>(m_DynamicParams.cannonRefireRate);
		outBitStream->Write<double_t>(m_DynamicParams.cannonMinDistance);

		outBitStream->Write<float_t>(m_DynamicParams.cameraBarrelOffset.GetX());
		outBitStream->Write<float_t>(m_DynamicParams.cameraBarrelOffset.GetY());
		outBitStream->Write<float_t>(m_DynamicParams.cameraBarrelOffset.GetZ());

		outBitStream->Write<float_t>(m_DynamicParams.cannonAngle);

		outBitStream->Write<float_t>(m_DynamicParams.facing.GetX());
		outBitStream->Write<float_t>(m_DynamicParams.facing.GetY());
		outBitStream->Write<float_t>(m_DynamicParams.facing.GetZ());

		outBitStream->Write<LWOOBJID>(m_CurrentPlayerID);
		outBitStream->Write<float_t>(m_DynamicParams.cannonTimeout);
		outBitStream->Write<float_t>(m_DynamicParams.cannonFOV);
	}
}

