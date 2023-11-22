#include "CDRewardCodesTable.h"

void CDRewardCodesTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM RewardCodes");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM RewardCodes");
	while (!tableData.eof()) {
		CDRewardCode entry;
		entry.id = tableData.getIntField("id", -1);
		entry.code = tableData.getStringField("code", "");
		entry.attachmentLOT = tableData.getIntField("attachmentLOT", -1);
		UNUSED_COLUMN(entry.locStatus = tableData.getIntField("locStatus", -1));
		UNUSED_COLUMN(entry.gate_version = tableData.getStringField("gate_version", ""));

		this->entries.push_back(entry);
		tableData.nextRow();
	}
}

LOT CDRewardCodesTable::GetAttachmentLOT(uint32_t rewardCodeId) const {
	for (auto const &entry : this->entries){
		if (rewardCodeId == entry.id) return entry.attachmentLOT;
	}
	return LOT_NULL;
}

uint32_t CDRewardCodesTable::GetCodeID(std::string code) const {
	for (auto const &entry : this->entries){
		if (code == entry.code) return entry.id;
	}
	return -1;
}

