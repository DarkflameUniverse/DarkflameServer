# Building on a property: model placement and brick by brick

How the 1.10.64 client goes in and out of property editing, places and removes models and builds brick by brick (BBB)
models, what it expects the server to answer, and what DLU does. Sources: the client in Ghidra (addresses below,
bookmark category `BuildWorkflow`) and 2014 live captures (only the order of messages and how ids relate are used here;
no capture data is copied). Arrows: `C->S` client to server, `S->C` server to client. Message names are the client's.

Code: `dGame/dComponents/PropertyManagementComponent` (placing and removing models), `dGame/dUtilities/BrickByBrick`
(BBB sessions, autosave, recovery), messages in `dGame/dGameMessages/BuildingMessages`, `PropertyMessages`,
`InventoryMessages`, and `ClientPackets::BlueprintSaveResponse` / `BlueprintLoadItemResponse`.

## 1. Inventories and ids

| Inventory | Holds | Notes |
|---|---|---|
| `MODELS` (5) | Model items: premade models (their own LOT) and brick built models (LOT 6662) | Where every model goes when it leaves the property |
| `MODELS_IN_BBB` (3) | The models opened in BBB (the originals) | Used up by a save; given back otherwise |
| `BRICKS` (2) | Bricks | |
| `BRICKS_IN_BBB` (9) | Bricks taken into the BBB model being built | Moved with `MoveInventoryBatch` |
| `TEMP_MODELS` (6) | Modular build parts (rocket/car) | Modular builds only |

Ids of one model through the workflow (live):

