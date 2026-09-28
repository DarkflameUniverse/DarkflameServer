#pragma once

#include "ePropertyPrivacyOption.h"
#include <chrono>
#include <map>
#include <set>
#include "Entity.h"
#include "Component.h"
#include "eReplicaComponentType.h"
#include "LDFFormat.h"

class Item;

namespace GameMessages {
	struct DownloadPropertyData;
}

/**
 * Main component that handles interactions with a property, generally the plaques you see on properties.
 */
class PropertyManagementComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PROPERTY_MANAGEMENT;
	PropertyManagementComponent(Entity* parent, const int32_t componentID);
	~PropertyManagementComponent() override;
	static PropertyManagementComponent* Instance();

	/**
	 * Whether the player can build on this property: its owner, or with property_bff_build a best friend of the owner
	 * while the owner is building (and, once in build mode, until they leave it)
	 */
	bool CanBuild(const Entity& player) const;

	// A player finished loading into this world: looks up whether they are a best friend of the owner
	void OnPlayerLoaded(Entity& player);

	// A player left this world: ends their building and updates who else can build
	void OnPlayerRemoved(Entity& player);

	// Tells every player whose right to build changed (CanBuild) and ends building for those who lost it
	void UpdateBuildRights();

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
	 * Event triggered when a player who can build starts building. The first one to start (the owner) makes the property
	 * private, pauses the models and sends away the players who can't build.
	 */
	void OnStartBuilding(Entity& builder);

	/**
	 * Event triggered when a builder finished building. When the last one finishes the property is re-applied for
	 * moderation, gets its privacy back and the models run again.
	 */
	void OnFinishBuilding(const Entity& builder);

	/**
	 * Places a model from the builder's inventory on the property
	 * @param builder the player placing the model
	 * @param id the ID of the model item to place
	 * @param position the position to place the model on
	 * @param rotation the rotation to place the model on
	 */
	void UpdateModelPosition(Entity& builder, LWOOBJID id, NiPoint3 position, NiQuaternion rotation);

	/**
	 * Takes a model off the property, into the inventory of the player who placed it
	 * @param builder the player taking the model off
	 * @param id the ID of the model to delete
	 * @param deleteReason the reason of the deletion, e.g. picked up or destroyed (in case of UGC)
	 */
	void DeleteModel(Entity& builder, LWOOBJID id, int deleteReason);

	/**
	 * Spawns a model on the property and records it in the property's models (not yet saved)
	 * @param lot the model object's LOT (14 for a brick built model)
	 * @param modelId the model's id in properties_contents (its UGID)
	 * @param config extra config for the model object
	 * @param placedBy the player who placed it
	 */
	Entity* SpawnModel(LOT lot, LWOOBJID modelId, const NiPoint3& position, const NiQuaternion& rotation, const LwoNameValue& config, LWOOBJID placedBy);

	/**
	 * Places a model item of the builder's on the property, uses the item up and saves the property
	 * @return the placed model's id, or LWOOBJID_EMPTY if it could not be placed
	 */
	LWOOBJID PlaceModelFromItem(const Entity& builder, Item& item, const NiPoint3& position, const NiQuaternion& rotation);

	// The player who placed a model (by its id in properties_contents); the owner for models placed before this was kept
	LWOOBJID GetPlacedBy(LWOOBJID modelId) const;

	// GetModelsOnProperty with every model on the property
	void SendModelsOnProperty() const;

	/**
	 * Updates whether or not this property is approved by a moderator
	 * @param value true if the property should be approved, false otherwise
	 */
	void UpdateApprovedStatus(bool value, const std::string& rejectionReason = "");

	// A moderator decided on this property from the dashboard (already saved): keep this world's copy in step
	void ApplyModeration(bool approved, const std::string& reason);

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

	LWOOBJID GetId() const noexcept { return propertyId; }

	// Reputation visitors just gave (PropertyReputation.h; the database is updated there), so property data shows it
	void AddReputation(uint32_t points) { reputation += points; }
	uint32_t GetReputation() const noexcept { return reputation; }


	void OnChatMessageReceived(const std::string& sMessage) const;

	/**
	 * Live updates (docs/LiveUpdate.md): a new instance of this property is about to load it from the database. Takes
	 * everyone out of build mode, saves, and from then on nobody can build or claim it here and it is never saved again,
	 * so the new instance's saves can't be overwritten by this one.
	 */
	void FreezeForHandOff();

	// The hand-off was called off before anyone went to the new instance: building and saving work again
	void Unfreeze();

	bool IsFrozen() const noexcept { return frozen; }

	// Players in build mode here now
	size_t GetBuilderCount() const noexcept { return builders.size(); }
private:
	// Sends the property's data to one player; a player who can build is told they own it, which is the client's only
	// check before editing (the owner's name stays the owner's)
	void SendPropertyData(const Entity& player, const GameMessages::DownloadPropertyData& message);

	// Whether the player is a best friend of the owner (looked up the first time)
	bool IsBestFriend(LWOOBJID player) const;

	// OnFinishBuilding without updating who can build; false if they weren't building
	bool EndBuilding(const Entity& builder);

	/**
	 * This
	 */
	static PropertyManagementComponent* instance;

	// Handed off to a new instance for a live update (FreezeForHandOff)
	bool frozen = false;

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
	std::map<LWOOBJID /* ObjectID */, LWOOBJID /* SpawnerID */> models = {};

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

	// Whether each player on the property is a best friend of the owner, looked up once per player (again when the
	// owner changes)
	mutable std::map<LWOOBJID, bool> bestFriends;

	// Whether each player on the property was last told they can build (see SendPropertyData)
	std::map<LWOOBJID, bool> sentBuildRights;

	// Players building right now
	std::set<LWOOBJID> builders;

	// Who placed each model, by its id in properties_contents (LWOOBJID_EMPTY: the owner)
	std::map<LWOOBJID, LWOOBJID> placedBy;
};
