#pragma once

#include "Component.h"
#include "Entity.h"

/**
 * Component that handles the LOT that is shown in the LUP exhibit in the LUP world. Works by setting a timer and
 * switching the LOTs around that we'd like to display.
 */
class LUPExhibitComponent : public Component
{
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_EXHIBIT;

	LUPExhibitComponent(Entity* parent);
	~LUPExhibitComponent();
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
	std::vector<LOT> m_Exhibits;

	/**
	 * The current index in the exhibit list
	 */
	size_t m_ExhibitIndex;
};