- **Model item id**: a new object id every time the model becomes an item (placing then picking up gives a new item id:
  one more than the placed model's id in the capture). Opening a model in BBB keeps the item id
  (`BlueprintLoadItemResponse.destItemId == itemId`).
- **UGID / property model id** (`userModelID`, the `properties_contents` id, the spawner id): premade models get a new
  one each time they are placed. A brick built model keeps its UGID and blueprint across pick up / place: it is the
  item's `subkey` and `userModelID` config.
- **Blueprint id** (`blueprintid`, the `ugc` row): made by a BBB save, one per model the save splits into. Every save
  makes new blueprint ids (the client caches blueprints by id), so an edited model is a new blueprint; the old `ugc`
  row is kept.
- **Model object id**: the runtime id of the spawned object on the property; new on every spawn.
  `GetModelsOnProperty` pairs it with the UGID.
- **BBB local id**: `BBBSaveRequest.localID`, a client-made id the save response must echo (client keeps it at
  `BBBManager+0x18`).

A brick built model item's config (as live sent it): `blueprintid`, `userModelID`, `userModelName`, `userModelDesc`,
`userModelHasBhvr`, `userModelBehaviors` ("id,id,id,id,id"), `userModelBehaviorSourceIDs`, `userModelOpt`,
`userModelMod`, `userModelPhysicsType` (`BrickByBrick::ModelItemConfig`).

## 2. Entering and leaving property editing

1. `C->S StartBuildingWithItem` (subject: property plaque/build area; source = the thinking hat, `sourceType` 1).
   `S->C StartArrangingWithItem` (`firstTime`, `buildAreaID` = subject, player position, `sourceType` 1 answered as 4).
2. `C->S SetBuildMode(start)` to the build area. `S->C SetBuildModeConfirmed` (broadcast; `warnVisitors` false going in,
   true coming out).
3. `C->S PropertyEditorBegin`. Server: property goes private, visitors are sent away, equipped items are pushed, models
   pause and reset (`OnStartBuilding`). Live also sent `NotifyPropertyOfEditMode` and `PropertyBuildModeUpdate`.
4. `C->S PropertyContentsFromClient`. `S->C GetModelsOnProperty` (pairs: model object, UGID).
5. `C->S BuildModeSet(start)` (player). The client's own notification; DLU records build mode on the character.

Leaving: `C->S SetBuildMode(false)`, `PopEquippedItemsState`, `DoneArrangingWithItem` (new source empty: no answer),
`S->C SetBuildModeConfirmed(false)`, `C->S PropertyEditorEnd` (property saved, models resume, privacy restored),
`C->S BuildModeSet(false)`.

## 3. Placing and removing models

Place (the model item is carried in the hand):

| Step | Messages |
|---|---|
| Equip | `C->S EquipInventory(item)`, `C->S ZonePropertyModelEquipped` (zone control) |
| Rotate | `C->S ZonePropertyModelRotated` (zone control), client side only |
| Place | `C->S PlacePropertyModel(0)` then `C->S UpdateModelFromClient(item id, position, rotation)` |
| Answer | `S->C RemoveItemFromInventory(item)`, `S->C HandleUGCEquipPreCreateBasedOnEditMode(0, UGID)`, `S->C PlaceModelResponse(position, plaque, 14, rotation)`, `S->C GetModelsOnProperty` |

`PlaceModelResponse` echoes the rotation the client sent: every field is optional and the rotation is a w, x, y, z
quaternion (`PlaceModelResponse::Deserialize`, `0x00dc0170`). DLU used to write the 4-byte response there instead
(wire fix, commit "fix(wire): PlaceModelResponse writes the model's rotation").

Remove (`C->S DeleteModelFromClient(model object, reason)`):

| Reason | Where the model goes | Answer |
|---|---|---|
| 0 picking up | `MODELS`, equipped (carried) | `AddItemToInventoryClientSync` (new item id), `EquipInventory`, `HandleUGCEquipPostDeleteBasedOnEditMode(item, count)`, `GetModelsOnProperty`, `PlaceModelResponse(16)` |
| 1 returning to inventory | `MODELS` | `AddItemToInventoryClientSync`, `GetModelsOnProperty`, `PlaceModelResponse(16)` |
| 2 breaking apart (open in BBB) | `MODELS`; the client then sends `BBBLoadItemRequest` | as picking up, not equipped |

Putting a carried model away: `C->S UnEquipInventory` and `ZonePropertyModelRemovedWhileEquipped`; nothing to answer.

DLU saves the property after each place and remove (`PropertyManagementComponent::Save`), so a crash or disconnect
before `PropertyEditorEnd` loses nothing: the item is gone only once the model is in `properties_contents`.

## 4. Brick by brick

### 4.1 Entering

- New model: `C->S DoneArrangingWithItem(newSource = a brick in BRICKS, sourceType 2)`,
  `S->C StartArrangingWithItem(firstTime false, same source)`.
- Editing a placed model: `C->S DoneArrangingWithItem(newTarget = the model object, LOT 14, targetType 4)`,
  `S->C StartArrangingWithItem`, then `DeleteModelFromClient(reason 2)` (3) and `BBBLoadItemRequest`.
- `C->S ActivateBrickMode(buildObjectID = build area, buildType 2, enterBuildFromWorld false, enterFlag true)`,
  `C->S BuildModeSet(start)`.

`C->S BBBLoadItemRequest(item)`: the server moves the model item from `MODELS` to `MODELS_IN_BBB` keeping its id
(`RemoveItemFromInventory`, `AddItemToInventoryClientSync`) and answers `S->C BlueprintLoadItemResponse(success, item,
destItem)`. With `success` 0 the client shows `BBB_ERROR_LOADING_BLUEPRINT` and drops the load (`0x00b75bb0`). Live also
put the model's bricks in `BRICKS_IN_BBB`; DLU does not track the bricks inside models (see 6).

Bricks: each brick taken from the backpack is `C->S MoveInventoryBatch(BRICKS -> BRICKS_IN_BBB, LOT, count)` and each
brick put back the reverse. The client has already taken them out of the source bag (`0x00ce1310`); the server moves
them without telling the client about the source and answers `S->C AddItemToInventoryClientSync` for the destination.

### 4.2 Saving

`C->S BBBSaveRequest(localID, sd0 LXFML, timeTakenInMs)`. The client sends it only from **B3Close** (the exit button,
`BBBManager::OnB3Close`, `0x00b73ad0`) when the model has bricks (with `BBB_TOO_MANY_MODELS_WARNING` first when it
splits into more than 10 models); it shows a saving bar and waits (`BBB_SAVE_WAIT_FOR_PREVIOUS` meanwhile). The server:

1. splits the LXFML into models (`Lxfml::Split`), stores each as a new `ugc` row (`is_optimized` 0: the UGC server
   makes its mesh and icon);
2. settles the bricks in `BRICKS_IN_BBB` (live used them up: `RemoveItemFromInventory`; DLU gives them back unless
   `bbb_consume_bricks=1`);
3. `S->C BlueprintSaveResponse(localID, reason, [blueprint id, sd0 of each model])`;
4. uses up the models in `MODELS_IN_BBB` (`RemoveItemFromInventory`);
5. places the new models on the property at their centers, saves the property, clears the autosave;
6. `S->C RequeryPropertyModels` (the client then asks `PropertyContentsFromClient`).

On the save response (`BBBManager::OnBlueprintSaveResponse`, `0x00b73a50`) the client ignores any `localId` other than
the one it is waiting for; reason 0 while in BBB clears the build and leaves brick mode; any reason ends the saving
state. Reasons with a message: 2, 3, 5, 6, 7, 8, 9, 10, 13. DLU answers 10 (`ModelGenerationFailed`) when nothing
could be made from the LXFML and 11 (`PlacementFailed`, no message, the build stays) off the player's own property.

### 4.3 Quick save (autosave)

`C->S SetBBBAutosave(sd0 LXFML)` is the client's quick save of the model being built
(`BBBManager::SaveModel`, `0x00b5be30`, sent only when the model changed since the last one):

- every 5 minutes while not placing a brick (`BBBManager::AutosaveTick`, `0x00b5bfd0`);
- when the server tells the client the player is AFK (`msgInformAFK`, `0x00be1b88`) and before the client shuts down
  (`PreShutdown`, `0x00be1c79`);
- when leaving brick mode (`BBBManager::Deactivate`, `0x00b71f90`).

With nothing to keep (after a save, or an emptied model) it sends the bare sd0 header (5 bytes) to clear it. (The
client also has a `BBB_SAVE` key action, 0x55; its handler was not traced.) DLU keeps the latest one per character in `bbb_autosave`, with the
ids of the models in `MODELS_IN_BBB` at the time.

`S->C RebuildBBBAutosaveMsg(count)` tells the client the server rebuilt `count` unfinished models from an autosave; it
shows `BBB_AUTOSAVE_REBUILDING_SINGLE` / `_MULTIPLE` ("... tried to rebuild it for you. Any leftover bricks have been
returned to your Backpack", `0x00cfabd0`).

### 4.4 Leaving without a save, disconnects and crashes

Leaving: `C->S BBBResetMetadataSourceItem` (when the model is empty), `C->S SetBBBAutosave` (maybe), `C->S
ActivateBrickMode(enterFlag false)`, `C->S BuildModeSet(false)`, then the property editor's leave (2).

DLU resolves every build that ends without a save the same way (`BrickByBrick::RecoverUnfinishedBuild`), when the
player leaves brick mode (`ActivateBrickMode` with `enterFlag` false) and when a character loads into a world:

- an autosave with a model: it is rebuilt into brick built model items in `MODELS` (new `ugc` rows), the models it was
  made from are used up, `RebuildBBBAutosaveMsg(count)` is sent;
- otherwise the models in `MODELS_IN_BBB` go back to `MODELS` unchanged (leaving an edit without saving never loses the
  original);
- bricks in `BRICKS_IN_BBB` go back to `BRICKS`; the autosave is cleared.

`MODELS_IN_BBB` is saved with the character (it used to be left out, so a disconnect in BBB deleted the model, #1632),
and on load the BBB bags load into `MODELS` and `BRICKS`. A world or client crash therefore keeps the opened models;
the next load rebuilds the autosave, if there was one, in their place.

### 4.5 Undo

The client's undo and redo (`PropertyEditUndo` / `PropertyEditRedo`, registered only by the BBB UI in
`BBBManager::RegisterUIListeners`, `0x00b73c70`) work on the BBB session; model placement has no undo. What reaches the
server:

- Bricks taken or put back: `MoveInventoryBatch` in either direction (4.1).
- Importing a placed model into the build (import tool, `BBBImportModelTool::ImportModel`, `0x00b5fd20`): it is taken
  off the property (`DeleteModelFromClient` reason 2) and opened (`BBBLoadItemRequest`). Undoing it, or a model whose
  blueprint could not be loaded (`BBBManager::OnBlueprintLoaded`, `0x00b73040`), sends `C->S UnUseBBBModel(model item,
  bHasWorldTransform, position, rotation)` (`BBBManager::SendUnUseBBBModel`, `0x00b6b210`). With a world transform the
  model came from the property: DLU places it back there; otherwise it goes back to `MODELS`. Either way the id is
  dropped from the autosave's list so a later rebuild cannot use it up. With a world transform the client waits in its
  saving state; DLU ends it with `BlueprintSaveResponse(localId 0, PlacementFailed)`, which keeps the build.
- Leaving without saving (4.4): the originals come back.

## 5. Modular builds (rockets and cars)

`C->S StartBuildingWithItem` / `S->C StartArrangingWithItem`, the parts are arranged client side, `C->S
ModularBuildFinish(part LOTs)`, `C->S ModularBuildMoveAndEquip(LOT)` (the built item from `TEMP_MODELS` to `MODELS`,
equipped), `S->C FinishArrangingWithItem` / `ModularBuildEnd`, `C->S DoneArrangingWithItem`. Parts sit in
`TEMP_MODELS` while building; `DoneArrangingWithItem` moves what is left there back to `MODELS`.
`ModularBuildConvertModel` takes a build apart into `TEMP_MODELS`. Unchanged by this work.

## 6. DLU choices that differ from live

- Bricks are free: a save gives the bricks in `BRICKS_IN_BBB` back to the backpack (live used them up), unless
  `bbb_consume_bricks=1`. Loading a model does not put its bricks in `BRICKS_IN_BBB`, so bricks taken off a loaded
  model cannot be put in the backpack (the client shows them there until the next load).
- A model opened and then emptied is given back when leaving, rather than removed as live did (its bricks were never
  given to the player).
- An edited model gets new blueprint ids; the old `ugc` row stays.

## 7. Client functions (1.10.64)

| Address | Name |
|---|---|
| `0x00b5be30` | `BBBManager::SaveModel` (autosave) |
| `0x00b5b580` | `BBBManager::SendAutosaveIfChanged` |
| `0x00b5bfd0` | `BBBManager::AutosaveTick` |
| `0x00b6cb40` | `BBBManager::SendBBBSaveRequest` |
| `0x00b6cf00` | `BBBManager::SendSaveRequestTimed` |
| `0x00b6d7b0` | `BBBManager::RequestSave` |
| `0x00b73ad0` | `BBBManager::OnB3Close` |
| `0x00b756b0` | `BBBManager::RequestExit` |
| `0x00b71f90` | `BBBManager::Deactivate` |
| `0x00b73a50` | `BBBManager::OnBlueprintSaveResponse` |
| `0x00b66850` | `BBBManager::ProcessBlueprintSaveResponse` |
| `0x00b75bb0` | `OnBlueprintLoadItemResponse` |
| `0x00b73040` | `BBBManager::OnBlueprintLoaded` |
| `0x00b6b210` | `BBBManager::SendUnUseBBBModel` |
| `0x00b5fd20` / `0x00b6b400` | `BBBImportModelTool::ImportModel` / `ReturnModel` |
| `0x00b73c70` | `BBBManager::RegisterUIListeners` |
| `0x00cfabd0` | `LWOBBBComponent_Client::msgRebuildBBBAutosaveMsg` |
| `0x00d8ecb0` | `GameMessage::ActivateBrickMode::Deserialize` |
| `0x00f2af60` | `GameMessage::SetBBBAutosave::Deserialize` |
| `0x00dc0170` | `GameMessage::PlaceModelResponse::Deserialize` |
| `0x00ce1310` | `LWOInventoryComponent_Common::msgMoveInventoryBatch` |
