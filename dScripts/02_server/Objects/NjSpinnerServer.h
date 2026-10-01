#pragma once
#include "CppScripts.h"

#include <string>

/**
 * L_NJ_SPINNER_SERVER and its 40 L_NJ_<element>_<kind>_SPINNER scripts: the Ninjago spinner traps. A spinjitzu user
 * (the element's precondition) or an event moves the spinner (a simple mover, MovingPlatforms rows 1-40) to its other
 * end: up is active (a blade spinner hurts players near it with skill 971, an arrow one shoots skill 972 every
 * skill_pulse_time, a spawn one activates its spawner networks), down is inactive. The variant scripts only pick
 * defaults for type, element_type and the audio.
 * Not as live: the proximities are spheres (live: cylinders of a height), and use is checked when used rather than
 * by the client's use requirements.
 */
class NjSpinnerServer : public CppScripts::Script {
public:
	struct Defaults {
		std::u16string type;
		std::u16string elementType;
		std::string audioMovementBegin;
		std::string audioMovementEnd;
		std::string audioMovement;
		std::string audioSpinning;
	};

	NjSpinnerServer() = default;
	explicit NjSpinnerServer(Defaults defaults) : m_Defaults(std::move(defaults)) {}

	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;
	void OnWaypointReached(Entity* self, uint32_t waypointIndex) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	struct Element {
		std::string capFX;
		int32_t precondition;
	};
	static Element GetElement(Entity* self);

	void SetCapFX(Entity* self, bool on);
	void PlayAnim(Entity* self, Entity* player, const std::string& animation);
	void SetActiveState(Entity* self, bool active, bool playerTriggered);
	void ToggleActive(Entity* self, bool playerTriggered);
	void ToggleSpawnerNetwork(Entity* self, bool active);
	void OnSpawnedDied(Entity* self, const std::string& network);
	void PlayCinematic(Entity* self, bool on);
	void CastSkill(Entity* self);
	void ToggleSpinner(Entity* self, bool playerTriggered);
	void ActivateSpinner(Entity* self, bool activate, bool playerTriggered);
	void ToggleDamageProx(Entity* self, bool on);
	void SendEvent(Entity* self, bool on);

	std::string Audio(Entity* self, const std::u16string& var, const std::string& variantDefault, const std::string& baseDefault) const;

	Defaults m_Defaults;
};
