#ifndef __ETRIGGEREVENTTYPE__H__
#define __ETRIGGEREVENTTYPE__H__

enum class eTriggerEventType {
	INVALID,
	DESTROY,
	CUSTOM_EVENT,
	ENTER,
	EXIT,
	CREATE,
	HIT,
	TIMER_DONE,
	REBUILD_COMPLETE,
	ACTIVATED,
	DEACTIVATED,
	ARRIVED,
	ARRIVED_AT_END_OF_PATH,
	ZONE_SUMMARY_DISMISSED,
	ARRIVED_AT_DESIRED_WAYPOINT,
	PET_ON_SWITCH,
	PET_OFF_SWITCH,
	INTERACT
};

class TriggerEventType {
public:
	static eTriggerEventType StringToTriggerEventType(std::string commandString) {
		const std::map<std::string, eTriggerEventType> TriggerEventMap = {
			{"OnDestroy", eTriggerEventType::DESTROY},
			{"OnCustomEvent", eTriggerEventType::CUSTOM_EVENT},
			{"OnEnter", eTriggerEventType::ENTER},
			{"OnExit", eTriggerEventType::EXIT},
			{"OnCreate", eTriggerEventType::CREATE},
			{"OnHit", eTriggerEventType::HIT},
			{"OnTimerDone", eTriggerEventType::TIMER_DONE},
			{"OnRebuildComplete", eTriggerEventType::REBUILD_COMPLETE},
			{"OnActivated", eTriggerEventType::ACTIVATED},
			{"OnDeactivated", eTriggerEventType::DEACTIVATED},
			{"OnArrived", eTriggerEventType::ARRIVED},
			{"OnArrivedAtEndOfPath", eTriggerEventType::ARRIVED_AT_END_OF_PATH},
			{"OnZoneSummaryDismissed", eTriggerEventType::ZONE_SUMMARY_DISMISSED},
			{"OnArrivedAtDesiredWaypoint", eTriggerEventType::ARRIVED_AT_DESIRED_WAYPOINT},
			{"OnPetOnSwitch", eTriggerEventType::PET_ON_SWITCH},
			{"OnPetOffSwitch", eTriggerEventType::PET_OFF_SWITCH},
			{"OnInteract", eTriggerEventType::INTERACT},
		};

		auto intermed = TriggerEventMap.find(commandString);
		return (intermed != TriggerEventMap.end()) ? intermed->second : eTriggerEventType::INVALID;
	};
};

#endif  //!__ETRIGGEREVENTTYPE__H__
