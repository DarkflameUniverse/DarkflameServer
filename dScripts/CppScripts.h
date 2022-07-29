#pragma once
#include "dCommonVars.h"
#include "MissionState.h"
#include <string>
#include <vector>

class User;
class Entity;
class NiPoint3;

namespace CppScripts {
	/**
	 * Base class for all scripts. Includes virtual methods to be overridden to handle LUA equivelent events.
	 *
	 * All methods pass 'self' as the first parameter, this is the associated parent entity for the event.
	 * There will only ever be one instance of each script.
	 *
	 * Do not use class members as entity specific variables unless you're sure there will only event be one instance of this script.
	 * Do use class members as script wide variables, variables all entities which this script will access.
	 *
	 * Use self->GetVar<type_t>(u"variable_name") and self->SetVar<type_t>(u"variable_name", value) to manage variables.
	 *
	 * Designed to yield as close to a 1:1 mapping as possible with LUA.
	 * There will be events which are not implemented or inheritetly LUA features not easily translated to C++.
	 * Most of the time these can be worked around or ignored.
	 */
	class Script {
	public:
		Script();
		~Script();

		/**
		 * Invoked one frame after the script is loaded.
		 *
		 * Equivalent to 'function onStartup(self)'
		 */
		virtual void OnStartup(Entity* self) {};

		/**
		 * Invoked upon an entity entering the phantom collider on self.
		 *
		 * Equivalent to 'function onCollisionPhantom(self, msg)'
		 */
		virtual void OnCollisionPhantom(Entity* self, Entity* target) {};

		/**
		 * Invoked when a player accepted a mission.
		 *
		 * Equivalent to 'function onMissionDialogueOK(self, msg)'
		 */
		virtual void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {};

		/**
		 * Invoked when the client or the server invoked an event server-side.
		 *
		 * Equivalent to 'function onFireEventServerSide(self, msg)'
		 */
		virtual void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {};

		/**
		 * Invoked upon sending a object notification.
		 *
		 * Equivalent to 'function onNotifyObject(self, msg)'
		 */
		virtual void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0) {};

		/**
		 * Invoked upon a player exiting the modular build minigame.
		 *
		 * Equivalent to 'function onModularBuildExit(self, msg)'
		 */
		virtual void OnModularBuildExit(Entity* self, Entity* player, bool bCompleted, std::vector<LOT> modules) {};

		/**
		 * Invoked when a player has loaded into the zone.
		 *
		 * Equivalent to 'function onPlayerLoaded(self, msg)'
		 */
		virtual void OnPlayerLoaded(Entity* self, Entity* player) {};

		/**
		 * Invoked when a player has died.
		 *
		 * Equivalent to 'function onPlayerDied(self, msg)'
		 */
		virtual void OnPlayerDied(Entity* self, Entity* player) {};

		/**
		 * Invoked when a player has resurrected.
		 *
		 * Equivalent to 'function onPlayerResurrected(self, msg)'
		 */
		virtual void OnPlayerResurrected(Entity* self, Entity* player) {};

		/**
		 * Invoked when a player has left the zone.
		 *
		 * Equivalent to 'function onPlayerExit(self, msg)'
		 */
		virtual void OnPlayerExit(Entity* self, Entity* player) {};

		/**
		 * Invoked when a player has interacted with the proximity collider on self.
		 *
		 * Equivalent to 'function onProximityUpdate(self, msg)'
		 *
		 * @param name  The name of the proximity collider recviving an interaction.
		 * @param status "ENTER" if a player has entered the proximity collider; "LEAVE" if a player has left the proximity collider
		 */
		virtual void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {};

		/**
		 * Invoked when a timer on self has expired.
		 *
		 * Equivalent to 'function onTimerDone(self, msg)'
		 */
		virtual void OnTimerDone(Entity* self, std::string timerName) {};

		/**
		 * Invoked when a player interactions with self.
		 *
		 * Equivalent to 'function onUse(self, msg)'
		 */
		virtual void OnUse(Entity* self, Entity* user) {};

		/**
		 * Invoked when self has died.
		 *
		 * Equivalent to 'function onDie(self, msg)'
		 */
		virtual void OnDie(Entity* self, Entity* killer) {};

		/**
		 * Invoked when self has received a hit.
		 *
		 * Equivalent to 'function onHit(self, msg)'
		 */
		virtual void OnHit(Entity* self, Entity* attacker) {};

		/**
		 * Invoked when self has received an emote from a player.
		 *
		 * Equivalent to 'function onEmoteReceived(self, msg)'
		 */
		virtual void OnEmoteReceived(Entity* self, int32_t emote, Entity* target) {};

		/**
		 * Invoked when a player has started building this quickbuild.
		 *
		 * Equivalent to 'function onRebuildStart(self, msg)'
		 */
		virtual void OnRebuildStart(Entity* self, Entity* target) {};

		/**
		 * Invoked when this quickbuild has changed state.
		 *
		 * Equivalent to 'function onRebuildNotifyState(self, msg)'
		 */
		virtual void OnRebuildNotifyState(Entity* self, eRebuildState state) {};

		/**
		 * Invoked when this quickbuild has been completed.
		 *
		 * Equivalent to 'function onRebuildComplete(self, msg)'
		 */
		virtual void OnRebuildComplete(Entity* self, Entity* target) {};

