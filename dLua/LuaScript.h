#include "dLua.h"

#include "CppScripts.h"
#include "Game.h"
#include "dLogger.h"
#include "LuaTerminateBehavior.h"
#include "lEntity.h"

class Entity;

class LuaScript : public CppScripts::Script
{
private:
    sol::state m_State;

    LuaTerminateBehavior m_TerminateBehavior;

    int32_t m_Hooks;

    bool m_Terminating;

    bool m_AttemptingToFinalize;

    void CheckHooks();

    void AttemptToFinalize();

public:
    LuaScript(LuaTerminateBehavior terminateBehavior);

    sol::state& GetState();

    void Script(const std::string& script);

    void SetEntity(Entity* entity);

    Entity* GetEntity();

    /**
     * Add a callback timer.
     */
    void AddCallbackTimer(float seconds, sol::function callback);

    template <typename F, typename ...Types>
    void Invoke(F function, Types... args)
    {
        sol::function func = m_State[function];

        if (!func.valid())
        {
            return;
        }

        func(std::forward<Types>(args)...);
    }

    // ======================================================================
    // CppScripts::Script overrides
    // ======================================================================

    /**
     * Invoked one frame after the script is loaded.
     * 
     * Equivalent to 'function onStartup(self)'
     */
    void OnStartup(Entity* self) override {
        Invoke("onStartup");
    }

    /**
     * Invoked upon an entity entering the phantom collider on self.
     * 
     * Equivalent to 'function onCollisionPhantom(self, msg)'
     */
    void OnCollisionPhantom(Entity* self, Entity* target) override {
        Invoke("onCollisionPhantom", lEntity(target));
    }

    /**
     * Invoked when a player accepted a mission.
     * 
     * Equivalent to 'function onMissionDialogueOK(self, msg)'
     */
    void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override { 
        Invoke("onMissionDialogueOK", lEntity(target), missionID, (int32_t) missionState);
    }

