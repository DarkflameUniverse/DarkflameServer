#pragma once

#include <chrono>
#include "Entity.h"
#include "Component.h"

/**
 * Information regarding which players may visit this property
 */
enum class PropertyPrivacyOption
{
	/**
	 * Default, only you can visit your property
	 */
	Private = 0,

	/**
	 * Your friends can visit your property
	 */
	 Friends = 1,

	 /**
	  * Requires Mythran approval, everyone can visit your property
	  */
	  Public = 2
};

/**
 * Main component that handles interactions with a property, generally the plaques you see on properties.
 */
class PropertyManagementComponent : public Component
{
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_PROPERTY_MANAGEMENT;
	PropertyManagementComponent(Entity* parent);
	static PropertyManagementComponent* Instance();

	/**
	 * Event handler for when an entity requests information about this property, will send back whether it's owned, etc.
	 * @param originator the entity that triggered the event
	 * @param sysAddr the address to send game message responses to
	 * @param author optional explicit ID for the property, if not set defaults to the originator
	 */
	void OnQueryPropertyData(Entity* originator, const SystemAddress& sysAddr, LWOOBJID author = LWOOBJID_EMPTY);

	/**
	 * Handles an OnUse event, telling the client who owns this property, etc.
	 * @param originator the entity that triggered the event
	 */
	void OnUse(Entity* originator) override;

	/**
	 * Sets the owner of this property
	 * @param value the owner to set
	 */
	void SetOwnerId(LWOOBJID value);

	/**
	 * Returns the ID of the owner of this property
	 * @return the ID of the owner of this property
	 */
	LWOOBJID GetOwnerId() const;

	/**
	 * Returns the owner of this property
	 * @return the owner of this property
	 */
	Entity* GetOwner() const;

	/**
	 * sets the owner of this property
	 * @param value the owner to set
	 */
	void SetOwner(Entity* value);

	/**
	 * Returns the paths that this property has
	 * @return the paths that this property has
	 */
	std::vector<NiPoint3> GetPaths() const;

	/**
	 * Returns the privacy options for this property
	 * @return the privacy options for this property
	 */
	PropertyPrivacyOption GetPrivacyOption() const;

	/**
	 * Updates the privacy option for this property
	 * @param value the privacy option to set
	 */
	void SetPrivacyOption(PropertyPrivacyOption value);

	/**
	 * Updates information of this property, saving it to the database
	 * @param name the name to set for the property
	 * @param description the description to set for the property
	 */
	void UpdatePropertyDetails(std::string name, std::string description);

	/**
	 * Makes this property owned by the passed player ID, storing it in the database
	 * @param playerId the ID of the entity that claimed the property
	 *
	 * @return If the claim is successful return true.
	 */
	bool Claim(LWOOBJID playerId);

	/**
	 * Event triggered when the owner of the property starts building, will kick other entities out
	 */
	void OnStartBuilding();

	/**
	 * Event triggered when the owner of the property finished building, will re-apply this property for moderation
	 * request.
	 */
	void OnFinishBuilding();

	/**
	 * Updates the position of a model on the property
	 * @param id the ID of the model to reposition
	 * @param position the position to place the model on
	 * @param rotation the rotation to place the model on
	 */
	void UpdateModelPosition(LWOOBJID id, NiPoint3 position, NiQuaternion rotation);

	/**
	 * Deletes a model for a property
	 * @param id the ID of the model to delete
	 * @param deleteReason the reason of the deletion, e.g. picked up or destroyed (in case of UGC)
	 */
	void DeleteModel(LWOOBJID id, int deleteReason);

	/**
	 * Updates whether or not this property is approved by a moderator
	 * @param value true if the property should be approved, false otherwise
	 */
	void UpdateApprovedStatus(bool value);

	/**
	 * Loads all the models on this property from the database
	 */
	void Load();

	/**
	 * Saves all the models from this property to the database
	 */
	void Save();

	/**
	 * Adds a model to the cache of models
	 * @param modelId the ID of the model
	 * @param spawnerId the ID of the object that spawned the model
	 */
	void AddModel(LWOOBJID modelId, LWOOBJID spawnerId);

	/**
	 * Returns all the models on this property, indexed by property ID, containing their spawn objects
	 * @return all the models on this proeprty
	 */
	const std::map<LWOOBJID, LWOOBJID>& GetModels() const;

	LWOCLONEID GetCloneId() { return clone_Id; };

private:
	/**
	 * This
	 */
	static PropertyManagementComponent* instance;

	/**
	 * The ID of the owner of this property
	 */
	LWOOBJID owner = LWOOBJID_EMPTY;

	/**
	 * The LOT of this console
	 */
	uint32_t templateId = 0;

	/**
	 * The unique ID for this property, if it's owned
	 */
	LWOOBJID propertyId = LWOOBJID_EMPTY;

	/**
	 * The time since this property was claimed
	 */
	uint64_t claimedTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	/**
	 * The models that are placed on this property
	 */
	std::map<LWOOBJID, LWOOBJID> models = {};

	/**
	 * The name of this property
	 */
	std::string propertyName = "";

	/**
	 * The clone ID of this property
	 */
	LWOCLONEID clone_Id = 0;

	/**
	 * Whether a moderator was requested
	 */
	bool moderatorRequested = false;

	/**
	 * The rejection reason for the property
	 */
	std::string rejectionReason = "";

	/**
	 * The description of this property
	 */
	std::string propertyDescription = "";

	/**
	 * The reputation of this property
	 */
	uint32_t reputation = 0;

	/**
	 * The last time this property was updated
	 */
	uint32_t LastUpdatedTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	/**
	 * Determines which players may visit this property
	 */
	PropertyPrivacyOption privacyOption = PropertyPrivacyOption::Private;

	/**
	 * The privacy setting before it was changed, saved to set back after a player finishes building
	 */
	PropertyPrivacyOption originalPrivacyOption = PropertyPrivacyOption::Private;
};
