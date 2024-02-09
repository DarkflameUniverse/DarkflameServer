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


class CDRewardCodesTable : public CDTable<CDRewardCodesTable, std::vector<CDRewardCode>> {
public:
	void LoadValuesFromDatabase();
	LOT GetAttachmentLOT(uint32_t rewardCodeId) const;
	uint32_t GetCodeID(std::string code) const;
};
