#include "CDAnimationsTable.h"

//! Constructor
CDAnimationsTable::CDAnimationsTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM Animations");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Animations");
	while (!tableData.eof()) {
		CDAnimations entry;
		entry.animationGroupID = tableData.getIntField("animationGroupID", -1);
		entry.animation_type = tableData.getStringField("animation_type", "");
		entry.animation_name = tableData.getStringField("animation_name", "");
		entry.chance_to_play = tableData.getFloatField("chance_to_play", -1.0f);
		entry.min_loops = tableData.getIntField("min_loops", -1);
		entry.max_loops = tableData.getIntField("max_loops", -1);
		entry.animation_length = tableData.getFloatField("animation_length", -1.0f);
		entry.hideEquip = tableData.getIntField("hideEquip", -1) == 1 ? true : false;
		entry.ignoreUpperBody = tableData.getIntField("ignoreUpperBody", -1) == 1 ? true : false;
		entry.restartable = tableData.getIntField("restartable", -1) == 1 ? true : false;
		entry.face_animation_name = tableData.getStringField("face_animation_name", "");
		entry.priority = tableData.getFloatField("priority", -1.0f);
		entry.blendTime = tableData.getFloatField("blendTime", -1.0f);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDAnimationsTable::~CDAnimationsTable(void) {}

//! Returns the table's name
std::string CDAnimationsTable::GetName(void) const {
	return "Animations";
}

//! Queries the table with a custom "where" clause
std::vector<CDAnimations> CDAnimationsTable::Query(std::function<bool(CDAnimations)> predicate) {

	std::vector<CDAnimations> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDAnimations> CDAnimationsTable::GetEntries(void) const {
	return this->entries;
}

