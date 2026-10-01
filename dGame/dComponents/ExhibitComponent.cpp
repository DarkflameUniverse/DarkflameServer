#include "ExhibitComponent.h"

#include "CDClientDatabase.h"
#include "Entity.h"
#include "Logger.h"

// LWOExhibitComponent::LoadDataFromTemplate 0x00c9ce40: the ExhibitComponent row
void ExhibitComponent::LoadDataFromTemplate() {
	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT offsetX, offsetY, offsetZ, width, height, length, fReputationSizeMultiplier, fImaginationCost FROM ExhibitComponent WHERE id = ?;");
	query.bind(1, m_ComponentID);
	auto result = query.execQuery();
	if (result.eof()) {
		LOG("Object LOT %i couldn't find ExhibitComponent %i in database", m_Parent->GetLOT(), m_ComponentID);
		return;
	}
	m_Offset = NiPoint3(result.getFloatField(0), result.getFloatField(1), result.getFloatField(2));
	m_Size = NiPoint3(result.getFloatField(3), result.getFloatField(4), result.getFloatField(5)) * 0.8f;
	m_ReputationSizeMultiplier = result.getFloatField(6);
	m_ImaginationCost = result.getFloatField(7);
}

// LWOExhibitComponent::LoadConfigData 0x00c9c950: the spawn component's config, then the exhibit_* keys
void ExhibitComponent::LoadConfigData() {
	SpawnerComponent::LoadConfigData();
	const auto text = [this](const std::u16string& key, const size_t max) {
		auto value = m_Parent->GetVar<std::u16string>(key);
		if (value.size() > max) value.resize(max);
		return value;
	};
	m_ResetTime = m_Parent->GetVar<uint32_t>(u"exhibit_reset_time");
	m_InstanceName = text(u"exhibit_instance_name", 32);
	m_VotingAllowed = m_Parent->GetVar<bool>(u"exhibit_voting_allowed");
	m_AllowUnmoderated = m_Parent->GetVar<bool>(u"exhibit_allow_unmoderated");
	m_AcceptsDroppedModels = m_Parent->GetVar<bool>(u"exhibit_accepts_dropped_models");
	m_RequiredNumberOfBricks = m_Parent->GetVar<uint32_t>(u"exhibit_required_number_of_bricks");
	m_MustUseBrickLots = text(u"exhibit_must_use_brick_lots", 250);
	m_MustNotUseBrickLots = text(u"exhibit_must_not_use_brick_lots", 250);
	m_MustUseBehaviorLots = text(u"exhibit_must_use_behavior_lots", 250);
	m_MustNotUseBehaviorLots = text(u"exhibit_must_not_use_behavior_lots", 250);
	m_InstanceDescription = text(u"exhibit_instance_description", 250);
	m_IncludeModelsMine = m_Parent->GetVar<bool>(u"exhibit_include_models_mine");
	m_IncludeModelsBffs = m_Parent->GetVar<bool>(u"exhibit_include_models_bffs");
	m_IncludeModelsFriends = m_Parent->GetVar<bool>(u"exhibit_include_models_friends");
	m_IncludeModelsGuild = m_Parent->GetVar<bool>(u"exhibit_include_models_guild");
	m_IncludeModelsPublic = m_Parent->GetVar<bool>(u"exhibit_include_models_public");
	m_DisplayRangeByRank = m_Parent->GetVar<uint32_t>(u"exhibit_display_range_by_rank");
	m_TargetTopModelsInstance = text(u"exhibit_target_top_models_instance", 32);
	m_IsAnimatedPedestal = m_Parent->GetVar<bool>(u"exhibit_is_animated_pedestal");
	m_AnimatedPedestalActiveBone = m_Parent->GetVar<uint32_t>(u"exhibit_animated_ped_active_bone");
}
