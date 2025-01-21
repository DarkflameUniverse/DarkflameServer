#ifndef FLAGCOMPONENT_H
#define FLAGCOMPONENT_H

#include <cstdint>
#include <set>
#include <unordered_map>

#include "Component.h"
#include "eReplicaComponentType.h"

class FlagComponent final : public Component {
public:
	static const inline eReplicaComponentType ComponentType = eReplicaComponentType::FLAG;
	FlagComponent(Entity* parent);

	void UpdateXml(tinyxml2::XMLDocument& doc) override;
	void LoadFromXml(const tinyxml2::XMLDocument& doc) override;

	// Used to clear the save data from a static context where you only have a doc (switching characters)
	static void ClearSessionFlags(tinyxml2::XMLDocument& doc);
private:

	/**
	 * Sets a flag for the character, indicating certain parts of the game that have been interacted with. Not to be
	 * confused with the permissions
	 * @param flagId the ID of the flag to set
	 * @param value the value to set for the flag
	 */
	bool OnSetFlag(GameMessages::GameMsg& msg);
	void SetPlayerFlag(const uint32_t flagId, const bool value);

	/**
	 * Gets the value for a certain character flag
	 * @param flagId the ID of the flag to get a value for
	 * @return the value of the flag given the ID (the default is false, obviously)
	 */
	bool OnGetFlag(GameMessages::GameMsg& msg);
	bool GetPlayerFlag(const uint32_t flagId) const;

	bool OnClearSessionFlags(GameMessages::GameMsg& msg) { m_SessionFlags.clear(); return true; }

	/**
	 * Sets any flags that are meant to have been set that may not have been set due to them being
	 * missing in a previous patch.
	 */
	bool OnSetRetroactiveFlags(GameMessages::GameMsg& msg);

	bool OnGetObjectReportInfo(GameMessages::GameMsg& msg);

	/**
	 * Flags only set for the duration of a session
	 *
	 */
	std::set<uint32_t> m_SessionFlags;

	/**
	 * The gameplay flags this character has (not just true values)
	 */
	std::unordered_map<uint32_t, uint64_t> m_PlayerFlags;
};

#endif  //!FLAGCOMPONENT_H
