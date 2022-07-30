#include "CDSkillBehaviorTable.h"
//#include "Logger.hpp"

//! Constructor
CDSkillBehaviorTable::CDSkillBehaviorTable(void) {
	m_empty = CDSkillBehavior();

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM SkillBehavior");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	//this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM SkillBehavior");
	while (!tableData.eof()) {
		CDSkillBehavior entry;
		entry.skillID = tableData.getIntField(0, -1);
		UNUSED(entry.locStatus = tableData.getIntField(1, -1));
		entry.behaviorID = tableData.getIntField(2, -1);
		entry.imaginationcost = tableData.getIntField(3, -1);
		entry.cooldowngroup = tableData.getIntField(4, -1);
		entry.cooldown = tableData.getFloatField(5, -1.0f);
		UNUSED(entry.isNpcEditor = tableData.getIntField(6, -1) == 1 ? true : false);
		UNUSED(entry.skillIcon = tableData.getIntField(7, -1));
		UNUSED(entry.oomSkillID = tableData.getStringField(8, ""));
		UNUSED(entry.oomBehaviorEffectID = tableData.getIntField(9, -1));
		UNUSED(entry.castTypeDesc = tableData.getIntField(10, -1));
		UNUSED(entry.imBonusUI = tableData.getIntField(11, -1));
		UNUSED(entry.lifeBonusUI = tableData.getIntField(12, -1));
		UNUSED(entry.armorBonusUI = tableData.getIntField(13, -1));
		UNUSED(entry.damageUI = tableData.getIntField(14, -1));
		UNUSED(entry.hideIcon = tableData.getIntField(15, -1) == 1 ? true : false);
		UNUSED(entry.localize = tableData.getIntField(16, -1) == 1 ? true : false);
		UNUSED(entry.gate_version = tableData.getStringField(17, ""));
		UNUSED(entry.cancelType = tableData.getIntField(18, -1));

		this->entries.insert(std::make_pair(entry.skillID, entry));
		//this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDSkillBehaviorTable::~CDSkillBehaviorTable(void) {}

//! Returns the table's name
std::string CDSkillBehaviorTable::GetName(void) const {
	return "SkillBehavior";
}

//! Queries the table with a custom "where" clause
std::vector<CDSkillBehavior> CDSkillBehaviorTable::Query(std::function<bool(CDSkillBehavior)> predicate) {

	/*std::vector<CDSkillBehavior> data = cpplinq::from(this->entries)
	>> cpplinq::where(predicate)
	>> cpplinq::to_vector();

	return data;*/

	//Logger::LogDebug("CDSkillBehaviorTable", "The 'Query' function is no longer working! Please use GetSkillByID instead!");
	std::vector<CDSkillBehavior> data; //So MSVC shuts up
	return data;
}

//! Gets an entry by ID
const CDSkillBehavior& CDSkillBehaviorTable::GetSkillByID(unsigned int skillID) {
	std::map<unsigned int, CDSkillBehavior>::iterator it = this->entries.find(skillID);
	if (it != this->entries.end()) {
		return it->second;
	}

	return m_empty;
}
