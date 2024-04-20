#ifndef PETCOMPONENT_H
#define PETCOMPONENT_H

#include "Entity.h"
#include "Flag.h"
#include "MovementAIComponent.h"
#include "Component.h"
#include "Preconditions.h"
#include "ePetAbilityType.h"
#include "eReplicaComponentType.h"
#include "ePetAbilityType.h"
#include "CDPetComponentTable.h"
#include "CDClientManager.h"

#include <optional>

/*
* The current state of the pet AI
*/
enum class PetAiState : uint8_t {
	idle = 0,   	// Doing nothing
	spawn,			// Spawning into the world
	follow,			// Begin following
	goToObj,		// Go to object
	interact,		// Interact with an object
	despawn 		// Despawning from world
};

/*
* The type of object the pet is interacting with
*/
enum class PetInteractType : uint8_t {
	none,		// Not interacting
	treasure,	// Treasure dig
	bouncer		// Bouncer switch
};

/**
 * The flags governing the status of the pet: Governs the icon above their head and the interactions available
*/
enum class PetFlag : uint32_t {
	NONE,
	IDLE,				//0x01 - Seems to be "idle," which the game doesn't differentiate from "follow"
	UNKNOWN2,			//0x02,
	UNKNOWN4,			//0x04 - FOLLOWING(?)
	BEING_TAMED, 		//0x10,
	NOT_WAITING, 		//0x20,
	IMMOBILE,			//0x40 - Seems to be the "stop moving" flag - called when taming begins and stays active until a name is submitted
	SPAWNING,			//0x80
	ON_SWITCH, 			//0x100
	UNKNOWN1024 = 10,	//0x400
	TAMEABLE = 26		//0x4000000
};

/**
 * The pet emote animation ids that can used in PetComponent::Command()
*/
enum class PetEmote : int32_t {
	ActivateSwitch = 201,
	DigTreasure,
	Bounce
};

/**
 * Represents an entity that is a pet. This pet can be tamed and consequently follows the tamer around, allowing it
 * to dig for treasure and activate pet bouncers.
 */
class PetComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::PET;

	/**
	 * PetComponent constructor
	 * @param parentEntity The parent entity
	 * @param componentId The component id
	*/
	explicit PetComponent(Entity* parentEntity, uint32_t componentId);

	~PetComponent() override = default;

	/**
	 * Serializes the pet
	 * @param outBitStream The output bitstream
	 * @param bIsInitialUpdate Boolean value of whether this is the initial update
	*/
	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

	/**
	 * Sets the AI state of the pet
	 * @param newState New pet AI state
	*/
	void SetPetAiState(const PetAiState newState) noexcept {
		m_State = newState;
	};

	/**
	 * Gets the AI state of the pet
	*/
	[[nodiscard]]
	PetAiState GetPetAiState() const noexcept {
		return m_State;
	}

	/**
	 * Governs the pet update loop
	 * @param deltaTime Time elapsed since last update
	*/
	void Update(float deltaTime) override;

	/**
	 * Handles an OnUse event from another entity, initializing the pet taming minigame if this pet is untamed.
	 * @param originator the entity that triggered the event
	 */
	void OnUse(Entity* originator) override;

	/**
	 * Attempts to complete the pet minigame by passing a list of bricks to build the minigame model.
	 * @param bricks the bricks to try to complete the minigame with
	 * @param clientFailed unused
	 */
	void TryBuild(uint32_t numBricks, bool clientFailed);

	/**
	 * Handles a notification from the client regarding the completion of the pet minigame, adding the pet to their
	 * inventory.
	 * @param position the position to spawn the completed model at
	 */
	void NotifyTamingBuildSuccess(NiPoint3 position);

	/**
	 * Handles the notification of the client to set the name of the pet (indicating that minigame was completed
	 * successfully).
	 * @param name the name of the pet to set
	 */
	void RequestSetPetName(const std::u16string& name);

	/**
	 * Handles a notification of the client that the taming entity is leaving the minigame, either voluntary or because
	 * time ran out.
	 * @param voluntaryExit whether the client voluntarily exited the minigame
	 */
	void ClientExitTamingMinigame(bool voluntaryExit);

	/**
	 * Starts the internal timer for the build limit for building the minigame model
	 */
	void StartTimer();

	/**
	 * Notifies the client that they failed the minigame because time ran out
	 */
	void ClientFailTamingMinigame();

	/**
	 * Makes the pet wander around
	 */
	void Wander();

	/**
	 * Called when the pet is first spawned
	*/
	void OnSpawn();

	/**
	 * Continues a step in the follow state, making sure that the entity is around its start position
	*/
	void OnFollow(const float deltaTime);

	/**
	 * Continues a step in the interact state, handling the pet's interaction with an entity
	*/
	void OnInteract();

	/**
	 * Start a pet interaction with an object at a given position
	*/
	void StartInteract(const NiPoint3 position, const PetInteractType interactionType, const LWOOBJID interactID);

	/**
	 * Stop a pet interaction with an object
	 * @param bDontSerialize optional parameter, set to true to not serialize afterwards
	*/
	void StopInteract(bool bDontSerialize = false);

	/**
	 * Spawns a pet from an item in the inventory of an owner
	 * @param item the item to create the pet from
	 * @param registerPet notifies the client that the pet was spawned, not necessary if this pet is being tamed
	 */
	void Activate(Item* item, bool registerPet = true, bool fromTaming = false);

	/**
	 * Despawns the pet
	 */
	void Deactivate(eHelpType msg = eHelpType::NONE);

	/**
	 * Removes the pet from the inventory
	 */
	void Release();

	/**
	 * Commands the pet to do an action, actions are still a relative mystery, next to playing emotes
	 * @param position a position to execute the command at, currently unused
	 * @param source the source object that triggered the command
	 * @param commandType the type of the command (see function body for types)
	 * @param typeId extra information about the command, e.g. the emote to play
	 * @param overrideObey unused
	 */
	void Command(const NiPoint3& position, const LWOOBJID source, const int32_t commandType, const int32_t typeId, const bool overrideObey);

	/**
	 * Returns the ID of the owner of this pet (if any)
	 * @return the ID of the owner of this pet
	 */
	[[nodiscard]]
	LWOOBJID GetOwnerId() const noexcept {
		return m_Owner;
	};

	/**
	 * Returns the entity that owns this pet (if any)
	 * @return the entity that owns this pet
	 */
	[[nodiscard]]
	Entity* GetOwner() const;

	/**
	 * Returns the ID that is stored in the database with regards to this pet, only set for pets that are tamed
	 * @return the ID that is stored in the database with regards to this pet
	 */
	[[nodiscard]]
	LWOOBJID GetDatabaseId() const noexcept {
		return m_DatabaseId;
	}

	/**
	 * Returns the ID that this pet was spawned from, only set for tamed pets
	 * @return the ID that this pet was spawned from
	 */
	[[nodiscard]]
	LWOOBJID GetItemId() const noexcept {
		return m_ItemId;
	}

	/**
	 * Sets preconditions for the pet that need  to be met before it can be tamed
	 * @param conditions the preconditions to set
	 */
	void SetPreconditions(const std::string& preconditions) {
		m_Preconditions = PreconditionExpression(preconditions);
	}

	/**
	 * Sets if the pet is ready to interact with an object
	 * @param isReady whether the pet is ready to interact (true) or not (false)
	 */
	void SetIsReadyToInteract(const bool isReady) {
		m_ReadyToInteract = isReady;
	};

	/**
	 * @return is pet ready to interact with an object
	 */
	[[nodiscard]]
	bool IsReadyToInteract() const noexcept {
		return m_ReadyToInteract;
	}

	/**
	 * Sets if the pet is currently handling an interaction with an object
	 * @param isHandlingInteraction whether the pet is currently handling an interaction with an object
	*/
	void SetIsHandlingInteraction(const bool isHandlingInteraction) {
		m_IsHandlingInteraction = isHandlingInteraction;
	}

	/**
	 * @return is pet currently handling an interaction with an object
	*/
	[[nodiscard]]
	bool IsHandlingInteraction() const noexcept {
		return m_IsHandlingInteraction;
	};

	/**
	 * Set up the pet bouncer interaction
	*/
	void SetupInteractBouncer();

	/**
	 * Starts the pet bouncer interaction
	*/
	void StartInteractBouncer();

	/**
	 * Handles the pet bouncer interaction
	*/
	void HandleInteractBouncer();

	/**
	 * Set up the treasure dig interaction
	 */
	void SetupInteractTreasureDig();

	/**
	 * Starts the pet treasure dig interaction
	*/
	void StartInteractTreasureDig();

	/**
	 * Handles the pet treasure dig interaction
	*/
	void HandleInteractTreasureDig();

	/**
	 * Returns the entity that this component belongs to
	 * @return the entity that this component belongs to
	 */
	[[nodiscard]]
	Entity* GetParentEntity() const noexcept {
		return m_Parent;
	}

	/**
	 * Sets the name of the pet to be moderated
	 * @param petName the name of the pet to set
	 */
	void SetPetNameForModeration(const std::string& petName);

	/**
	 * Loads the pet name up for moderation along with the moderation status from the database and sets them for this
	 * pet.
	 */
	void LoadPetNameFromModeration();

	/**
	 * Returns the component of the pet some entity is currently taming (if any)
	 * @param tamer the entity that's currently taming
	 * @return the pet component of the entity that's being tamed
	 */
	[[nodiscard]]
	static PetComponent* GetTamingPet(LWOOBJID tamer);

	/**
	 * Returns the pet that's currently spawned for some entity (if any)
	 * @param owner the owner of the pet that's spawned
	 * @return the pet component of the entity that was spawned by the owner
	 */
	[[nodiscard]]
	static PetComponent* GetActivePet(LWOOBJID owner);

	/**
	 * Adds the timer to the owner of this pet to drain imagination at the rate
	 * specified by the parameter imaginationDrainRate
	 *
	 * @param item The item that represents this pet in the inventory.
	 */
	void AddDrainImaginationTimer(Item* item, bool fromTaming = false);

