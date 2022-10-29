#pragma once
#include "dCommonVars.h"
#include "NiPoint3.h"
#include "Entity.h"
#include "Component.h"

/**
 * Parameters for the shooting gallery that change during playtime
 */
struct DynamicShootingGalleryParams {

	/**
	 * The distance from the camera to the barrel
	 */
	Vector3 cameraBarrelOffset;

	/**
	 * The area the barrel is looking at
	 */
	Vector3 facing;

	/**
	 * The velocity of the cannonballs
	 */
	double_t cannonVelocity;

	/**
	 * The max firerate of the cannon
	 */
	double_t cannonRefireRate;

	/**
	 * The min distance the cannonballs traverse
	 */
	double_t cannonMinDistance;

	/**
	 * The angle at which the cannon is shooting
	 */
	float_t cannonAngle;

	/**
	 * The timeout between cannon shots
	 */
	float_t cannonTimeout;

	/**
	 * The FOV while in the canon
	 */
	float_t cannonFOV;
};

/**
 * Parameters for the shooting gallery that don't change over time
 */
struct StaticShootingGalleryParams {

	/**
	 * The position of the camera
	 */
	Vector3 cameraPosition;

	/**
	 * The position that the camera is looking at
	 */
	Vector3 cameraLookatPosition;
};

/**
 * A very ancient component that was used to guide shooting galleries, it's still kind of used but a lot of logic is
 * also in the related scripts.
 */
class ShootingGalleryComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_SHOOTING_GALLERY;

	explicit ShootingGalleryComponent(Entity* parent);
	~ShootingGalleryComponent();
	void Serialize(RakNet::BitStream* outBitStream, bool isInitialUpdate, uint32_t& flags) const;

	/**
	 * Returns the static params for the shooting gallery
	 * @return the static params for the shooting gallery
	 */
	const StaticShootingGalleryParams& GetStaticParams() const { return m_StaticParams; };

	/**
	 * Sets the static parameters for the shooting gallery, see `StaticShootingGalleryParams`
	 * @param params the params to set
	 */
	void SetStaticParams(const StaticShootingGalleryParams& params);

	/**
	 * Returns the dynamic params for the shooting gallery
	 * @return the dynamic params for the shooting gallery
	 */
	const DynamicShootingGalleryParams& GetDynamicParams() const { return m_DynamicParams; };

	/**
	 * Sets the mutable params for the shooting gallery, see `DynamicShootingGalleryParams`
	 * @param params the params to set
	 */
	void SetDynamicParams(const DynamicShootingGalleryParams& params);

	/**
	 * Sets the entity that's currently playing the shooting gallery
	 * @param playerID the entity to set
	 */
	void SetCurrentPlayerID(LWOOBJID playerID) { m_CurrentPlayerID = playerID; m_Dirty = true; };

	/**
	 * Returns the player that's currently playing the shooting gallery
	 * @return the player that's currently playing the shooting gallery
	 */
	LWOOBJID GetCurrentPlayerID() const { return m_CurrentPlayerID; };
private:

	/**
	 * The player that's currently playing the shooting gallery
	 */
	LWOOBJID m_CurrentPlayerID = LWOOBJID_EMPTY;

	/**
	 * The static parameters for the shooting gallery, see `StaticShootingGalleryParams`
	 */
	StaticShootingGalleryParams m_StaticParams{};

	/**
	 * The dynamic params for the shooting gallery, see `DynamicShootingGalleryParams`
	 */
	DynamicShootingGalleryParams m_DynamicParams{};

	/**
	 * Whether or not the component should be serialized
	 */
	bool m_Dirty = false;
};
