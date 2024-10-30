#pragma once

#ifndef EBLUEPRINTSAVERESPONSETYPE_H
#define EBLUEPRINTSAVERESPONSETYPE_H

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

#endif  //!EBLUEPRINTSAVERESPONSETYPE_H
