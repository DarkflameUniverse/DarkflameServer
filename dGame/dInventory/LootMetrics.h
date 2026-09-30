#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "dCommonVars.h"
#include "eLootSourceType.h"

/**
 * Where added items came from, as live servers put it in the extra info of AddItemToInventoryClientSync next to the
 * item's own config. Only the message carries these keys; the item does not keep them.
 * Live: a picked up drop had _Metric_Souce_LOT_Int (the source object's LOT, 1 for a player sourced
 * drop) on all 2,344 pickups of a DropClientLoot; mission and achievement rewards the mission ID and source LOT 1;
 * activity rewards the activity ID and the activity object's LOT; vendor purchases the vendor's LOT and the coins paid
 * (negative); mail the mail ID; trades the trade ID. Package contents had none.
 */
struct LootMetrics {
	std::optional<LOT> sourceLot;          // _Metric_Souce_LOT_Int (the key is misspelt in live's data)
	std::optional<int32_t> missionId;      // _Metric_Mission_ID_Int
	std::optional<int32_t> activityId;     // _Metric_Activity_ID_Int
	std::optional<int32_t> currencyDelta;  // _Metric_Currency_Delta_Int
	std::optional<uint64_t> mailId;        // _Metric_Mail_ID_Int64, name value type 8
	std::optional<LWOOBJID> transactionId; // _Metric_Transaction_ID_Int64, name value type 9

	[[nodiscard]] bool Empty() const;

	/**
	 * The keys as name value text (key=type:value), comma separated like an item's config in the extra info.
	 * Empty when there are none.
	 */
	[[nodiscard]] std::u16string ToExtraInfo() const;
};

// What took items away, for RemoveItemFromInventory's loot source fields (e.g. a quickbuild taking its item cost)
struct ItemRemovalSource {
	eLootSourceType type = eLootSourceType::NONE;
	LWOOBJID object = LWOOBJID_EMPTY;
};
