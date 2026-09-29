# Capture unknowns: GM 716, 1726, 1481, ignored client messages and server-only tables

Open questions left by the live packet capture comparison, answered from the 1.10.64 client (`legouniverse.exe`) and the
2011/2012 live captures (553 capture zips).

Source tags: **G** verified in Ghidra, **C** verified in the captures, **D** from docs (earlier notes),
**I** inferred (not proven).

## 1. GM 716 DownloadPropertyData

| Fact | Source |
|---|---|
| 716 is `DownloadPropertyData` (`GameMessage::DownloadPropertyData::Initialize` writes msgId 0x2cc). 717 is `QueryPropertyData`. | G |
| Payload is `PropertyData`, read by `PropertyData::Deserialize` @ 0x00c084f0. No flags and no optional fields. | G |
| DLU's `GameMessages::DownloadPropertyData` has the same layout. Every complete 716 packet in the captures (8,605) decodes with it and no bits left over. The other 665 are split capture fragments (`(1ofN)` files). | G, C |
| Handler: `LWOPropertyComponent::msgDownloadPropertyData` @ 0x00c83ad0. | G |

Wire layout (all little endian, byte aligned):

| # | Field | Type |
|---|---|---|
| 1 | propertyId | i64 |
| 2 | templateId | i32 (PropertyTemplate id) |
| 3 | mapId | u16 |
| 4 | vendorMapId | u16 |
| 5 | cloneId | u32 |
| 6-8 | name, description, ownerName | u32 length + UTF-16 |
| 9 | ownerId | i64 |
| 10-13 | propertyType, zoneCode, rent, rentalPeriod | u32 |
| 14 | expirationDate | u64 |
| 15 | rentAmount | u32 |
| 16 | reputation | u64 |
| 17-19 | spawnName, templateName, templateDescription | u32 length + UTF-16 |
| 20-22 | rentDuration, votes, durationType | u32 |
| 23 | renew | u8 |
| 24 | ownerAccountID | i64 |
| 25 | moderationStatus | u32 |
| 26 | rejectionReason | u32 length + UTF-16 |
| 27 | lastLogoutTime | u64 |
| 28-29 | dayOfMonthPlaqueWasBought, repAchievementReq | u32 |
| 30 | zonePosition | 3 x f32 |
| 31 | maxBuildHeight | f32 |
| 32 | rentalDate | u64 |
| 33 | accessType | u8 |
| 34 | pathPositions | u32 count, then count x 3 x f32 (client rejects count x 96 bits > unread bits) |

### Why live sent it in non-property worlds

| Fact | Source |
|---|---|
| Objects with a PropertyComponent (component 36): LOT 3188 "Property Deed in inventory", 3404, 3972 (deed items), 3315 plaque, 3189 / 9628 vendors. | CDClient |
| `LWOPropertyComponent::LoadDataFromTemplate` @ 0x00c01d00 sends `QueryPropertyData` on every such object when it loads, including inventory items. | G |
| A player holds one deed item per claimed property map. The deed's object ID is `propertyId + 1` (1,009 of 1,018 sampled packets). | C |
| After the local player's construction, the client sends one 717 per deed (for example 6 at once), and the server answers each with 716 then `UpdatePropertyModelCount` (1595). | C |
| Most 716 packets (7,221 of 8,822) were never queried by the receiving client. Those carry other players' deeds: `ownerId` is another player in the zone, sent in bursts of one packet per deed of that owner, often before that owner is constructed for the receiver (297 of 736 sampled) or never constructed (348). | C |
| Conclusion: live answered each player's deed queries with a zone-wide broadcast. The receiving client has no object with the deed's ID, so it drops those packets. | I |
| mapIds seen: 1150 (3,085), 1250, 1151, 1251, 1350, 1450, 1102 (2). | C |

Client effect for the owner's own deed (`msgDownloadPropertyData`):

| Step | Source |
|---|---|
| Copies `PropertyData` into the component and marks it valid. | G |
| Localises templateName / templateDescription from `PropertyTemplate` by templateId. | G |
| Non-vendor, non-inventory objects (plaques): `SendPropertyPlaqueVisUpdate(ownerId != 0)`, launch pad aura when owned. | G |
| Inventory items: `InventoryRefreshItemDetails` for the item (deed tooltip). | G |
| LOT 3188 only: `rentDuration != 0` starts "RenewWarningTimer"; if lastLogoutTime is newer than the stored logout and the last moderation display, raises `PropertyModerationChanged(propertyId, moderationStatus)` to the UI. | G |

