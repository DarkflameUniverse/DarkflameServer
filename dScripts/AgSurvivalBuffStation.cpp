#include "AgSurvivalBuffStation.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "SkillComponent.h"
#include "dLogger.h"

void AgSurvivalBuffStation::OnRebuildComplete(Entity* self, Entity* target) {
    auto destroyableComponent = self->GetComponent<DestroyableComponent>();
    // We set the faction to 6 so that the buff station sees players as friendly targets to buff
    if (destroyableComponent != nullptr) destroyableComponent->SetFaction(6);

    auto skillComponent = self->GetComponent<SkillComponent>();

    if (skillComponent != nullptr) skillComponent->CalculateBehavior(skillIdForBuffStation, behaviorIdForBuffStation, self->GetObjectID());

    self->AddCallbackTimer(smashTimer, [self]() {
        self->Smash();
    });
    self->AddTimer("DropArmor", dropArmorTimer);
    self->AddTimer("DropLife", dropLifeTimer);
    self->AddTimer("Dropimagination", dropImaginationTimer);
    self->SetVar<LWOOBJID>(u"PlayerId", target->GetObjectID());
}

void AgSurvivalBuffStation::OnTimerDone(Entity* self, std::string timerName) {
    auto targetID = self->GetVar<LWOOBJID>(u"PlayerId");
    auto target = EntityManager::Instance()->GetEntity(targetID);
    uint32_t powerupToDrop = lifePowerup;
    if (timerName == "DropArmor") {
        powerupToDrop = armorPowerup;
        self->AddTimer("DropArmor", dropArmorTimer);
    }
    if (timerName == "DropLife") {
        powerupToDrop = lifePowerup;
        self->AddTimer("DropLife", dropLifeTimer);
    }
    if (timerName == "Dropimagination") {
        powerupToDrop = imaginationPowerup;
        self->AddTimer("Dropimagination", dropImaginationTimer);
    }
    if (target != nullptr) GameMessages::SendDropClientLoot(target, self->GetObjectID(), powerupToDrop, 0, self->GetPosition());
}
void AgSurvivalBuffStation::OnDie(Entity* self, Entity* killer) {
    //self->CancelAllTimers();
}