#pragma once
#include "CppScripts.h"

// L_SIMPLE_ONCE_ONLY_MOVER_SPINNER_EVENT.lua (02_server/Map/firetemple): a simple mover (or a mover on a two point
// path) that a spinner sends to its other end. It is put in the spinner's group_deactivated_event group with the
// spinner's name_deactivated_event, and goes to waypoint 1 when the spinner fires that event.
class SimpleOnceOnlyMoverSpinnerEvent : public CppScripts::Script {
public:
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;
};
