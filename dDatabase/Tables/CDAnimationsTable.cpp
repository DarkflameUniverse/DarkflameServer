#include "CDAnimationsTable.h"
#include "GeneralUtils.h"
#include "Game.h"

namespace {
	/**
	 * Cache all animations given a premade key
	 */
	void CacheAnimations(const CDAnimationsTable::CDAnimationKey animationKey) {
		auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM Animations WHERE animationGroupID = ? and animation_type = ?");
		query.bind(1, static_cast<int32_t>(animationKey.second));
		query.bind(2, animationKey.first.c_str());
		// If we received a bad lookup, cache it anyways so we do not run the query again.
		if (!CacheData(query)) {
			animations[animationKey];
		}
	}

	/**
	 * Run the query responsible for caching the data.
	 */
	bool CacheData(CppSQLite3Statement& queryToCache) {
		auto tableData = queryToCache.execQuery();
		// If we received a bad lookup, cache it anyways so we do not run the query again.
		if (tableData.eof()) return false;

		do {
			std::string animation_type = tableData.getStringField("animation_type", "");
			DluAssert(!animation_type.empty());
			CDAnimationsTable::AnimationGroupID animationGroupID = tableData.getIntField("animationGroupID", -1);
			DluAssert(animationGroupID != -1);

			CDAnimation entry;
			entry.animation_name = tableData.getStringField("animation_name", "");
			entry.chance_to_play = tableData.getFloatField("chance_to_play", 1.0f);
			UNUSED_COLUMN(entry.min_loops = tableData.getIntField("min_loops", 0););
			UNUSED_COLUMN(entry.max_loops = tableData.getIntField("max_loops", 0););
			entry.animation_length = tableData.getFloatField("animation_length", 0.0f);
			UNUSED_COLUMN(entry.hideEquip = tableData.getIntField("hideEquip", 0) == 1;);
			UNUSED_COLUMN(entry.ignoreUpperBody = tableData.getIntField("ignoreUpperBody", 0) == 1;);
			UNUSED_COLUMN(entry.restartable = tableData.getIntField("restartable", 0) == 1;);
			UNUSED_COLUMN(entry.face_animation_name = tableData.getStringField("face_animation_name", ""););
			UNUSED_COLUMN(entry.priority = tableData.getFloatField("priority", 0.0f););
			UNUSED_COLUMN(entry.blendTime = tableData.getFloatField("blendTime", 0.0f););

			animations[CDAnimationsTable::CDAnimationKey(animation_type, animationGroupID)].push_back(entry);
			tableData.nextRow();
		} while (!tableData.eof());

		tableData.finalize();

		return true;
	}

	/**
	 * Each animation is key'd by its animationName and its animationGroupID.  Each
	 * animation has a possible list of animations.  This is because there can be animations have a percent chance to play so one is selected at random.
	 */
	std::map<CDAnimationsTable::CDAnimationKey, std::list<CDAnimation>> animations;
};

void CDAnimationsTable::CacheAnimationGroup(AnimationGroupID animationGroupID) {
	auto animationEntryCached = animations.find(CDAnimationKey("", animationGroupID));
	if (animationEntryCached != animations.end()) {
		return;
	}

	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM Animations WHERE animationGroupID = ?");
	query.bind(1, static_cast<int32_t>(animationGroupID));

	// Cache the query so we don't run the query again.
	CacheData(query);
	animations[CDAnimationKey("", animationGroupID)];
}

CDAnimationLookupResult CDAnimationsTable::GetAnimation(const AnimationID& animationType, const std::string& previousAnimationName, const AnimationGroupID animationGroupID) {
	CDAnimationKey animationKey(animationType, animationGroupID);
	auto animationEntryCached = animations.find(animationKey);
	if (animationEntryCached == animations.end()) {
		CacheAnimations(animationKey);
	}

	auto animationEntry = animations.find(animationKey);
	// If we have only one animation, return it regardless of the chance to play.
	if (animationEntry->second.size() == 1) {
		return CDAnimationLookupResult(animationEntry->second.front());
	}
	auto randomAnimation = GeneralUtils::GenerateRandomNumber<float>(0, 1);

	for (auto& animationEntry : animationEntry->second) {
		randomAnimation -= animationEntry.chance_to_play;
		// This is how the client gets the random animation.
		if (animationEntry.animation_name != previousAnimationName && randomAnimation <= 0.0f) return CDAnimationLookupResult(animationEntry);
	}

	return CDAnimationLookupResult();
}
