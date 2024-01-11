#include "WblGenericZone.h"
#include "CharacterComponent.h"

void WblGenericZone::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (args == m_WblAbortMsg) {
		if (!sender) return;

		auto* characterComponent = sender->GetComponent<CharacterComponent>();
		if (characterComponent) characterComponent->SendToZone(m_WblMainZone);
	}
}
