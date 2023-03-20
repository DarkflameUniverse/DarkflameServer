#pragma once

// Custom Classes
#include "CDTable.h"

struct CDAnimation {
	// unsigned int animationGroupID;
	// std::string animation_type;
	// The above two are a pair to represent a primary key in the map.
	std::string animation_name;        //!< The animation name
	float chance_to_play;           //!< The chance to play the animation
	unsigned int min_loops;                 //!< The minimum number of loops
	unsigned int max_loops;                 //!< The maximum number of loops
	float animation_length;         //!< The animation length
	bool hideEquip;                 //!< Whether or not to hide the equip
	bool ignoreUpperBody;           //!< Whether or not to ignore the upper body
	bool restartable;               //!< Whether or not the animation is restartable
	std::string face_animation_name;   //!< The face animation name
	float priority;                 //!< The priority
	float blendTime;                //!< The blend time
};

typedef LookupResult<CDAnimation> CDAnimationLookupResult;

class CDAnimationsTable : public CDTable<CDAnimationsTable> {
	typedef int32_t AnimationGroupID;
	typedef std::string AnimationID;
	typedef std::pair<std::string, AnimationGroupID> CDAnimationKey;
public:
	CDAnimationLookupResult GetAnimation(const AnimationID& animationType, const std::string& previousAnimationName, const AnimationGroupID animationGroupID);
	void CacheAnimationGroup(AnimationGroupID animationGroupID);
private:
	void CacheAnimations(const CDAnimationKey animationKey);
	bool CacheData(CppSQLite3Statement queryToCache);
	/**
	 * Each animation type has a vector of animations.  This is because there can be animations have a percent chance to play so one is selected at random.
	 */
	std::map<CDAnimationKey, std::vector<CDAnimation>> animations;
};
