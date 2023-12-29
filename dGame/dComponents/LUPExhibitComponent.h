#pragma once

#include "Component.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

/**
 * Component that handles the LOT that is shown in the LUP exhibit in the LUP world. Works by setting a timer and
 * switching the LOTs around that we'd like to display.
 */
class LUPExhibitComponent : public Component
{
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::LUP_EXHIBIT;

	LUPExhibitComponent(Entity* parent);
	void Update(float deltaTime) override;
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) override;

	/**
	 * After the timer runs out, this changes the currently exhibited LOT to the next one
	 */
	void NextLUPExhibit();
private:
	/**
	 * The LOT that's currently on exhibit
	 */
	LOT m_LUPExhibit;

	/**
	 * The time since we've last updated the exhibit
	 */
	float m_UpdateTimer;

	/**
	 * The list of possible exhibits to show
	 */
	std::vector<LOT> m_LUPExhibits;

	/**
	 * The current index in the exhibit list
	 */
	size_t m_LUPExhibitIndex;

	/**
	 * If the data is dirty
	*/
	bool m_DirtyLUPExhibit = false;
};
