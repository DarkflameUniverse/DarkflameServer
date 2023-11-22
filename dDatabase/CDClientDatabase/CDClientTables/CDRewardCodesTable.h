#pragma once

// Custom Classes
#include "CDTable.h"


struct CDRewardCode {
	uint32_t id;
	std::string code;
	LOT attachmentLOT;
	UNUSED(uint32_t locStatus);
	UNUSED(std::string gate_version);
};


class CDRewardCodesTable : public CDTable<CDRewardCodesTable> {
private:
	std::vector<CDRewardCode> entries;

public:
	void LoadValuesFromDatabase();
	const std::vector<CDRewardCode>& GetEntries() const;
	LOT GetAttachmentLOT(uint32_t rewardCodeId) const;
	uint32_t GetCodeID(std::string code) const;
};
