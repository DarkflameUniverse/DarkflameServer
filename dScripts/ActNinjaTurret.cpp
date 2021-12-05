#include "ActNinjaTurret.h"

void ActNinjaTurret::OnRebuildNotifyState(Entity* self, eRebuildState state)
{
    Game::logger->Log("ActNinjaTurret", "Rebuild state: %i\n", state);

    if (state == eRebuildState::REBUILD_COMPLETED)
    {
        Game::logger->Log("ActNinjaTurret", "I am build\n");

        self->SetVar(u"AmBuilt", true);
    }
    else if (state == eRebuildState::REBUILD_RESETTING)
    {
        Game::logger->Log("ActNinjaTurret", "I am not build\n");

        self->SetVar(u"AmBuilt", false);
    }
}

void
ActNinjaTurret::OnFireEventServerSide(Entity *self, Entity *sender, std::string args, int32_t param1, int32_t param2,
                                      int32_t param3)
{
    Game::logger->Log("ActNinjaTurret", "Got server side event %s\n", args.c_str());

    if (args == "ISpawned" && self->GetVar<bool>(u"AmBuilt"))
    {
        sender->Smash();
    }
}
