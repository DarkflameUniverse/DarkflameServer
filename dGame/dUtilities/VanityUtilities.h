#pragma once

#include "dCommonVars.h"
#include "Entity.h"
#include <map>
#include <set>

struct VanityObjectLocation
{
	float m_Chance = 1.0f;
	NiPoint3 m_Position;
	NiQuaternion m_Rotation;
	float m_Scale = 1.0f;
};

struct VanityObject
{
	LWOOBJID m_ID = LWOOBJID_EMPTY;
	std::string m_Name;
	LOT m_LOT;
	std::vector<LOT> m_Equipment;
	std::vector<std::string> m_Phrases;
	std::string m_Script;
	std::map<uint32_t, std::vector<VanityObjectLocation>> m_Locations;
	std::vector<LDFBaseData*> m_Config;
};


class VanityUtilities
{
public:
	static void SpawnVanity();

	static Entity* SpawnObject(
		const VanityObject& object,
		const VanityObjectLocation& location
	);

	static LWOOBJID SpawnSpawner(
		const VanityObject& object,
		const VanityObjectLocation& location
	);

	static std::string ParseMarkdown(
		const std::string& file
	);

	static void ParseXML(
		const std::string& file
	);

	static VanityObject* GetObject(const std::string& name);

private:
	static void SetupNPCTalk(Entity* npc);

	static void NPCTalk(Entity* npc);

	static std::vector<VanityObject> m_Objects;
	
	static std::set<std::string> m_LoadedFiles;
};
