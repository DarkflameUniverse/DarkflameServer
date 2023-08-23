#ifndef __TRIGGERCOMPONENT__H__
#define __TRIGGERCOMPONENT__H__

#include "Component.h"
#include "LUTriggers.h"
#include "eReplicaComponentType.h"

class TriggerComponent : public Component {
public:
	static const eReplicaComponentType ComponentType = eReplicaComponentType::TRIGGER;

	explicit TriggerComponent(Entity* parent, const std::string triggerInfo);

	void TriggerEvent(eTriggerEventType event, Entity* optionalTarget = nullptr);
	LUTriggers::Trigger* GetTrigger() const { return m_Trigger; }
	void SetTriggerEnabled(bool enabled){ m_Trigger->enabled = enabled; };


private:

	void HandleTriggerCommand(LUTriggers::Command* command, Entity* optionalTarget);
	std::vector<Entity*> GatherTargets(LUTriggers::Command* command, Entity* optionalTarget);

	// Trigger Event Handlers
	void HandleFireEvent(Entity* targetEntity, std::string args);
	void HandleDestroyObject(Entity* targetEntity, std::string args);
	void HandleToggleTrigger(Entity* targetEntity, std::string args);
	void HandleResetRebuild(Entity* targetEntity, std::string args);
	void HandleSetPath(Entity* targetEntity, std::vector<std::string> argArray);
	void HandleMoveObject(Entity* targetEntity, std::vector<std::string> argArray);
	void HandleRotateObject(Entity* targetEntity, std::vector<std::string> argArray);
	void HandlePushObject(Entity* targetEntity, std::vector<std::string> argArray);
	void HandleRepelObject(Entity* targetEntity, std::string args);
	void HandleSetTimer(Entity* targetEntity, std::vector<std::string> argArray);
	void HandleCancelTimer(Entity* targetEntity, std::string args);
	void HandlePlayCinematic(Entity* targetEntity, std::vector<std::string> argArray);
	void HandleToggleBBB(Entity* targetEntity, std::string args);
	void HandleUpdateMission(Entity* targetEntity, std::vector<std::string> argArray);
	void HandleTurnAroundOnPath(Entity* targetEntity);
	void HandleGoForwardOnPath(Entity* targetEntity);
	void HandleGoBackwardOnPath(Entity* targetEntity);
	void HandleStopPathing(Entity* targetEntity);
	void HandleStartPathing(Entity* targetEntity);
	void HandlePlayEffect(Entity* targetEntity, std::vector<std::string> argArray);
	void HandleCastSkill(Entity* targetEntity, std::string args);
	void HandleSetPhysicsVolumeEffect(Entity* targetEntity, std::vector<std::string> argArray);
	void HandleSetPhysicsVolumeStatus(Entity* targetEntity, std::string args);
	void HandleActivateSpawnerNetwork(std::string args);
	void HandleDeactivateSpawnerNetwork(std::string args);
	void HandleResetSpawnerNetwork(std::string args);
	void HandleDestroySpawnerNetworkObjects(std::string args);
	void HandleActivatePhysics(Entity* targetEntity, std::string args);

	LUTriggers::Trigger* m_Trigger;
};
#endif  //!__TRIGGERCOMPONENT__H__
