#include "NsDanceNpcs.h"
#include "GameMessages.h"
#include "MissionComponent.h"

void NsDanceNpcs::OnStartup(Entity* self) 
{
}

void NsDanceNpcs::OnEmoteReceived(Entity* self, const int32_t emote, Entity* target)
{
	int validEmotes[] = { 383, 359, 377, 209, 208, 379, 380, 207, 382, 378, 381 };

	if (!std::find(std::begin(validEmotes), std::end(validEmotes), emote))
	{
		return;
	}

	auto* missionComponent = target->GetComponent<MissionComponent>();
	
	if (missionComponent != nullptr)
	{
		if (self->GetLOT() == 5006) // taskID(uid) = 898 for 5006 (Kant Dance)
			missionComponent->ForceProgress(595, 898, 1, false);
		else if (self->GetLOT() == 5007) // taskID(uid) = 899 for 5007 (Sofie Cushion)
			missionComponent->ForceProgress(595, 899, 1, false);
		else if (self->GetLOT() == 3002) // taskID(uid) = 900 for 3002 (Peppy Slapbiscuit)
			missionComponent->ForceProgress(595, 900, 1, false);
	}
}

