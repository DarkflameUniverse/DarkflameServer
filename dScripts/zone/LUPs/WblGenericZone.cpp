#include "WblGenericZone.h"
#include "Player.h"

void WblGenericZone::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (args == m_WblAbortMsg) {
		if (!sender) return;
		auto player = dynamic_cast<Player*>(sender);
		if (player) player->SendToZone(m_WblMainZone);
	}
}
