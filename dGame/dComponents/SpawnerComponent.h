#ifndef SPAWNERCOMPONENT_H
#define SPAWNERCOMPONENT_H

#include <memory>

#include "Component.h"
#include "eReplicaComponentType.h"

class Spawner;

/**
 * The spawner component (10) of a spawner object (LOT 176), made from the registry as the client makes it. It owns the
 * spawner (its nodes, what it spawned and its timers); the zone manager indexes spawners by the object's ID.
 */
class SpawnerComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::SPAWNER;

	SpawnerComponent(Entity* parent, const int32_t componentID);
	~SpawnerComponent() override;

	void SetSpawner(std::unique_ptr<Spawner> spawner);
	Spawner* GetSpawner() const { return m_Spawner.get(); }

private:
	std::unique_ptr<Spawner> m_Spawner;
};

#endif // SPAWNERCOMPONENT_H
