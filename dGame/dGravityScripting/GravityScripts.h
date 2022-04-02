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

    void OnCollisionPhantom(Entity* self, Entity* target) override {
        auto newEntity = GravityEntity(self);
        newEntity.CreateEntityGravityValue(this->m_VM);

        auto secondEntity = GravityEntity(target);
        secondEntity.CreateEntityGravityValue(this->m_VM);

        Game::logger->Log("OnCollisionPhantom", "Object ID: " + std::to_string(target->GetObjectID()));
        this->Call("OnCollisionPhantom", {newEntity.GetGravityValue(), secondEntity.GetGravityValue()});
    };
private:
    /*
        virtual void OnFireEventServerSide(Entity *self, Entity *sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {};
		virtual void OnNotifyObject(Entity *self, Entity *sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0) {};
		virtual void OnModularBuildExit(Entity* self, Entity* player, bool bCompleted, std::vector<LOT> modules) {};
		virtual void OnPlayerLoaded(Entity* self, Entity* player) {};
		virtual void OnPlayerDied(Entity* self, Entity* player) {};
		virtual void OnPlayerResurrected(Entity* self, Entity* player) {};
		virtual void OnPlayerExit(Entity* self, Entity* player) {};
		virtual void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {};
		virtual void OnTimerDone(Entity* self, std::string timerName) {};
		virtual void OnUse(Entity* self, Entity* user) {};
		virtual void OnDie(Entity* self, Entity* killer) {};
		virtual void OnHit(Entity* self, Entity* attacker) {};
		virtual void OnEmoteReceived(Entity* self, int32_t emote, Entity* target) {};
		virtual void OnRebuildStart(Entity* self, Entity* target) {};
		virtual void OnRebuildNotifyState(Entity* self, eRebuildState state) {};
		virtual void OnRebuildComplete(Entity* self, Entity* target) {};
		virtual void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {};
		virtual void OnRespondToMission(Entity* self, int missionID, Entity* player, int reward) {};
		virtual void OnUpdate(Entity* self) {};
		virtual void OnZonePropertyRented(Entity* self, Entity* renter) {};
		virtual void OnZonePropertyEditBegin(Entity* self) {};
		virtual void OnZonePropertyEditEnd(Entity* self) {};
		virtual void OnZonePropertyModelEquipped(Entity* self) {};
		virtual void OnZonePropertyModelPlaced(Entity* self, Entity* player) {};
		virtual void OnZonePropertyModelPickedUp(Entity* self, Entity* player) {};
		virtual void OnZonePropertyModelRemoved(Entity* self, Entity* player) {};
		virtual void OnZonePropertyModelRemovedWhileEquipped(Entity* self, Entity* player) {};
		virtual void OnZonePropertyModelRotated(Entity* self, Entity* player) {};
		virtual void OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, eNotifyType type) {};
		virtual void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {};
		virtual void OnChoiceBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) {};
		virtual void OnWaypointReached(Entity* self, uint32_t waypointIndex) {};
		virtual void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {};
		virtual void OnSkillCast(Entity* self, uint32_t skillID) {};
		virtual void OnPlayerRailArrived(Entity* self, Entity* sender, const std::u16string& pathName, int32_t waypoint) {};
        virtual void OnActivityStateChangeRequest(Entity* self, const LWOOBJID senderID, const int32_t value1, const int32_t value2, const std::u16string& stringValue) {};

		virtual void OnCinematicUpdate(Entity* self, Entity* sender, eCinematicEvent event, const std::u16string& pathName, float_t pathTime, float_t totalTime, int32_t waypoint) {};
    */

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