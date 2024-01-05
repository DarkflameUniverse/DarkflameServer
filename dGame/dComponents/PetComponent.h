#ifndef PETCOMPONENT_H
#define PETCOMPONENT_H

#include "Entity.h"
#include "MovementAIComponent.h"
#include "Component.h"
#include "Preconditions.h"
#include "eReplicaComponentType.h"
#include "ePetAbilityType.h"
#include "CDPetComponentTable.h"
#include "CDClientManager.h"

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
enum PetInteractType : uint8_t {
	none,		// Not interacting
	treasure,	// Treasure dig
	bouncer		// Bouncer switch
};

/**
 * The flags governing the status of the pet: Governs the icon above their head and the interactions available
*/
enum PetFlag : uint32_t {
	NONE,
	IDLE = 1 << 0,			//0x01 - Seems to be "idle," which the game doesn't differentiate from "follow"
	UNKNOWN4 = 1 << 2,		//0x04 - FOLLOWING(?)
	BEING_TAMED = 1 << 4, 	//0x10,
	NOT_WAITING = 1 << 5, 	//0x20,
	IMMOBILE = 1 << 6,		//0x40 - Seems to be the "stop moving" flag - called when taming begins and stays active until a name is submitted
	SPAWNING = 1 << 7,		//0x80
	ON_SWITCH = 1 << 8, 	//0x100
	UNKNOWN1024 = 1 << 10,	//0x400
	TAMEABLE = 1 << 26		//0x4000000
};

/*
132 = 128 + 4
*/

/**
 * The pet emote animation ids that can used in PetComponent::Command()
*/
enum PetEmote : int32_t {
	ActivateSwitch = 201,
	DigTreasure,
	Bounce
};

/**
 * Represents an entity that is a pet. This pet can be tamed and consequently follows the tamer around, allowing it
 * to dig for treasure and activate pet bouncers.
 */
class PetComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::PET;

	/**
	 * Pet information loaded from the CDClientDatabase
	 */
	CDPetComponent& m_PetInfo;

	/**
	 * PetComponent constructor
	 * @param parentEntity The parent entity
	 * @param componentId The component id
	*/
	explicit PetComponent(Entity* parentEntity, uint32_t componentId) : Component{ parentEntity },
		m_PetInfo{ CDClientManager::Instance().GetTable<CDPetComponentTable>()->GetByID(componentId) } {
		m_ComponentId = componentId;
		m_Interaction = LWOOBJID_EMPTY;
		m_InteractType = PetInteractType::none;
		m_Owner = LWOOBJID_EMPTY;
		m_ModerationStatus = 0;
		m_Tamer = LWOOBJID_EMPTY;
		m_ModelId = LWOOBJID_EMPTY;
		m_Timer = 0;
		m_TimerAway = 0;
		m_TimerBounce = 0;
		m_DatabaseId = LWOOBJID_EMPTY;
		m_Flags = PetFlag::SPAWNING; // Tameable
		m_Ability = ePetAbilityType::Invalid;
		m_StartPosition = m_Parent->GetPosition();
		m_MovementAI = nullptr;
		m_Preconditions = nullptr;

		m_ReadyToInteract = false;
		SetPetAiState(PetAiState::spawn);
		SetIsHandlingInteraction(false);

		std::string checkPreconditions = GeneralUtils::UTF16ToWTF8(parentEntity->GetVar<std::u16string>(u"CheckPrecondition"));

		if (!checkPreconditions.empty()) {
			SetPreconditions(checkPreconditions);
		}

		m_FollowRadius = 8.0f; //Game::zoneManager->GetPetFollowRadius(); // TODO: FIX THIS TO LOAD DYNAMICALLY
	}

	~PetComponent() override;

	/**
	 * Serializes the pet
	 * @param outBitStream The output bitstream
	 * @param bIsInitialUpdate Boolean value of whether this is the initial update
	*/
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) override;

	/**
	 * Sets the AI state of the pet
	 * @param newState New pet AI state
	*/
	void SetPetAiState(PetAiState newState);

	/**
	 * Gets the AI state of the pet
	*/
	PetAiState GetPetAiState() { return m_State; };

	/**
	 * Sets one or more pet flags
	 * @param flag PetFlag(s) to set
	*/
	template <typename... varArg>
	void SetFlag(varArg... flag) { m_Flags |= (static_cast<uint32_t>(flag) | ...); };

	/**
	 * Sets the pet to ONLY have the specified flag(s), clearing all others
	 * @param flag PetFlag(s) to set exclusively
	*/
	template <typename... varArg>
	void SetOnlyFlag(varArg... flag) { m_Flags = (static_cast<uint32_t>(flag) | ...); };

	/**
	 * Unsets one or more pet flags
	 * @param flag PetFlag(s) to unset
	*/
	template <typename... varArg>
	void UnsetFlag(varArg... flag) { m_Flags &= ~(static_cast<uint32_t>(flag) | ...); };

	/**
	 * Returns true if the pet has all the specified flag(s)
	 * @param flag PetFlag(s) to check
	*/
	template <typename... varArg>
	const bool HasFlag(varArg... flag) { return (m_Flags & (static_cast<uint32_t>(flag) | ...)) == (static_cast<uint32_t>(flag) | ...); };

	/**
	 * Returns true if the pet has ONLY the specified flag(s)
	 * @param flag PetFlag(s) to check if the pet has exclusively
	*/
	template <typename... varArg>
	const bool HasOnlyFlag(varArg... flag) { return m_Flags == (static_cast<uint32_t>(flag) | ...); };

	/**
	 * Governs the pet update loop
	 * @param deltaTime Time elapsed since last update
	*/
	void Update(float deltaTime) override;

	/**
	 * Handles updates for unowned pets
	 * @param deltaTime time since last update
	 */
	void UpdateUnowned(float deltaTime);

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
	void RequestSetPetName(std::u16string name);

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
	void StartInteract(const NiPoint3& position, const PetInteractType interactType, const LWOOBJID& interactID);

	/**
	 * Stop a pet interaction with an object
	 * @param bDontSerialize optional parameter, set to true to not serialize afterwards
	*/
	void StopInteract(bool bDontSerialize = false);

	/**
	 * Set the type of interaction the pet is executing
	*/
	void SetInteractType(PetInteractType interactType) { m_InteractType = interactType; };

	/**
	 * Get the type of interaction the pet is executing
	*/
	PetInteractType GetInteractType() { return m_InteractType; };

	/**
	 * Spawns a pet from an item in the inventory of an owner
	 * @param item the item to create the pet from
	 * @param registerPet notifies the client that the pet was spawned, not necessary if this pet is being tamed
	 */
	void Activate(Item* item, bool registerPet = true, bool fromTaming = false);

	/**
	 * Despawns the pet
	 */
	void Deactivate();

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
	void Command(const NiPoint3& position, const LWOOBJID& source, int32_t commandType, int32_t typeId, bool overrideObey);

	/**
	 * Returns the ID of the owner of this pet (if any)
	 * @return the ID of the owner of this pet
	 */
	LWOOBJID GetOwnerId() const;

	/**
	 * Returns the entity that owns this pet (if any)
	 * @return the entity that owns this pet
	 */
	Entity* GetOwner() const;

	/**
	 * Returns the ID that is stored in the database with regards to this pet, only set for pets that are tamed
	 * @return the ID that is stored in the database with regards to this pet
	 */
	LWOOBJID GetDatabaseId() const;

	/**
	 * Returns the ID of the object that the pet is currently interacting with, could be a treasure chest or a switch
	 * @return the ID of the object that the pet is currently interacting with
	 */
	LWOOBJID GetInteraction() const;

	/**
	 * Sets the ID that the pet is interacting with
	 * @param value the ID that the pet is interacting with
	 */
	void SetInteraction(LWOOBJID value);

	/**
	 * Returns the ID that this pet was spawned from, only set for tamed pets
	 * @return the ID that this pet was spawned from
	 */
	LWOOBJID GetItemId() const;

	/**
	 * Returns the status of this pet, e.g. tamable or tamed. The values here are still a bit of mystery and likely a
	 * bit map
	 * @return the status of this pet
	 */
	uint32_t GetStatus() const;

	/**
	 * Sets the current status of the pet
	 * @param value the current status of the pet to set
	 */
	void SetStatus(uint32_t value);

	/**
	 * Returns an ability the pet may perform, currently unused
	 * @return an ability the pet may perform
	 */
	ePetAbilityType GetAbility() const;

	/**
	 * Sets the ability of the pet, currently unused
	 * @param value the ability to set
	 */
	void SetAbility(ePetAbilityType value);

	/**
	 * Sets preconditions for the pet that need  to be met before it can be tamed
	 * @param conditions the preconditions to set
	 */
	void SetPreconditions(std::string& conditions);

	/**
	 * Sets if the pet is ready to interact with an object
	 * @param isReady whether the pet is ready to interact (true) or not (false)
	 */
	void SetIsReadyToInteract(bool isReady) { m_ReadyToInteract = isReady; };

	/**
	 * @return is pet ready to interact with an object
	 */
	bool IsReadyToInteract() { return m_ReadyToInteract; };

	/**
	 * Sets if the pet is currently handling an interaction with an object
	 * @param isHandlingInteraction whether the pet is currently handling an interaction with an object
	*/
	void SetIsHandlingInteraction(bool isHandlingInteraction) { m_IsHandlingInteraction = isHandlingInteraction; };

	/**
	 * @return is pet currently handling an interaction with an object
	*/
	bool IsHandlingInteraction() { return m_IsHandlingInteraction; };

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
	Entity* GetParentEntity() const;

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
	static PetComponent* GetTamingPet(LWOOBJID tamer);

	/**
	 * Returns the pet that's currently spawned for some entity (if any)
	 * @param owner the owner of the pet that's spawned
	 * @return the pet component of the entity that was spawned by the owner
	 */
	static PetComponent* GetActivePet(LWOOBJID owner);

	/**
	 * Adds the timer to the owner of this pet to drain imagination at the rate
	 * specified by the parameter imaginationDrainRate
	 *
	 * @param item The item that represents this pet in the inventory.
	 */
	void AddDrainImaginationTimer(Item* item, bool fromTaming = false);

