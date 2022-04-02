#pragma once

#include <unordered_map>

#include <gravity_compiler.h>
#include <gravity_macros.h>
#include <gravity_core.h>
#include <gravity_vm.h>
#include <gravity_vmmacros.h>

#include "GravityEntity.h"
#include "CppScripts.h"
#include "ChatPackets.h"

class GravityScript : public CppScripts::Script {
public:
    GravityScript(uint32_t id, gravity_vm* vm);
    ~GravityScript();

    void Call(std::string name, std::vector<gravity_value_t> args);
    void SetupGlobals();

    void OnStartup(Entity* self) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        Call("OnStartup", {
            newEntity.GetGravityValue()
        });
    }

    void OnCollisionPhantom(Entity* self, Entity* target) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(target);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnCollisionPhantom", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    };

    void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(target);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnMissionDialogueOK", {
            newEntity.GetGravityValue(), 
            secondEntity.GetGravityValue(), 
            VALUE_FROM_INT(missionID), 
            VALUE_FROM_INT((int)missionState)
        });
    };

   void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override {
       auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(sender);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnFireEventServerSide", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue(),
            VALUE_FROM_CSTRING(args.c_str()),
            VALUE_FROM_INT(param1),
            VALUE_FROM_INT(param2),
            VALUE_FROM_INT(param3)
        });
   }

    void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(sender);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnNotifyObject", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue(),
            VALUE_FROM_CSTRING(name.c_str()),
            VALUE_FROM_INT(param1),
            VALUE_FROM_INT(param2)
        });
    }

    void OnModularBuildExit(Entity* self, Entity* player, bool bCompleted, std::vector<LOT> modules) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnModularBuildExit", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue(),
            VALUE_FROM_BOOL(bCompleted),
            VALUE_FROM_INT(modules.size())
        });
    }

    void OnPlayerLoaded(Entity* self, Entity* player) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnPlayerLoaded", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnPlayerDied(Entity* self, Entity* player) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnPlayerDied", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }
    
    void OnPlayerResurrected(Entity* self, Entity* player) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnPlayerResurrected", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnPlayerExit(Entity* self, Entity* player) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnPlayerExit", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnProximityUpdate(Entity* self, Entity* sender, std::string name, std::string status) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(sender);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnProximityUpdate", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue(),
            VALUE_FROM_CSTRING(name.c_str()),
            VALUE_FROM_CSTRING(status.c_str())
        });
    }

    void OnTimerDone(Entity* self, std::string timerName) {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnTimerDone", {
            newEntity.GetGravityValue(),
            VALUE_FROM_CSTRING(timerName.c_str())
        });
    }

    void OnUse(Entity* self, Entity* user) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(user);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnUse", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnDie(Entity* self, Entity* killer) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(killer);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnDie", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnHit(Entity* self, Entity* attacker) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(attacker);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnHit", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnEmoteReceived(Entity* self, int32_t emote, Entity* target) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(target);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnEmoteReceived", {
            newEntity.GetGravityValue(),
            VALUE_FROM_INT(emote),
            secondEntity.GetGravityValue()
        });
    }

    void OnRebuildStart(Entity* self, Entity* player) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnRebuildStart", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnRebuildNotifyState(Entity* self, eRebuildState state) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnRebuildNotifyState", {
            newEntity.GetGravityValue(),
            VALUE_FROM_INT(state)
        });
    }

    void OnRebuildComplete(Entity* self, Entity* target) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(target);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnRebuildComplete", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(attacker);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnHitOrHealResult", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue(),
            VALUE_FROM_INT(damage)
        });
    }

    void OnRespondToMission(Entity* self, int missionID, Entity* player, int reward) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnRespondToMission", {
            newEntity.GetGravityValue(),
            VALUE_FROM_INT(missionID),
            secondEntity.GetGravityValue(),
            VALUE_FROM_INT(reward)
        });
    }

    void OnUpdate(Entity* self) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnUpdate", {
            newEntity.GetGravityValue()
        });
    }

    void OnZonePropertyRented(Entity* self, Entity* renter) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(renter);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnZonePropertyRented", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnZonePropertyEditBegin(Entity* self) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnZonePropertyEditBegin", {
            newEntity.GetGravityValue()
        });
    }

    void OnZonePropertyEditEnd(Entity* self) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnZonePropertyEditEnd", {
            newEntity.GetGravityValue()
        });
    }

    void OnZonePropertyModelEquipped(Entity* self) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnZonePropertyModelEquipped", {
            newEntity.GetGravityValue()
        });
    }

    void OnZonePropertyModelPlaced(Entity* self, Entity* player) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnZonePropertyModelPlaced", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnZonePropertyModelPickedUp(Entity* self, Entity* player) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnZonePropertyModelPickedUp", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnZonePropertyModelRemoved(Entity* self, Entity* player) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnZonePropertyModelRemoved", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnZonePropertyModelRemovedWhileEquipped(Entity* self, Entity* player) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnZonePropertyModelRemovedWhileEquipped", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnZonePropertyModelRotated(Entity* self, Entity* player) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnZonePropertyModelRotated", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue()
        });
    }

    void OnNotifyPetTamingMinigame(Entity* self, Entity* player, eNotifyType type) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(player);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnNotifyPetTamingMinigame", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue(),
            VALUE_FROM_INT(type)
        });
    }

    void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(sender);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnMessageBoxResponse", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue(),
            VALUE_FROM_INT(button),
            VALUE_FROM_STRING(GeneralUtils::UTF16ToWTF8(identifier).c_str()),
            VALUE_FROM_STRING(GeneralUtils::UTF16ToWTF8(userData).c_str())
        });
    } 

    void OnChoiceBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(sender);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnChoiceBoxResponse", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue(),
            VALUE_FROM_INT(button),
            VALUE_FROM_STRING(GeneralUtils::UTF16ToWTF8(buttonIdentifier).c_str()),
            VALUE_FROM_STRING(GeneralUtils::UTF16ToWTF8(identifier).c_str())
        });
    }

    void OnWaypointReached(Entity* self, uint32_t waypointIndex) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnWaypointReached", {
            newEntity.GetGravityValue(),
            VALUE_FROM_INT(waypointIndex)
        });
    }

    void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(caster);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnSkillEventFired", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue(),
            VALUE_FROM_STRING(message.c_str())
        });
    }

    void OnSkillCast(Entity* self, uint32_t skillID) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnSkillCast", {
            newEntity.GetGravityValue(),
            VALUE_FROM_INT(skillID)
        });
    }

    void OnPlayerRailArrived(Entity* self, Entity* sender, const std::u16string& pathName, int32_t waypoint) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(sender);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnPlayerRailArrived", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue(),
            VALUE_FROM_STRING(GeneralUtils::UTF16ToWTF8(pathName).c_str()),
            VALUE_FROM_INT(waypoint)
        });
    }

    void OnActivityStateChangeRequest(Entity* self, const LWOOBJID senderID, const int32_t value1, const int32_t value2, const std::u16string& stringValue) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnActivityStateChangeRequest", {
            newEntity.GetGravityValue(),
            VALUE_FROM_INT(senderID),
            VALUE_FROM_INT(value1),
            VALUE_FROM_INT(value2),
            VALUE_FROM_STRING(GeneralUtils::UTF16ToWTF8(stringValue).c_str())
        });
    }

    void OnCinematicUpdate(Entity* self, Entity* sender, eCinematicEvent event, const std::u16string& pathName, float_t pathTime, float_t totalTime, int32_t waypoint) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(sender);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        this->Call("OnCinematicUpdate", {
            newEntity.GetGravityValue(),
            secondEntity.GetGravityValue(),
            VALUE_FROM_INT(event),
            VALUE_FROM_STRING(GeneralUtils::UTF16ToWTF8(pathName).c_str()),
            VALUE_FROM_NUMBER(pathTime),
            VALUE_FROM_NUMBER(totalTime),
            VALUE_FROM_INT(waypoint)
        });
    }
