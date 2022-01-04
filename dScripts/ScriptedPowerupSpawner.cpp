#include "ScriptedPowerupSpawner.h"
#include "RenderComponent.h"
#include "GameMessages.h"
#include "EntityManager.h"

void ScriptedPowerupSpawner::OnTemplateStartup(Entity *self) {
    self->SetVar<uint32_t>(u"currentCycle", 1);
    self->AddTimer("timeToSpawn", self->GetVar<float_t>(u"delayToFirstCycle"));
}

void ScriptedPowerupSpawner::OnTimerDone(Entity *self, std::string message) {
    if (message == "die") {
        self->Smash();
    } else if (message == "timeToSpawn") {

        const auto itemLOT = self->GetVar<LOT>(u"lootLOT");

        // Create the drops manually, since they don't exist in the matrix
        std::unordered_map<LOT, int32_t> drops;
        drops.insert({itemLOT, 1});

        // Spawn the required number of powerups
        auto* owner = EntityManager::Instance()->GetEntity(self->GetSpawnerID());
        if (owner != nullptr) {
            auto* renderComponent = self->GetComponent<RenderComponent>();
            for (auto i = 0; i < self->GetVar<uint32_t>(u"numberOfPowerups"); i++) {
                if (renderComponent != nullptr) {
                    renderComponent->PlayEffect(0, u"cast", "N_cast");
                }

                // Drop the given LOTs, avoiding the LootMatrix since there
                // are no entries for PowerSpawners
                LootGenerator::Instance().DropLoot(owner, self, drops, 0, 1);
            }

            // Increment the current cycle
            if (self->GetVar<uint32_t>(u"currentCycle") < self->GetVar<uint32_t>(u"numCycles")) {
                self->AddTimer("timeToSpawn", self->GetVar<float_t>(u"secPerCycle"));
                self->SetVar<uint32_t>(u"currentCycle", self->GetVar<uint32_t>(u"currentCycle") + 1);
            } else {
                // Kill if this was the last cycle
                self->AddTimer("die", self->GetVar<float_t>(u"deathDelay"));
            }
        }
    }
}
