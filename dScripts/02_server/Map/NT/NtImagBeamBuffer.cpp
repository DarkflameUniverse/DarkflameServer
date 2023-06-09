#include "NtImagBeamBuffer.h"
#include "EntityManager.h"
#include "SkillComponent.h"

void NtImagBeamBuffer::OnStartup(Entity* self) {
	self->SetProximityRadius(100, "ImagZone");

	self->AddTimer("BuffImag", 2.0f);
}

void NtImagBeamBuffer::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name != "ImagZone" || !entering->IsPlayer()) {
		return;
	}

	if (status == "ENTER") {
		const auto& iter = std::find(m_EntitiesInProximity.begin(), m_EntitiesInProximity.end(), entering->GetObjectID());

		if (iter == m_EntitiesInProximity.end()) {
			m_EntitiesInProximity.push_back(entering->GetObjectID());
		}
	} else if (status == "LEAVE") {
		const auto& iter = std::find(m_EntitiesInProximity.begin(), m_EntitiesInProximity.end(), entering->GetObjectID());

		if (iter != m_EntitiesInProximity.end()) {
			m_EntitiesInProximity.erase(iter);
		}
	}
}

void NtImagBeamBuffer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName != "BuffImag") {
		return;
	}

	auto* skillComponent = self->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		return;
	}

	for (const auto entityID : m_EntitiesInProximity) {
		auto* entity = EntityManager::Instance()->GetEntity(entityID);

		if (entity == nullptr) {
			continue;
		}

		skillComponent->CalculateBehavior(1311, 30235, entityID, true);
	}

	self->AddTimer("BuffImag", 2.0f);
}