DLU: `QueryPropertyData::Handle` only answers entities with a property vendor or property management component. A
query on a deed item (an inventory object, not an entity) gets no reply, so deed tooltips and the deed moderation
notice never get data. Behaviour change is backlog 73.

## 2. GM 1726 RemoveBuffsAppliedByObject

| Fact | Source |
|---|---|
| Client message table: `RemoveBuffsAppliedByObject::Initialize` writes msgId 0x6be (1726); `SetMountInventoryID::Initialize` writes 0x6bf (1727). DLU agrees. lu_packets labels 1726 `SetMountInventoryId` and is wrong. | G, D |
| Payload: bit hasObjectID, then i64 objectID when set; default 0 (Serialize 0x00d80870 / Deserialize 0x00d808e0). | G |
| Handler `LWOBuffComponent::msgRemoveBuffsAppliedByObject` @ 0x00d38f20: removes every buff (RemoveBuffIcon, immunity=false) and then every immunity (immunity=true) whose source is objectID. As decompiled, the immunity loop compares the buff loop's iterator, so immunity removal may not match by source. | G (loop detail I) |
| Live: 217 messages / 70 zips in the census; all 206 readable ones have the flag set and a persistent (player-range) object ID other than the receiver. It is sent to each remaining player, followed within a few packets by a replica destruction (125 of 206) and often `TeamRemovePlayer` (41): live sent it when an object (usually a player) left the world. | C |

DLU now has the struct (`CombatMessages.h`); sending it is backlog 67.

## 3. GM 1481 UpdatePlayerStatistic

| Fact | Source |
|---|---|
| msgId 0x5c9 (1481). Payload: u32 updateID, bit hasValue, i64 updateValue when set; default 1 (0x00d8c7d0 / 0x00d8c870). DLU's struct matches. | G |
| `LWOCharacterComponent::SendMessage` @ 0x00d34330: `playerStatistics[updateID] += updateValue`; for updateID != 12 also sends UI message "ModifyPlayerStat" {index, amount} (passport statistics page). | G |
| `LWOControllablePhysComponent::SendMessage` @ 0x00d2f020, updateID 12 only: rebases the client's local meters-travelled accumulator by the server's value. | G |
| The client also builds UpdatePlayerStatistic locally (`LWODestroyableComponent::SendMessage`, id 16 with a computed amount). Live captures hold no client -> server copy. | G, C |
| Live sent 59,933 messages server -> client. DLU only receives it and never sends it, so the passport does not change until the next login. | C |

Statistic ids live sent (updateID = DLU `StatisticID`; "value" = share of messages carrying the optional amount):