		/**
		 * Invoked when self has received either a hit or heal.
		 *
		 * Equivalent to 'function onHitOrHealResult(self, msg)'
		 */
		virtual void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {};

		/**
		 * Invoked when a player has responsed to a mission.
		 *
		 * Equivalent to 'function onRespondToMission(self, msg)'
		 */
		virtual void OnRespondToMission(Entity* self, int missionID, Entity* player, int reward) {};

		/**
		 * Invoked once per frame.
		 *
		 * No LUA eqivalent.
		 */
		virtual void OnUpdate(Entity* self) {};

		/**
		 * Invoked when this property has been rented.
		 *
		 * Equivalent to 'function onZonePropertyRented(self, msg)'
		 */
		virtual void OnZonePropertyRented(Entity* self, Entity* renter) {};

		/**
		 * Invoked when a player has begun to edit this property.
		 *
		 * Equivalent to 'function onZonePropertyEditBegin(self, msg)'
		 */
		virtual void OnZonePropertyEditBegin(Entity* self) {};

		/**
		 * Invoked when a player has concluded editing this property.
		 *
		 * Equivalent to 'function onZonePropertyEditEnd(self, msg)'
		 */
		virtual void OnZonePropertyEditEnd(Entity* self) {};

		/**
		 * Invoked when a player has equipped a model while editing this property.
		 *
		 * Equivalent to 'function onZonePropertyModelEquipped(self, msg)'
		 */
		virtual void OnZonePropertyModelEquipped(Entity* self) {};

		/**
		 * Invoked when a player has placed a model while editing this property.
		 *
		 * Equivalent to 'function onZonePropertyModelPlaced(self, msg)'
		 */
		virtual void OnZonePropertyModelPlaced(Entity* self, Entity* player) {};

		/**
		 * Invoked when a player has picked up a model while editing this property.
		 *
		 * Equivalent to 'function onZonePropertyModelPickedUp(self, msg)'
		 */
		virtual void OnZonePropertyModelPickedUp(Entity* self, Entity* player) {};

		/**
		 * Invoked when a player removed a model while editing this property.
		 *
		 * Equivalent to 'function onZonePropertyModelRemoved(self, msg)'
		 */
		virtual void OnZonePropertyModelRemoved(Entity* self, Entity* player) {};

		/**
		 * Invoked when a player removed a model while holding it when editing this property.
		 *
		 * Equivalent to 'function onZonePropertyModelRemoved(self, msg)'
		 */
		virtual void OnZonePropertyModelRemovedWhileEquipped(Entity* self, Entity* player) {};

		/**
		 * Invoked when a player rotated a model while editing this property.
		 *
		 * Equivalent to 'function onZonePropertyModelRotated(self, msg)'
		 */
		virtual void OnZonePropertyModelRotated(Entity* self, Entity* player) {};

		/**
		 * Invoked when the pet taming minigame encounted an event.
		 *
		 * Equivalent to 'function onNotifyPetTamingMinigame(self, msg)'
		 */
		virtual void OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, eNotifyType type) {};

		/**
		 * Invoked when a player responded to a message box.
		 *
		 * Equivalent to 'function onMessageBoxResponse(self, msg)'
		 */
		virtual void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {};

		/**
		 * Invoked when a player responded to a choice box.
		 *
		 * Equivalent to 'function onChoiceBoxResponse(self, msg)'
		 */
		virtual void OnChoiceBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) {};

		/**
		 * Invoked when self arrived at a moving platform waypoint.
		 *
		 * Equivalent to 'function onWaypointReached(self, msg)'
		 */
		virtual void OnWaypointReached(Entity* self, uint32_t waypointIndex) {};

		/**
		 * Invoked when a player fired a skill event on self.
		 *
		 * Equivalent to 'function onSkillEventFired(self, msg)'
		 */
		virtual void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {};

		/**
		 * Invoked when self casted a skill.
		 *
		 * Equivalent to 'function onSkillCast(self, msg)'
		 */
		virtual void OnSkillCast(Entity* self, uint32_t skillID) {};

		/**
		 * Invoked when a player on a rail reaches a waypoint
		 * Equivalent to: 'onPlayerRailArrivedNotification(self, msg)'
		 * @param self the parent of the script
		 * @param sender the entity that sent the event
		 * @param pathName the name of the path the entity was on
		 * @param waypoint the waypoint number of the path the entity was on
		 */
		virtual void OnPlayerRailArrived(Entity* self, Entity* sender, const std::u16string& pathName, int32_t waypoint) {};

		/**
		 * Used by legacy minigames to indicate something has changed about the activity
		 * @param self the entity the script belongs to
		 * @param senderID the sender of the message
		 * @param value1 some value to represent the change
		 * @param value2 some other value to represent the change
		 * @param stringValue some string value to represent the change
		 */
		virtual void OnActivityStateChangeRequest(Entity* self, const LWOOBJID senderID, const int32_t value1,
			const int32_t value2, const std::u16string& stringValue) {
		};

		virtual void OnCinematicUpdate(Entity* self, Entity* sender, eCinematicEvent event, const std::u16string& pathName,
			float_t pathTime, float_t totalTime, int32_t waypoint) {
		};
	};

	Script* GetScript(Entity* parent, const std::string& scriptName);
	std::vector<Script*> GetEntityScripts(Entity* entity);
};