private:
	// Needed so it can access flags
	friend class DamagingPets;

	/**
	 * Information for the minigame to be completed
	 */
	struct PuzzleData {
		/**
		 * The LOT of the object that is to be created
		 */
		LOT puzzleModelLot;

		/**
		 * That file that contains the bricks required to build the model
		 */
		std::string buildFile;

		/**
		 * The time limit to complete the build
		 */
		int32_t timeLimit;

		/**
		 * The imagination cost for the tamer to start the minigame
		 */
		int32_t imaginationCost;

		/**
		 * The number of pieces required to complete the minigame
		 */
		int32_t numValidPieces;
	};

	struct Interaction {
		/**
		 * The type of object that the pet is currently interacting with (e.g. a treasure chest or switch)
		*/
		PetInteractType type = PetInteractType::none;

		/**
		 * The interaction ability
		*/
		ePetAbilityType ability = ePetAbilityType::Invalid;

		/**
		 * The ID of the object that the pet is currently interacting with (e.g. a treasure chest or switch)
		 */
		LWOOBJID obj = LWOOBJID_EMPTY;
	};

	/**
	 * Pet interaction info
	*/
	Interaction m_Interaction{};

	/**
	 * Cache of all the pets that are currently spawned, indexed by tamer
	 */
	static std::unordered_map<LWOOBJID, LWOOBJID> activePets;

	/**
	 * Cache of all the pets that are currently being tamed, indexed by tamer
	 */
	static std::unordered_map<LWOOBJID, LWOOBJID> currentActivities;

	/**
	 * Cache of all the minigames and their information from the database
	 */
	static std::unordered_map<LOT, PetComponent::PuzzleData> buildCache;

	/**
	 * Flags that indicate that a player has tamed a pet, indexed by the LOT of the pet
	 */
	static std::map<LOT, int32_t> petFlags;

	/**
	 * The halting radius of the pet while following a player TODO: Move into struct?
	*/
	float m_FollowRadius;

	/**
	 * The ID of the component in the pet component table
	 */
	uint32_t m_ComponentId;

	/**
	 * The ID of the model that was built to complete the taming minigame for this pet
	 */
	LWOOBJID m_ModelId = LWOOBJID_EMPTY;

	/**
	 * The ID of the entity that owns this pet
	 */
	LWOOBJID m_Owner = LWOOBJID_EMPTY;

	/**
	 * The ID of the entity that is currently taming this pet
	 */
	LWOOBJID m_Tamer = LWOOBJID_EMPTY;

	/**
	 * The ID under which this pet is stored in the database (if it's tamed)
	 */
	LWOOBJID m_DatabaseId = LWOOBJID_EMPTY;

	/**
	 * The ID of the item from which this pet was created
	 */
	LWOOBJID m_ItemId;

	/**
	 * The moderation status for the name of this pet
	 */
	uint32_t m_ModerationStatus = 0;

	/**
	 * The name of this pet
	 */
	std::string m_Name;

	/**
	 * The name of the owner of this pet
	 */
	std::string m_OwnerName;

	/**
	 * The current flags of the pet (e.g. tamable, tamed, etc).
	 */
	Flag<PetFlag> m_Flags;

	/**
	 * The current state of the pet AI
	 */
	PetAiState m_State;

	/**
	 * The time an entity has left to complete the minigame
	 */
	float m_Timer = 0;

	/**
	 * A timer that tracks how long a tamed pet has been to far away from its owner, triggering a teleport after timeout
	 */
	float m_TimerAway = 0;

	/**
	 * A timer that tracks how long until a tamed pet will bounce again when standing over a treasure dig site
	*/
	float m_TimerBounce = 0;

	/**
	 * Boolean that sets if a pet is ready to interact with an object
	 */
	bool m_ReadyToInteract;

	/**
	 * Boolean that sets if a pet is currently handling an interaction with an object
	*/
	bool m_IsHandlingInteraction;

	/**
	 * The position that this pet was spawned at
	 */
	NiPoint3 m_StartPosition;

	/**
	 * The movement AI component that is related to this pet, required to move it around
	 */
	MovementAIComponent* m_MovementAI;

	/**
	 * Preconditions that need to be met before an entity can tame this pet
	 */
	std::optional<PreconditionExpression> m_Preconditions;

	/**
	 * Pet information loaded from the CDClientDatabase
	 */
	CDPetComponent m_PetInfo;
};

#endif // !PETCOMPONENT_H
