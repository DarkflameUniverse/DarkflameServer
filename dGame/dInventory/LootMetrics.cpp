#include "LootMetrics.h"

#include "LDFFormat.h"

namespace {
	template<typename T>
	void Append(std::u16string& out, const char16_t* key, const std::optional<T>& value) {
		if (!value) return;
		if (!out.empty()) out += u',';
		out += GeneralUtils::ASCIIToUTF16(LDFData<T>(key, *value).GetString());
	}
}

bool LootMetrics::Empty() const {
	return !sourceLot && !missionId && !activityId && !currencyDelta && !mailId && !transactionId;
}

std::u16string LootMetrics::ToExtraInfo() const {
	std::u16string out;
	Append<int32_t>(out, u"_Metric_Souce_LOT_Int", sourceLot);
	Append<int32_t>(out, u"_Metric_Mission_ID_Int", missionId);
	Append<int32_t>(out, u"_Metric_Activity_ID_Int", activityId);
	Append<int32_t>(out, u"_Metric_Currency_Delta_Int", currencyDelta);
	Append<uint64_t>(out, u"_Metric_Mail_ID_Int64", mailId);
	Append<LWOOBJID>(out, u"_Metric_Transaction_ID_Int64", transactionId);
	return out;
}
