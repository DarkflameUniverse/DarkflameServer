#ifndef SWITCHCOMPONENT_H
#define SWITCHCOMPONENT_H

#include "RakNetTypes.h"
#include "Entity.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "QuickBuildComponent.h"
#include "BouncerComponent.h"
#include <algorithm>
#include "Component.h"
#include "eReplicaComponentType.h"

/**
 * A component for switches in game, including pet triggered switches.
 */
class SwitchComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::SWITCH;

	SwitchComponent(Entity* parent);
	~SwitchComponent() override;

	void LoadConfigData() override;
	void Startup() override;
	void Update(float deltaTime) override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Sets whether the switch is on or off.
	 * @param active whether the switch is on or off.
	 */
	void SetActive(const bool active);

	/**
	 * Returns whether the switch is on or off.
	 */
	bool GetActive() const { return m_Active; }

	/**
	 * Sets the attached pet bouncer
	 * @param value the attached pet bouncer
	 */
	void SetPetBouncer(BouncerComponent* value);

	/**
	 * Returns the attached pet bouncer
	 */
	BouncerComponent* GetBouncer() const { return m_Bouncer; }

	/**
	 * Invoked when a entity enters the trigger area.
	 */
	void EntityEnter(Entity* entity);

	/**
	 * Returns the closest switch from a given position
	 * @param position the position to check
	 * @return the closest switch from a given position
	 */
	static SwitchComponent* GetClosestSwitch(const NiPoint3& position);

private:
	/**
	 * A list of all pet switches.
	 */
	static std::vector<SwitchComponent*> switches;

	/**
	 * Attached rebuild component.
	 */
	QuickBuildComponent* m_Rebuild;

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
	BouncerComponent* m_Bouncer = nullptr;
};

#endif // SWITCHCOMPONENT_H