private:

    std::vector<std::string> toCheckFunctions = {
        "OnStartup",
        "OnCollisionPhantom",
        "OnMissionDialogueOk",
        "OnFireEventServerSide",
        "OnNotifyObject",
        "OnModularBuildExit",
        "OnPlayerLoaded",
        "OnPlayerDied",
        "OnPlayerResurrected",
        "OnPlayerExit",
        "OnProximityUpdate",
        "OnTimerDone",
        "OnUse",
        "OnDie",
        "OnHit",
        "OnEmoteReceived",
        "OnRebuildStart",
        "OnRebuildNotifyState",
        "OnRebuildComplete",
        "OnHitOrHealResult",
        "OnRespondToMission",
        "OnUpdate",
        "OnZonePropertyRented",
        "OnZonePropertyEditBegin",
        "OnZonePropertyEditEnd",
        "OnZonePropertyModelEquipped",
        "OnZonePropertyModelPlaced",
        "OnZonePropertyModelPickedUp",
        "OnZonePropertyModelRemoved",
        "OnZonePropertyModelRemovedWhileEquipped",
        "OnZonePropertyModelRotated",
        "OnNotifyPetTamingMinigame",
        "OnMessageBoxResponse",
        "OnChoiceBoxResponse",
        "OnWaypointReached",
        "OnSkillEventFired",
        "OnSkillCast",
        "OnPlayerRailArrived",
        "OnActivityStateChangeRequest",
        "OnCinematicUpdate"
    };

    gravity_vm* m_VM = nullptr;
    uint32_t m_Id;
    std::unordered_map<std::string, gravity_closure_t*> m_KnownFunctions;
};