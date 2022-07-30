#ifndef SWITCHCOMPONENT_H
#define SWITCHCOMPONENT_H

#include "RakNetTypes.h"
#include "Entity.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "RebuildComponent.h"
#include "BouncerComponent.h"
#include <algorithm>
#include "Component.h"

/**
 * A component for switches in game, including pet triggered switches.
 */
class SwitchComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_SWITCH;

	SwitchComponent(Entity* parent);
	~SwitchComponent() override;

	void Update(float deltaTime) override;

	Entity* GetParentEntity() const;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Sets whether the switch is on or off.
	 * @param active whether the switch is on or off.
	 */
	void SetActive(bool active);

	/**
	 * Returns whether the switch is on or off.
	 */
	bool GetActive() const;

	/**
	 * Sets the attached pet bouncer
	 * @param value the attached pet bouncer
	 */
	void SetPetBouncer(BouncerComponent* value);

	/**
	 * Returns the attached pet bouncer
	 */
	BouncerComponent* GetPetBouncer() const;

	/**
	 * Invoked when a entity enters the trigger area.
	 */
	void EntityEnter(Entity* entity);

	/**
	 * Invoked when a entity leaves the trigger area.
	 */
	void EntityLeave(Entity* entity);

	/**
	 * Returns the closest switch from a given position
	 * @param position the position to check
	 * @return the closest switch from a given position
	 */
	static SwitchComponent* GetClosestSwitch(NiPoint3 position);

private:
	/**
	 * A list of all pet switches.
	 */
	static std::vector<SwitchComponent*> petSwitches;

	/**
	 * Attached rebuild component.
	 */
	RebuildComponent* m_Rebuild;

	/**
	 * If the switch is on or off.
	 */
	bool m_Active;

	/**
	 * The amount of entities in the trigger area.
	 */
	int m_EntitiesOnSwitch = 0;

	/**
	 * The switch reset time
	 */
	int m_ResetTime = INT_MAX;

	/**
	 * Timer for resetting the switch
	 */
	float m_Timer = 0.0f;

	/**
	 * Attached pet bouncer
	 */
	BouncerComponent* m_PetBouncer = nullptr;
};

#endif // SWITCHCOMPONENT_H
