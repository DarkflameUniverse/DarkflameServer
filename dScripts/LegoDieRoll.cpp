#include "LegoDieRoll.h"
#include "Entity.h"
#include "dLogger.h"
#include "GameMessages.h"

void LegoDieRoll::OnStartup(Entity* self) {
    self->AddTimer("DoneRolling", 10.0f);
    self->AddTimer("ThrowDice", LegoDieRoll::animTime);
}

void LegoDieRoll::OnTimerDone(Entity* self, std::string timerName) {
    if (timerName == "DoneRolling") {
        self->Smash(self->GetObjectID(), SILENT);
    } 
    else if (timerName == "ThrowDice") {
        int dieRoll = GeneralUtils::GenerateRandomNumber<int>(1, 6);

        switch (dieRoll)
        {
        case 1:
		    GameMessages::SendPlayAnimation(self, u"roll-die-1");
            break;
        case 2:
		    GameMessages::SendPlayAnimation(self, u"roll-die-2");
            break;
        case 3:
		    GameMessages::SendPlayAnimation(self, u"roll-die-3");
            break;
        case 4:
		    GameMessages::SendPlayAnimation(self, u"roll-die-4");
            break;
        case 5:
		    GameMessages::SendPlayAnimation(self, u"roll-die-5");
            break;
        case 6:
		    GameMessages::SendPlayAnimation(self, u"roll-die-6");
            break;
        default:
	        Game::logger->LogDebug("LegoDieRoll", "Invalid animation: roll-die-%i\n", dieRoll);
            break;
        }
    }
}