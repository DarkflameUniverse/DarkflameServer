#pragma once

#ifndef __MISSIONSTATE__H__
#define __MISSIONSTATE__H__

/**
 * Represents the possible states a mission can be in
 */
enum class MissionState : int {
	/**
	 * The mission state is unknown
	 */
	MISSION_STATE_UNKNOWN = -1,

	/**
	 * The mission is yielding rewards
	 */
	 MISSION_STATE_REWARDING = 0,

	 /**
	  * The mission can be accepted
	  */
	  MISSION_STATE_AVAILABLE = 1,

	  /**
	   * The mission has been accepted but not yet completed
	   */
	   MISSION_STATE_ACTIVE = 2,

	   /**
		* All the tasks for the mission have been completed and the entity can turn the mission in to complete it
		*/
		MISSION_STATE_READY_TO_COMPLETE = 4,            //!< The mission is ready to complete

		/**
		 * The mission has been completed
		 */
		 MISSION_STATE_COMPLETE = 8,

		 /**
		  * The mission is available again and has been completed before. Used for daily missions.
		  */
		  MISSION_STATE_COMPLETE_AVAILABLE = 9,

		  /**
		   * The mission is active and has been completed before. Used for daily missions.
		   */
		   MISSION_STATE_COMPLETE_ACTIVE = 10,

		   /**
			* The mission has been completed before and has now been completed again. Used for daily missions.
			*/
			MISSION_STATE_COMPLETE_READY_TO_COMPLETE = 12
};

#endif  //!__MISSIONSTATE__H__
