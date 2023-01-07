#pragma once

#ifndef __EBLUEPRINTSAVERESPONSETYPE__H__
#define __EBLUEPRINTSAVERESPONSETYPE__H__

#include <cstdint>

enum class eBlueprintSaveResponseType : uint32_t {
    EverythingWorked = 0,
    SaveCancelled,
    CantBeginTransaction,
    SaveBlueprintFailed,
    SaveUgobjectFailed,
    CantEndTransaction,
    SaveFilesFailed,
    BadInput,
    NotEnoughBricks,
    InventoryFull,
    ModelGenerationFailed,
    PlacementFailed,
    GmLevelInsufficient,
    WaitForPreviousSave,
    FindMatchesFailed
};

#endif  //!__EBLUEPRINTSAVERESPONSETYPE__H__