| id | StatisticID | Count | Value | Typically right after |
|---|---|---|---|---|
| 1 | CurrencyCollected | 16,756 | 17% (coins, e.g. 10, 500) | SetCurrency, PickupCurrency |
| 2 | BricksCollected | 1,010 | 8% | AddItemToInventoryClientSync (brick) |
| 3 | SmashablesSmashed | 672 | never | NotifyMissionTask |
| 4 | QuickBuildsCompleted | 227 | never | PlayFXEffect (rebuild complete) |
| 5 | EnemiesSmashed | 2,934 | never | NotifyMissionTask |
| 6 | RocketsUsed | 117 | never | NotifyClientFlagChange (rocket launch), before TransferToZone |
| 7 | MissionsCompleted | 234 | never | NotifyMissionTask / NotifyMission |
| 8 | PetsTamed | 11 | never | NotifyTamingBuildSuccess |
| 9 | ImaginationPowerUpsCollected | 2,585 | never | NotifyMissionTask, EchoStartSkill (power-up) |
| 10 | LifePowerUpsCollected | 1,428 | never | same |
| 11 | ArmorPowerUpsCollected | 1,442 | never | same |
| 12 | MetersTraveled | 20,439 | ~100% (mostly < 100) | periodic while moving; one more after TRANSFER_TO_WORLD |
| 13 | TimesSmashed | 25 | never | SetCurrency (death coin loss), Die |
| 14 | TotalDamageTaken | 939 | 43% | DoClientProjectileImpact, skills |
| 15 | TotalDamageHealed | 132 | 40% | skills, pickups |
| 16 | TotalArmorRepaired | 1,781 | 85% (often 0) | NotifyMissionTask, skills |
| 17 | TotalImaginationRestored | 3,060 | 85% (often 0) | NotifyMissionTask, SetCurrency, skills |
| 18 | TotalImaginationUsed | 1,233 | 12% | skills |
| 19 | DistanceDriven | 73 | 100% (large) | racing |
| 20 | TimeAirborneInCar | 30 | 67% | racing |
| 21 | RacingImaginationPowerUpsCollected | 222 | never | NotifyMissionTask |
| 22 | RacingImaginationCratesSmashed | 7 | never | NotifyMissionTask |
| 23 | RacingCarBoostsActivated | 51 | never | racing boost skill |
| 24 | RacingTimesWrecked | 5 | never | RequestDie / Die |
| 25 | RacingSmashablesSmashed | 53 | never | NotifyMissionTask |
| 26 | RacesFinished | 3 | never | NotifyRacingClient |
| 27 | FirstPlaceRaceFinishes | 2 | never | NotifyRacingClient |

All C. The MetersTraveled interval could not be measured (capture file times are not packet times).

What DLU sends (`CharacterComponent::UpdatePlayerStatistic` / `SendPlayerStatistic`), from the captures:

| Statistic | When | Amount | Source |
|---|---|---|---|
| CurrencyCollected | right after a SetCurrency that raised the coins (any source); none for losses | coins gained | C (15,815 pickups, all mission / achievement / vendor / activity gains) |
| BricksCollected | after each add to the bricks inventory the client is told about | the count | C (999 of 1,010) |
| EnemiesSmashed / SmashablesSmashed | to the killer right after Die, before the loot: isnpc set -> Enemies, else a smashable -> Smashables; neither while racing | 1 | C (2,766 / 538; faction and AI don't decide it) |
| TimesSmashed | after the player's Die and coin loss | 1 | C |
| MissionsCompleted | after NotifyMission(Completed), missions only | 1 | C (229 of 235) |
| QuickBuildsCompleted | after RebuildNotifyState(Completed) and effect 507, before EnableRebuild | 1 | C (218 of 227) |
| RocketsUsed | when the client fires "ZonePlayer", before TransferToZone | 1 | C (94 of 117) |
| *PowerUpsCollected | on pickup | 1 | C |
| TotalArmorRepaired / TotalImaginationRestored | what a repair / restore applied, 0 included | amount | C (1,050 / 2,148 zeros) |
| TotalDamageHealed / TotalDamageTaken / TotalImaginationUsed | health healed or lost, imagination spent; never 0 | amount | C |
| MetersTraveled | once 25 whole meters have gathered; the rest (0 included) when the player is taken down leaving the world | meters | C (values 25-33 while moving; low values only at leave) |
| DistanceDriven | every 10 s while racing, the rest on leave | units | I (interval inferred from ~1,300 per message) |
| Racing*, PetsTamed, RacesFinished, FirstPlaceRaceFinishes | where DLU already counted them | 1 | not re-checked |

The client's ModifyPlayerZoneStatistic (CoinsCollected, BricksCollected, EnemiesSmashed) only updates the zone counts,
so the totals aren't counted twice. TimeAirborneInCar is not tracked.

## 4. Client -> server messages DLU ignores

"Sent by" is the client function that builds the message (callers of `GameMessage::<Name>::Initialize`; G). IDs were
checked against the Initialize functions (G). "Server should" is D/I unless marked.

| ID | Message | Live count / zips | Sent by (G) | Server should |
|---|---|---|---|---|
| 851 | SetMissionTypeState | 650 / 303 | `LWOMissionComponent` (NotifyMissionTask, Startup) | Store the journal tab state, persist it in charxml `<mis>` (D) |
| 120 | CasterDead | 303 / 41 | `LWOSkillComponent::MsgSyncSkill`, `msgEchoStartSkill` when the caster is dead | End that skill handle for the dead caster (I) |
| 1485 | ModifyGhostingDistance | 239 / 237 | `LWOCharacterComponent::SendMessage`, on load | Scale the player's ghosting distance; every live sample leaves the scalar at default, so no-op is correct (C) |
| 932 | BounceNotification | 229 / 67 | `msgBouncePlayer` (ControllablePhys, HavokVehiclePhysics), `msgDeflect` | Record the bounce (missions, scripts, pets); live replied RequestClientBounce (C) |
| 890 | SetLastCustomBuild | 202 / 161 | `LWOModuleAssemblyComponent` (rocket build) | Save `char@lcbp` (D) |
| 1479 | RequestRailActivatorState | 152 / 6 | `LWORailActivatorComponent::SendMessage` on add to world | Reply NotifyRailActivatorStateChange (D) |
| 1577 | SetEmotesEnabled | 81 / 16 | pet taming (`LWOPlayerPetTamingComponent`), racing attach, `LWOSkillComponent` | Track emote permission while taming / racing (I) |
| 1238 | ResyncEquipment | 59 / 3 | property editor pick up / set down of a carried model, property model placement | Resend the player's equipment (I) |
| 1419 | UsedInformationPlaque | 49 / 18 | only the message factory: sent from script / UI | Plaque read tracking (achievement progress) (I) |
| 358 | ServerTerminateInteraction | 36 / 16 | `LWOPetComponent::HandleMessage` | End the pet interaction server side (I) |
| 660 | NotifyPet | 37 / - | `LWOBouncerComponent::BounceSucceeded` | Tell the player's pet about the bouncer (pet follows) (I) |
| 1166 | ToggleSendingPositionUpdates | 27 / 6 | build mode enter (false) / exit (true) | Stop expecting position updates while false (I) |
| 1072 | BuildExitConfirmation | 17 / - | `LWOPropertyManagementComponent::DoSetBuildMode`, build mode exit | Property build mode exit handshake (D) |
| 1406 | ResetPropertyBehaviors | 14 / - | `LWOPropertyEditorComponent::ResetBehaviors`, `SwitchMode` | Reset model behaviours on edit begin (D) |
| 667 | PetTamingMinigameResult | 11 / - | `LWOPlayerPetTamingComponent::SendMessage` | Taming result; DLU relies on NotifyTamingBuildSuccess (D) |
| 1632 | CelebrationCompleted | 11 / - | Character / FX component (celebration end) | Continue the flow that waited for the celebration (I) |
| 1371 | ZonePropertyModelRemovedWhileEquipped | 5 / - | `LWOPropertyEditorComponent::SwitchMode` | Remove the carried model (I) |
| 915 | PropertyModerationAction | 4 / - | `LWOPropertyManagementComponent` when moderationStatus != 0 | Handle the owner's moderation action (I) |
| 1746 | ServerCancelMoveSkill | 4 / - | status effect removal of a move skill (0x00c10e80) | Cancel the move skill server side (I) |
| 1004 | BBBResetMetadataSourceItem | 3 / - | `BBBManager::OnB3Close` | Reset the BBB source item metadata (I) |
| 469 | SetTooltipFlag | 2 / - | UI / script (factory only) | Persist `char@ttip` (D) |
| 903 | SetIgnoreProjectileCollision | 1 / - | `ForceMovementBehavior::Cast`, `ForceMovementStatusEffect::Run` | Set shouldIgnoreProjectileCollision (D) |

Handled now:

| ID | Message | DLU |
|---|---|---|
| 851 | SetMissionTypeState | Stored on MissionComponent, saved as live wrote it: `<mis>...<ts><type v="Build"><st sub="" val="1"/></type></ts>` (C). The client's loader reads `v` from `<ts>`, not from each `<type>` (G 0x00d171c0). |
| 469 | SetTooltipFlag | Bit set or cleared as the client does (clear masks `~1 << n`, which also clears the lower bits; n > 127 ignored; G 0x00d34330); saved as `char@ttip`, a u64 (G 0x00ca4910, C). |
| 890 | SetLastCustomBuild | Kept as the rocket config (`char@lcbp`, format `1:LOT;1:LOT;1:LOT;`, C). Sent when the carried rocket is assembled: at the launchpad and when landing (C). |
| 120 | CasterDead | Ends the caster's skill with that handle (pending hits dropped) when the server also sees the caster as dead (I). The target is the attacked player; the client sends it from `msgEchoStartSkill` (G 0x00d5dc90). |
| 1238 | ResyncEquipment | The player's equipment is serialized again. Live answered with replica serializations only (C). |
| 1479 | RequestRailActivatorState | Answered with NotifyRailActivatorStateChange (level key `rail_activator_active`, default true) to that client (C: all 413 true). |
| 1485 | ModifyGhostingDistance | Read and ignored: every live sample was the default scale (C). |
| world 120 | UgcDownloadFailed | Read and logged. Sent for every blueprint file whose request did not end with HTTP 200, including files not downloaded (status 0; 1520 of 1525 live) (G 0x0105e5c0, C). Live sent nothing back. The client's logout (`NET_DISCONNECT_FAILED_DOWNLOAD_UGC`, G 0x0102b9c0) is its own decision after repeated connection failures; no server message affects it. |

Not handled (the server side is unclear or needs the messages it answers with): 932 BounceNotification and 660
NotifyPet (RequestClientBounce, ClientNotifyPet), 1072 / 1406 / 1371 / 915 (property build mode protocol), 1577,
1419, 358, 1166, 667, 1632, 1746, 1004, 903.

## 5. Message names fixed in DLU

IDs unchanged; only enum names.

| ID | Old DLU name | Client name | Source |
|---|---|---|---|
| 792 | CREATEMODEL_FROM_CLIENT | DeletePropertyResponse -> DELETE_PROPERTY_RESPONSE | G, D |
| 793 | UPDATE_MODEL_FROM_CLIENT | CreateModelFromClient -> CREATE_MODEL_FROM_CLIENT | G, D |
| 915 | PROPERTY_MODERATION_STATUS_ACTION | PropertyModerationAction -> PROPERTY_MODERATION_ACTION | G, D |
| 916 | PROPERTY_MODERATION_STATUS_ACTION_RESPONSE | PropertyModerationActionResponse -> PROPERTY_MODERATION_ACTION_RESPONSE | G, D |

716, 1481 and 1726 were already named correctly. About 150 other enum names differ from other message references
(spelling, missing entries); they were not checked in Ghidra and are unchanged.

## 6. Server-only CDClient tables

The 1.10.64 client has no string for these tables (G), so only the live server read them. Meanings are from the 1.10.64
CDClient data (joined with ComponentsRegistry, Missions, MissionTasks, LootMatrix and the level files) and the live Lua.

| Table | Columns | Meaning | DLU |
|---|---|---|---|
| CollectibleComponent (79) | id, requirement_mission | The mission a collectible belongs to: usually the achievement whose collection task targets the collectible's LOT; for some, a mission to accept first (2040 for the Ninjago dragon relics, which achievements 2064-2067 collect). -1 / 66666666 on test rows. | A collectible counts only while its requirement mission (when it is a mission, not an achievement) is accepted (I). |
| EventGating (8) | eventName, date_start, date_end | Holiday event and its Unix times (UTC, inclusive): pirateDay 2011-09-19 to 2011-09-20, buildNexusTower 2010-03-09 to 2011-03-14, test rows. Lua: `GetHolidayEvent{eventToCheck}.isValid`, used only by the Crux Prime random spawners (pirateDay loads). | Read; an event runs in its dates or when event_1..event_8 names it; the str and zip random spawners use the pirateDay loads. |
| SmashableComponent (5) | id, LootMatrixIndex | No ComponentsRegistry type refers to it and 3 of its 5 loot matrices (28, 30, 31) do not exist. | Unused (no object has it). |
| RebuildSections (99) | rebuildID (RebuildComponent.id 1-27), objectID (piece LOT), offset_x/y/z, fall_angle_x/y/z and fall_height (19 rows), requires_list (piece placed first, 54 rows), size (0/1/2), bPlaced (14 rows) | Pieces of the early piece-by-piece quickbuilds ("Contest", "Team Nine Times", ZP and "? - Crate Solo" rebuilds). None of the 27 rebuilds or their pieces is placed in a 1.10.64 level or LUZ spawner. | Unused (no live object has it). |
