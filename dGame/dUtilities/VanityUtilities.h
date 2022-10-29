#pragma once

#include "dCommonVars.h"
#include "Entity.h"
#include <map>

struct VanityNPCLocation
{
	float m_Chance = 1.0f;
	NiPoint3 m_Position;
	NiQuaternion m_Rotation;
};

struct VanityNPC
{
	std::string m_Name;
	LOT m_LOT;
	std::vector<LOT> m_Equipment;
	std::vector<std::string> m_Phrases;
	std::string m_Script;
	std::map<std::string, bool> m_Flags;
	std::map<uint32_t, std::vector<VanityNPCLocation>> m_Locations;
};

struct VanityParty
{
	uint32_t m_Zone;
	float m_Chance = 1.0f;
	std::vector<VanityNPCLocation> m_Locations;
};

class VanityUtilities
{
public:
	static void SpawnVanity();

	static Entity* SpawnNPC(
		LOT lot,
		const std::string& name,
		const NiPoint3& position,
		const NiQuaternion& rotation,
		const std::vector<LOT>& inventory,
		const std::vector<LDFBaseData*>& ldf
	);

	static std::string ParseMarkdown(
		const std::string& file
	);

	static void ParseXML(
		const std::string& file
	);

	static VanityNPC* GetNPC(const std::string& name);

private:
	static void SetupNPCTalk(Entity* npc);

	static void NPCTalk(Entity* npc);

	static std::vector<VanityNPC> m_NPCs;

	static std::vector<VanityParty> m_Parties;

	static std::vector<std::string> m_PartyPhrases;
};
