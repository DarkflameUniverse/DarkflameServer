#ifndef EXHIBITCOMPONENT_H
#define EXHIBITCOMPONENT_H

#include <string>

#include "NiPoint3.h"
#include "SpawnerComponent.h"

/**
 * An exhibit (showcase pedestal, 32), as the client's LWOExhibitComponent: a spawn component that shows a player's
 * model. The client sends and reads nothing for it in a construction (its Deserialize is empty); it spawns the shown
 * model itself from the exhibit messages. 1.10.64's loader has the case but makes nothing (dead code); DLU makes it.
 */
class ExhibitComponent final : public SpawnerComponent {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::EXHIBIT;

	ExhibitComponent(Entity* parent, const int32_t componentID) : SpawnerComponent(parent, componentID) {}

	void LoadDataFromTemplate() override;
	void LoadConfigData() override;

	const NiPoint3& GetOffset() const { return m_Offset; }
	const NiPoint3& GetSize() const { return m_Size; }
	const std::u16string& GetInstanceName() const { return m_InstanceName; }
	bool GetVotingAllowed() const { return m_VotingAllowed; }

private:
	// The ExhibitComponent row: where the model sits on the pedestal and how big it may be (the client scales the
	// row's size by 0.8)
	NiPoint3 m_Offset{};
	NiPoint3 m_Size{};
	float m_ReputationSizeMultiplier = 1.0f;
	float m_ImaginationCost = 0.0f;

	// The level config (LWOExhibitComponent::LoadConfigData 0x00c9c950)
	uint32_t m_ResetTime = 0;
	std::u16string m_InstanceName; // at most 32 characters
	bool m_VotingAllowed = false;
	bool m_AllowUnmoderated = false;
	bool m_AcceptsDroppedModels = false;
	uint32_t m_RequiredNumberOfBricks = 0;
	std::u16string m_MustUseBrickLots; // at most 250 characters each
	std::u16string m_MustNotUseBrickLots;
	std::u16string m_MustUseBehaviorLots;
	std::u16string m_MustNotUseBehaviorLots;
	std::u16string m_InstanceDescription;
	bool m_IncludeModelsMine = false;
	bool m_IncludeModelsBffs = false;
	bool m_IncludeModelsFriends = false;
	bool m_IncludeModelsGuild = false;
	bool m_IncludeModelsPublic = false;
	uint32_t m_DisplayRangeByRank = 0;
	std::u16string m_TargetTopModelsInstance; // at most 32 characters
	bool m_IsAnimatedPedestal = false;
	uint32_t m_AnimatedPedestalActiveBone = 0;
};

#endif // EXHIBITCOMPONENT_H