    /**
     * Invoked when the client or the server invoked an event server-side.
     * 
     * Equivalent to 'function onFireEventServerSide(self, msg)'
     */
    void OnFireEventServerSide(Entity *self, Entity *sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override {
        Invoke("onFireEventServerSide", lEntity(sender), args, param1, param2, param3);
    }

    /**
     * Invoked upon sending a object notification.
     * 
     * Equivalent to 'function onNotifyObject(self, msg)'
     */
    void OnNotifyObject(Entity *self, Entity *sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0) override {
        Invoke("onNotifyObject", lEntity(sender), name, param1, param2);
    }
    
    /**
     * Invoked upon a player exiting the modular build minigame.
     * 
     * Equivalent to 'function onModularBuildExit(self, msg)'
     */
    void OnModularBuildExit(Entity* self, Entity* player, bool bCompleted, std::vector<LOT> modules) override {
        Invoke("onModularBuildExit", lEntity(player), bCompleted, modules);
    }
    
    /**
     * Invoked when a player has loaded into the zone.
     * 
     * Equivalent to 'function onPlayerLoaded(self, msg)'
     */
    void OnPlayerLoaded(Entity* self, Entity* player) override {
        Invoke("onPlayerLoaded", lEntity(player));
    }

    /**
     * Invoked when a player has died.
     * 
     * Equivalent to 'function onPlayerDied(self, msg)'
     */
    void OnPlayerDied(Entity* self, Entity* player) override {
        Invoke("onPlayerDied", lEntity(player));
    }

    /**
     * Invoked when a player has resurrected.
     * 
     * Equivalent to 'function onPlayerResurrected(self, msg)'
     */
    void OnPlayerResurrected(Entity* self, Entity* player) override {
        Invoke("onPlayerResurrected", lEntity(player));
    }

    /**
     * Invoked when a player has left the zone.
     * 
     * Equivalent to 'function onPlayerExit(self, msg)'
     */
    void OnPlayerExit(Entity* self, Entity* player) override {
        Invoke("onPlayerExit", lEntity(player));
    }

    /**
     * Invoked when a player has interacted with the proximity collider on self.
     * 
     * Equivalent to 'function onProximityUpdate(self, msg)'
     * 
     * @param name  The name of the proximity collider recviving an interaction.
     * @param status "ENTER" if a player has entered the proximity collider; "LEAVE" if a player has left the proximity collider
     */
    void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override {
        Invoke("onProximityUpdate", lEntity(entering), name, status);
    }

    /**
     * Invoked when a timer on self has expired.
     * 
     * Equivalent to 'function onTimerDone(self, msg)'
     */
    void OnTimerDone(Entity* self, std::string timerName) override {
        Invoke("onTimerDone", timerName);
    }

    /**
     * Invoked when a player interactions with self.
     * 
     * Equivalent to 'function onUse(self, msg)'
     */
    void OnUse(Entity* self, Entity* user) override {
        Invoke("onUse", lEntity(user));
    }

    /**
     * Invoked when self has died.
     * 
     * Equivalent to 'function onDie(self, msg)'
     */
    void OnDie(Entity* self, Entity* killer) override {
        Invoke("onDie", lEntity(killer));
    }

    /**
     * Invoked when self has received a hit.
     * 
     * Equivalent to 'function onHit(self, msg)'
     */
    void OnHit(Entity* self, Entity* attacker) override {
        Invoke("onHit", lEntity(attacker));
    }

    /**
     * Invoked when self has received an emote from a player.
     * 
     * Equivalent to 'function onEmoteReceived(self, msg)'
     */
    void OnEmoteReceived(Entity* self, int32_t emote, Entity* target) override {
        Invoke("onEmoteReceived", emote, lEntity(target));
    }

    /**
     * Invoked when a player has started building this quickbuild.
     * 
     * Equivalent to 'function onRebuildStart(self, msg)'
     */
    void OnRebuildStart(Entity* self, Entity* target) override {
        Invoke("onRebuildStart", lEntity(target));
    }
    
    /**
     * Invoked when this quickbuild has changed state.
     * 
     * Equivalent to 'function onRebuildNotifyState(self, msg)'
     */
    void OnRebuildNotifyState(Entity* self, eRebuildState state) override {
        Invoke("onRebuildNotifyState", (int32_t) state);
    }

    /**
     * Invoked when this quickbuild has been completed.
     * 
     * Equivalent to 'function onRebuildComplete(self, msg)'
     */
    void OnRebuildComplete(Entity* self, Entity* target) override {
        Invoke("onRebuildComplete", lEntity(target));
    }

    /**
     * Invoked when self has received either a hit or heal.
     * 
     * Equivalent to 'function onHitOrHealResult(self, msg)'
     */
    void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override {
        Invoke("onHitOrHealResult", lEntity(attacker), damage);
    }
    
    /**
     * Invoked when a player has responsed to a mission.
     * 
     * Equivalent to 'function onRespondToMission(self, msg)'
     */
    void OnRespondToMission(Entity* self, int missionID, Entity* player, int reward) override {
        Invoke("onRespondToMission", missionID, lEntity(player), reward);
    }
    
    /**
     * Invoked once per frame.
     * 
     * No LUA eqivalent.
     */
    void OnUpdate(Entity* self) override {
        Invoke("onUpdate");
    }
    
    /**
     * Invoked when this property has been rented.
     * 
     * Equivalent to 'function onZonePropertyRented(self, msg)'
     */
    void OnZonePropertyRented(Entity* self, Entity* renter) override {
        Invoke("onZonePropertyRented", lEntity(renter));
    }

    /**
     * Invoked when a player has begun to edit this property.
     * 
     * Equivalent to 'function onZonePropertyEditBegin(self, msg)'
     */
    void OnZonePropertyEditBegin(Entity* self) override {
        Invoke("onZonePropertyEditBegin");
    }

    /**
     * Invoked when a player has concluded editing this property.
     * 
     * Equivalent to 'function onZonePropertyEditEnd(self, msg)'
     */
    void OnZonePropertyEditEnd(Entity* self) override {
        Invoke("onZonePropertyEditEnd");
    }

    /**
     * Invoked when a player has equipped a model while editing this property.
     * 
     * Equivalent to 'function onZonePropertyModelEquipped(self, msg)'
     */
    void OnZonePropertyModelEquipped(Entity* self) override {
        Invoke("onZonePropertyModelEquipped");
    }

    /**
     * Invoked when a player has placed a model while editing this property.
     * 
     * Equivalent to 'function onZonePropertyModelPlaced(self, msg)'
     */
    void OnZonePropertyModelPlaced(Entity* self, Entity* player) override {
        Invoke("onZonePropertyModelPlaced", lEntity(player));
    }

    /**
     * Invoked when a player has picked up a model while editing this property.
     * 
     * Equivalent to 'function onZonePropertyModelPickedUp(self, msg)'
     */
    void OnZonePropertyModelPickedUp(Entity* self, Entity* player) override {
        Invoke("onZonePropertyModelPickedUp", lEntity(player));
    }

    /**
     * Invoked when a player removed a model while editing this property.
     * 
     * Equivalent to 'function onZonePropertyModelRemoved(self, msg)'
     */
    void OnZonePropertyModelRemoved(Entity* self, Entity* player) override {
        Invoke("onZonePropertyModelRemoved", lEntity(player));
    }

    /**
     * Invoked when a player removed a model while holding it when editing this property.
     * 
     * Equivalent to 'function onZonePropertyModelRemoved(self, msg)'
     */
    void OnZonePropertyModelRemovedWhileEquipped(Entity* self, Entity* player) override {
        Invoke("onZonePropertyModelRemovedWhileEquipped", lEntity(player));
    }
    
    /**
     * Invoked when a player rotated a model while editing this property.
     * 
     * Equivalent to 'function onZonePropertyModelRotated(self, msg)'
     */
    void OnZonePropertyModelRotated(Entity* self, Entity* player) override {
        Invoke("onZonePropertyModelRotated", lEntity(player));
    }

    /**
     * Invoked when the pet taming minigame encounted an event.
     * 
     * Equivalent to 'function onNotifyPetTamingMinigame(self, msg)'
     */
    void OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, eNotifyType type) override {
        Invoke("onNotifyPetTamingMinigame", lEntity(tamer), (int32_t) type);
    }
    
