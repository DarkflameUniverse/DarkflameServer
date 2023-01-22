#include "eMissionTaskType.h"

#ifndef __ACHIEVEMENTCACHEKEY__H__
#define __ACHIEVEMENTCACHEKEY__H__

class AchievementCacheKey {
public:
	AchievementCacheKey() {
		targets = "";
		value = 0;
		type = eMissionTaskType::UNKNOWN;
	};

	bool operator==(const AchievementCacheKey& point) const {
		return this->targets == point.targets && this->value == point.value && this->type == point.type;
	};
	void SetTargets(const std::string value) { this->targets = value; };
	void SetValue(uint32_t value) { this->value = value; };
	void SetType(eMissionTaskType value) { this->type = value; };

	std::string GetTargets() const { return this->targets; };
	uint32_t GetValue() const { return this->value; };
	eMissionTaskType GetType() const { return this->type; };
private:
	std::string targets;
	uint32_t value;
	eMissionTaskType type;

};

// Specialization of hash for the above class
namespace std {
	template<>
	struct hash<AchievementCacheKey> {
		size_t operator()(const AchievementCacheKey& key) const {
			size_t hash = 0;
			GeneralUtils::hash_combine(hash, key.GetType());
			GeneralUtils::hash_combine(hash, key.GetValue());
			GeneralUtils::hash_combine(hash, key.GetTargets());
			return hash;
		};
	};
};

#endif  //!__ACHIEVEMENTCACHEKEY__H__
