#pragma once

#include "Component.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

/**
 * Component that handles the LOT that is shown in the LUP exhibit in the LUP world. Works by setting a timer and
 * switching the LOTs around that we'd like to display.
 */
class LUPExhibitComponent final : public Component
{
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::EXHIBIT;

	LUPExhibitComponent(Entity* parent);
	void Update(float deltaTime) override;
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, uint32_t& flags);

	/**
	 * After the timer runs out, this changes the currently exhibited LOT to the next one
	 */
	void NextExhibit();
private:
	/**
	 * The LOT that's currently on exhibit
	 */
	LOT m_Exhibit;

	/**
	 * The time since we've last updated the exhibit
	 */
	float m_UpdateTimer;

	/**
	 * The list of possible exhibits to show
	 */
	const std::vector<LOT> m_Exhibits = { 11121, 11295, 11423, 11979 };

	/**
	 * The current index in the exhibit list
	 */
	size_t m_ExhibitIndex;

	// Whether or not to notify clients of a change in the visible exhibit
	bool m_DirtyExhibitInfo;
};
