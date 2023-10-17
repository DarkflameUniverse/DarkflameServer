#include "CppScripts.h"

class DamageReduction : public CppScripts::Script {
public:
	/**
	 * A script that casts a skill on the entity that reduces damage taken.
	 * Optionally kills the entity after 20 seconds.
	 * 
	 * @param skillToCast The skill to cast
	 * @param dieAfter20Seconds Whether to kill the entity after 20 seconds
	 */
	DamageReduction(int32_t skillToCast = 0, bool dieAfter20Seconds = false)
	: m_SkillToCast(skillToCast), m_DieAfter20Seconds(dieAfter20Seconds) {}
	void OnStartup(Entity* self) override;
private:
	int32_t m_SkillToCast;
	bool m_DieAfter20Seconds;
};
