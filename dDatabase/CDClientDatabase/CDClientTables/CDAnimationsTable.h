#pragma once

#include "CDTable.h"
#include <list>

struct CDAnimation {
	// unsigned int animationGroupID;
	// std::string animation_type;
	// The above two are a pair to represent a primary key in the map.
	std::string animation_name;        //!< The animation name
	float chance_to_play;           //!< The chance to play the animation
	UNUSED_COLUMN(unsigned int min_loops;)                 //!< The minimum number of loops
	UNUSED_COLUMN(unsigned int max_loops;)                 //!< The maximum number of loops
	float animation_length;         //!< The animation length
	UNUSED_COLUMN(bool hideEquip;)                 //!< Whether or not to hide the equip
	UNUSED_COLUMN(bool ignoreUpperBody;)           //!< Whether or not to ignore the upper body
	UNUSED_COLUMN(bool restartable;)               //!< Whether or not the animation is restartable
	UNUSED_COLUMN(std::string face_animation_name;)   //!< The face animation name
	UNUSED_COLUMN(float priority;)                 //!< The priority
	UNUSED_COLUMN(float blendTime;)                //!< The blend time
};

typedef LookupResult<CDAnimation> CDAnimationLookupResult;

class CDAnimationsTable : public CDTable<CDAnimationsTable> {
	typedef int32_t AnimationGroupID;
	typedef std::string AnimationID;
	typedef std::pair<std::string, AnimationGroupID> CDAnimationKey;
public:
	void LoadValuesFromDatabase();
	/**
	 * Given an animationType and the previousAnimationName played, return the next animationType to play.
	 * If there are more than 1 animationTypes that can be played, one is selected at random but also does not allow
	 * the previousAnimationName to be played twice.
	 * 
	 * @param animationType The animationID to lookup
	 * @param previousAnimationName The previously played animation
	 * @param animationGroupID The animationGroupID to lookup
	 * @return CDAnimationLookupResult 
	 */
	[[nodiscard]] CDAnimationLookupResult GetAnimation(const AnimationID& animationType, const std::string& previousAnimationName, const AnimationGroupID animationGroupID);

	/**
	 * Cache a full AnimationGroup by its ID.
	 */
	void CacheAnimationGroup(AnimationGroupID animationGroupID);
private:

	/**
	 * Cache all animations given a premade key
	 */
	void CacheAnimations(const CDAnimationKey animationKey);

	/**
	 * Run the query responsible for caching the data.
	 * @param queryToCache 
	 * @return true 
	 * @return false 
	 */
	bool CacheData(CppSQLite3Statement& queryToCache);

	/**
	 * Each animation is key'd by its animationName and its animationGroupID.  Each
	 * animation has a possible list of animations.  This is because there can be animations have a percent chance to play so one is selected at random.
	 */
	std::map<CDAnimationKey, std::list<CDAnimation>> animations;
};
