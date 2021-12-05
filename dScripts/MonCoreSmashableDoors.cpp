#include "MonCoreSmashableDoors.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "dZoneManager.h"

void MonCoreSmashableDoors::OnDie(Entity* self, Entity* killer) 
{
    auto myNum = self->GetVarAsString(u"spawner_name");

	myNum = myNum.substr(myNum.length() - 1, 1);
    
    auto triggerGroup = "CoreNookTrig0" + myNum;

	// Get the trigger
	auto triggers = EntityManager::Instance()->GetEntitiesInGroup(triggerGroup);

	if (triggers.empty())
	{
		return;
	}

	for (auto trigger : triggers)
	{
		trigger->OnFireEventServerSide(self, "DoorSmashed");
	}
}
