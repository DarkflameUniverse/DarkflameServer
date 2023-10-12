#include "DieAfterXSeconds.h"
#include "Entity.h"
void DieAfterXSeconds::OnStartup(Entity* self) {
	self->AddCallbackTimer(m_Time,[self](){
		self->Smash(LWOOBJID_EMPTY, eKillType::VIOLENT);
	});
}