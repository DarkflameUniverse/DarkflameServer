#ifndef __WORLDCONFIG__H__
#define __WORLDCONFIG__H__

#include <cstdint>
#include <string>

struct WorldConfig {
	int32_t worldConfigID{};						//! Primary key for WorldConfig table
	float peGravityValue{};							//! Unknown
	float peBroadphaseWorldSize{};					//! Unknown
	float peGameObjScaleFactor{};					//! Unknown
	float characterRotationSpeed{};					//! The players' rotation speed
	float characterWalkForwardSpeed{};				//! The players' walk forward speed
	float characterWalkBackwardSpeed{};				//! The players' walk backwards speed
	float characterWalkStrafeSpeed{};				//! The players' strafe speed
	float characterWalkStrafeForwardSpeed{};		//! The players' walk strafe forward speed
	float characterWalkStrafeBackwardSpeed{};		//! The players' walk strage backwards speed
	float characterRunBackwardSpeed{};				//! The players' run backwards speed
	float characterRunStrafeSpeed{};				//! The players' run strafe speed
	float characterRunStrafeForwardSpeed{};			//! The players' run strafe forward speed
	float characterRunStrafeBackwardSpeed{};		//! The players' run strage backwards speed
	float globalCooldown{};							//! The global ability cooldown
	float characterGroundedTime{};					//! Unknown
	float characterGroundedSpeed{};					//! Unknown
	float globalImmunityTime{};						//! Unknown
	float characterMaxSlope{};						//! Unknown
	float defaultRespawnTime{};						//! Unknown
	float missionTooltipTimeout{};
	float vendorBuyMultiplier{};					//! The buy scalar for buying from vendors
	float petFollowRadius{};						//! The players' pet follow radius
	float characterEyeHeight{};						//! The players' eye height
	float flightVerticalVelocity{};					//! Unknown
	float flightAirspeed{};							//! Unknown
	float flightFuelRatio{};						//! Unknown
	float flightMaxAirspeed{};						//! Unknown
	float fReputationPerVote{};						//! Unknown
	int32_t propertyCloneLimit{};					//! Unknown
	int32_t defaultHomespaceTemplate{};				//! Unknown
	float coinsLostOnDeathPercent{};				//! The percentage of coins to lose on a player death
	int32_t coinsLostOnDeathMin{};					//! The minimum number of coins to lose on a player death
	int32_t coinsLostOnDeathMax{};					//! The maximum number of coins to lose on a player death
	int32_t characterVotesPerDay{};					//! Unknown
	int32_t propertyModerationRequestApprovalCost{};//! Unknown
	int32_t propertyModerationRequestReviewCost{};	//! Unknown
	int32_t propertyModRequestsAllowedSpike{};		//! Unknown
	int32_t propertyModRequestsAllowedInterval{};	//! Unknown
	int32_t propertyModRequestsAllowedTotal{};		//! Unknown
	int32_t propertyModRequestsSpikeDuration{};		//! Unknown
	int32_t propertyModRequestsIntervalDuration{};	//! Unknown
	bool modelModerateOnCreate{};					//! Unknown
	float defaultPropertyMaxHeight{};				//! Unknown
	float reputationPerVoteCast{};					//! Unknown
	float reputationPerVoteReceived{};				//! Unknown
	int32_t showcaseTopModelConsiderationBattles{};	//! Unknown
	float reputationPerBattlePromotion{};			//! Unknown
	float coinsLostOnDeathMinTimeout{};				//! Unknown
	float coinsLostOnDeathMaxTimeout{};				//! Unknown
	int32_t mailBaseFee{};							//! The base fee to take when a player sends mail
	float mailPercentAttachmentFee{};				//! The scalar multiplied by an items base cost to determine how much that item costs to be mailed
	int32_t propertyReputationDelay{};				//! Unknown
	int32_t levelCap{};								//! The maximum player level
	std::string levelUpBehaviorEffect{};			//! Unknown
	int32_t characterVersion{};						//! Unknown
	int32_t levelCapCurrencyConversion{};			//! The ratio of UScore (LEGO Score) to coins
};

#endif //! __WORLDCONFIG__H__
