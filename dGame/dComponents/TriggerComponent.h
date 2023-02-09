#ifndef __TRIGGERCOMPONENT__H__
#define __TRIGGERCOMPONENT__H__

#include "Component.h"

namespace LUTriggers {
	struct Trigger;
	struct Command;
};

class TriggerComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_TRIGGER;

	explicit TriggerComponent(Entity* parent, const std::string triggerInfo);

	void TriggerEvent(eTriggerEventType event, Entity* optionalTarget = nullptr);
	LUTriggers::Trigger* GetTrigger() const { return m_Trigger; }

private:

	void HandleTriggerCommand(LUTriggers::Command* command, Entity* optionalTarget);
	std::vector<std::string> ParseArgs(std::string args);
	std::vector<Entity*> GatherTargets(LUTriggers::Command* command, Entity* optionalTarget);

	// Trigger Event Handlers
	void HandleSetPhysicsVolume(Entity* targetEntity, std::vector<std::string> argArray, std::string target);
	void HandleUpdateMission(Entity* targetEntity, std::vector<std::string> argArray);
	void HandleFireEvent(Entity* targetEntity, std::string args);
	void HandleCastSkill(Entity* targetEntity, uint32_t skillID);

	LUTriggers::Trigger* m_Trigger;
};
#endif  //!__TRIGGERCOMPONENT__H__
