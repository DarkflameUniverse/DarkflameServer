#include "AgDarklingMech.h"
#include "DestroyableComponent.h"

void AgDarklingMech::OnStartup(Entity *self) {
    auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
    if (destroyableComponent != nullptr) {
        destroyableComponent->SetFaction(4);
    }
}
