#include "AmTemplateSkillVolume.h"
#include "MissionComponent.h"

void AmTemplateSkillVolume::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message != "NinjagoSpinAttackEvent") {
		return;
	}

	auto* missionComponent = caster->GetComponent<MissionComponent>();

	const auto missionIDsVariable = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"missions"));
	const auto missionIDs = GeneralUtils::SplitString(missionIDsVariable, '_');

	for (const auto& missionIDStr : missionIDs) {
		const auto missionID = GeneralUtils::TryParse<uint32_t>(missionIDStr);
		if (!missionID) continue;

		missionComponent->ForceProgressTaskType(missionID.value(), 1, 1, false);
	}
}