    /**
     * Invoked when a player responded to a message box.
     * 
     * Equivalent to 'function onMessageBoxResponse(self, msg)'
     */
    void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) override {
        Invoke("onMessageBoxResponse", lEntity(sender), button, GeneralUtils::UTF16ToWTF8(identifier), GeneralUtils::UTF16ToWTF8(userData));
    }
    
    /**
     * Invoked when a player responded to a choice box.
     * 
     * Equivalent to 'function onChoiceBoxResponse(self, msg)'
     */
    void OnChoiceBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) override {
        Invoke("onChoiceBoxResponse", lEntity(sender), button, GeneralUtils::UTF16ToWTF8(buttonIdentifier), GeneralUtils::UTF16ToWTF8(identifier));
    }
    
    /**
     * Invoked when self arrived at a moving platform waypoint.
     * 
     * Equivalent to 'function onWaypointReached(self, msg)'
     */
    void OnWaypointReached(Entity* self, uint32_t waypointIndex) override {
        Invoke("onWaypointReached", waypointIndex);
    }
    
    /**
     * Invoked when a player fired a skill event on self.
     * 
     * Equivalent to 'function onSkillEventFired(self, msg)'
     */
    void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override {
        Invoke("onSkillEventFired", lEntity(caster), message);
    }
    
    /**
     * Invoked when self casted a skill.
     * 
     * Equivalent to 'function onSkillCast(self, msg)'
     */
    void OnSkillCast(Entity* self, uint32_t skillID) override {
        Invoke("onSkillCast", skillID);
    }

    /**
     * Invoked when a player on a rail reaches a waypoint
     * Equivalent to: 'onPlayerRailArrivedNotification(self, msg)'
     * @param self the parent of the script
     * @param sender the entity that sent the event
     * @param pathName the name of the path the entity was on
     * @param waypoint the waypoint number of the path the entity was on
     */
    void OnPlayerRailArrived(Entity* self, Entity* sender, const std::u16string& pathName, int32_t waypoint) override {
        Invoke("onPlayerRailArrived", lEntity(sender), GeneralUtils::UTF16ToWTF8(pathName), waypoint);
    }

    /**
     * Used by legacy minigames to indicate something has changed about the activity
     * @param self the entity the script belongs to
     * @param senderID the sender of the message
     * @param value1 some value to represent the change
     * @param value2 some other value to represent the change
     * @param stringValue some string value to represent the change
     */
    void OnActivityStateChangeRequest(Entity* self, const LWOOBJID senderID, const int32_t value1,
                                                const int32_t value2, const std::u16string& stringValue) override {
        Invoke("onActivityStateChangeRequest", senderID, value1, value2, GeneralUtils::UTF16ToWTF8(stringValue));
    }

    void OnCinematicUpdate(Entity* self, Entity* sender, eCinematicEvent event, const std::u16string& pathName,
                                    float_t pathTime, float_t totalTime, int32_t waypoint) override {
        Invoke("onCinematicUpdate", lEntity(sender), (int32_t) event, GeneralUtils::UTF16ToWTF8(pathName), pathTime, totalTime, waypoint);
    }
};