private:

	/**
	 * Information for the minigame to be completed
	 */
	struct PetPuzzleData
	{
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
	static std::unordered_map<LOT, PetComponent::PetPuzzleData> buildCache;

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
	LWOOBJID m_ModelId;

	/**
	 * The ID of the object that the pet is currently interacting with (e.g. a treasure chest or switch)
	 */
	LWOOBJID m_Interaction;

	/**
	 * The type of object that the pet is currently interacting with (e.g. a treasure chest or switch)
	*/
	PetInteractType m_InteractType;

	/**
	 * The ID of the entity that owns this pet
	 */
	LWOOBJID m_Owner;

	/**
	 * The ID of the entity that is currently taming this pet
	 */
	LWOOBJID m_Tamer;

	/**
	 * The ID under which this pet is stored in the database (if it's tamed)
	 */
	LWOOBJID m_DatabaseId;

	/**
	 * The ID of the item from which this pet was created
	 */
	LWOOBJID m_ItemId;

	/**
	 * The moderation status for the name of this pet
	 */
	uint32_t m_ModerationStatus;

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
	uint32_t m_Flags;

	/**
	 * The current state of the pet AI
	 */
	PetAiState m_State;

	/**
	 * A currently active ability, mostly unused
	 */
	ePetAbilityType m_Ability;

	/**
	 * The time an entity has left to complete the minigame
	 */
	float m_Timer;

	/**
	 * A timer that tracks how long a tamed pet has been to far away from its owner, triggering a teleport after timeout
	 */
	float m_TimerAway;

	/**
	 * A timer that tracks how long until a tamed pet will bounce again when standing over a treasure dig site
	*/
	float m_TimerBounce;

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
	PreconditionExpression* m_Preconditions;
};

#endif // PETCOMPONENT_H
