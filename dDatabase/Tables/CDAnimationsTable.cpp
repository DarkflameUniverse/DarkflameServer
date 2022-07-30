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
		entry.animationGroupID = tableData.getIntField(0, -1);
		entry.animation_type = tableData.getStringField(1, "");
		entry.animation_name = tableData.getStringField(2, "");
		entry.chance_to_play = tableData.getFloatField(3, -1.0f);
		entry.min_loops = tableData.getIntField(4, -1);
		entry.max_loops = tableData.getIntField(5, -1);
		entry.animation_length = tableData.getFloatField(6, -1.0f);
		entry.hideEquip = tableData.getIntField(7, -1) == 1 ? true : false;
		entry.ignoreUpperBody = tableData.getIntField(8, -1) == 1 ? true : false;
		entry.restartable = tableData.getIntField(9, -1) == 1 ? true : false;
		entry.face_animation_name = tableData.getStringField(10, "");
		entry.priority = tableData.getFloatField(11, -1.0f);
		entry.blendTime = tableData.getFloatField(12, -1.0f);

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
