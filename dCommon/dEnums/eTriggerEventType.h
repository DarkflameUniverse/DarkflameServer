#ifndef __ETRIGGEREVENTTYPE__H__
#define __ETRIGGEREVENTTYPE__H__

enum class eTriggerEventType {
	INVALID,
	DESTROY,
	CUSTOMEVENT,
	ENTER,
	EXIT,
	CREATE,
	HIT,
	TIMERDONE,
	REBUILDCOMPLETE,
	ACTIVATED,
	DEACTIVATED,
	ARRIVED,
	ARRIVEDATENDOFPATH,
	ZONESUMMARYDISMISSED,
	ARRIVEDATDESIREDWAYPOINT,
	PETONSWITCH,
	PETOFFSWITCH,
	INTERACT
};

class TriggerEventType {
public:
	static eTriggerEventType StringToTriggerEventType(std::string commandString) {
		const std::map<std::string, eTriggerEventType> TriggerEventMap = {
			{"OnDestroy", eTriggerEventType::DESTROY},
			{"OnCustomEvent", eTriggerEventType::CUSTOMEVENT},
			{"OnEnter", eTriggerEventType::ENTER},
			{"OnExit", eTriggerEventType::EXIT},
			{"OnCreate", eTriggerEventType::CREATE},
			{"OnHit", eTriggerEventType::HIT},
			{"OnTimerDone", eTriggerEventType::TIMERDONE},
			{"OnRebuildComplete", eTriggerEventType::REBUILDCOMPLETE},
			{"OnActivated", eTriggerEventType::ACTIVATED},
			{"OnDeactivated", eTriggerEventType::DEACTIVATED},
			{"OnArrived", eTriggerEventType::ARRIVED},
			{"OnArrivedAtEndOfPath", eTriggerEventType::ARRIVEDATENDOFPATH},
			{"OnZoneSummaryDismissed", eTriggerEventType::ZONESUMMARYDISMISSED},
			{"OnArrivedAtDesiredWaypoint", eTriggerEventType::ARRIVEDATDESIREDWAYPOINT},
			{"OnPetOnSwitch", eTriggerEventType::PETONSWITCH},
			{"OnPetOffSwitch", eTriggerEventType::PETOFFSWITCH},
			{"OnInteract", eTriggerEventType::INTERACT},
		};

		auto intermed = TriggerEventMap.find(commandString);
		return (intermed != TriggerEventMap.end()) ? intermed->second : eTriggerEventType::INVALID;
	};
};

#endif  //!__ETRIGGEREVENTTYPE__H__
