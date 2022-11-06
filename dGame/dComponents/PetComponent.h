#pragma once

#include "Entity.h"
#include "MovementAIComponent.h"
#include "Component.h"
#include "Preconditions.h"

enum class PetAbilityType
{
	Invalid,
	GoToObject,
	JumpOnObject,
	DigAtPosition
};

/**
 * Represents an entity that is a pet. This pet can be tamed and consequently follows the tamer around, allowing it
 * to dig for treasure and activate pet bouncers.
 */
class PetComponent : public Component
{
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_PET;

	explicit PetComponent(Entity* parentEntity, uint32_t componentId);
	~PetComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
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
	void Command(NiPoint3 position, LWOOBJID source, int32_t commandType, int32_t typeId, bool overrideObey);

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
	PetAbilityType GetAbility() const;

	/**
	 * Sets the ability of the pet, currently unused
	 * @param value the ability to set
	 */
	void SetAbility(PetAbilityType value);

	/**
	 * Sets preconditions for the pet that need  to be met before it can be tamed
	 * @param conditions the preconditions to set
	 */
	void SetPreconditions(std::string& conditions);

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
	static std::map<LOT, uint32_t> petFlags;

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
	 * The current state of the pet (e.g. tamable, tamed, etc).
	 */
	uint32_t m_Status;

	/**
	 * A currently active ability, mostly unused
	 */
	PetAbilityType m_Ability;

	/**
	 * The time an entity has left to complete the minigame
	 */
	float m_Timer;

	/**
	 * A timer that tracks how long a tamed pet has been to far away from its owner, triggering a teleport after timeout
	 */
	float m_TimerAway;

	/**
	 * Timer that tracks how long a pet has been digging up some treasure, required to spawn the treasure contents
	 * on time
	 */
	float m_TresureTime;

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

	/**
	 * The rate at which imagination is drained from the user for having the pet out.
	 */
	float imaginationDrainRate;
};
