#include "ScriptedPowerupSpawner.h"
#include "RenderComponent.h"
#include "EntityManager.h"
#include "Loot.h"

void ScriptedPowerupSpawner::OnTemplateStartup(Entity* self) {
	self->SetVar<uint32_t>(u"currentCycle", 1);
	self->AddTimer("timeToSpawn", self->GetVar<float_t>(u"delayToFirstCycle"));
}

void ScriptedPowerupSpawner::OnTimerDone(Entity* self, std::string message) {
	if (message == "die") {
		self->Smash();
	} else if (message == "timeToSpawn") {

		const auto itemLOT = self->GetVar<LOT>(u"lootLOT");

		// Spawn the required number of powerups
		auto* owner = Game::entityManager->GetEntity(self->GetSpawnerID());
		if (owner != nullptr) {
			auto* renderComponent = self->GetComponent<RenderComponent>();
			for (auto i = 0; i < self->GetVar<uint32_t>(u"numberOfPowerups"); i++) {
				if (renderComponent != nullptr) {
					renderComponent->PlayEffect(0, u"cast", "N_cast");
				}
				GameMessages::GetPosition posMsg{};
				posMsg.target = self->GetObjectID();
				posMsg.Send();

				GameMessages::DropClientLoot lootMsg{};
				lootMsg.target = owner->GetObjectID();
				lootMsg.ownerID = owner->GetObjectID();
				lootMsg.sourceID = self->GetObjectID();
				lootMsg.spawnPos = posMsg.pos;
				lootMsg.item = itemLOT;
				lootMsg.count = 1;
				lootMsg.currency = 0;
				Loot::DropItem(*owner, lootMsg, true, true);
			}

			// Increment the current cycle
			if (self->GetVar<uint32_t>(u"currentCycle") < self->GetVar<uint32_t>(u"numCycles")) {
				self->AddTimer("timeToSpawn", self->GetVar<float_t>(u"secPerCycle"));
				self->SetVar<uint32_t>(u"currentCycle", self->GetVar<uint32_t>(u"currentCycle") + 1);
			}

			// Kill if this was the last cycle
			if (self->GetVar<uint32_t>(u"currentCycle") >= self->GetVar<uint32_t>(u"numCycles")) {
				self->AddTimer("die", self->GetVar<float_t>(u"deathDelay"));
			}
		}
	}
}
