# Property building

Who can build on a property, and what happens to the models they place. The rules are in
`dGame/dUtilities/PropertyBuilders.h` (unit tested in `tests/dGameTests/PropertyBuildersTests.cpp`);
`PropertyManagementComponent` applies them.

## Setting (worldconfig.ini)

| Setting | Default | Meaning |
|---|---|---|
| `property_bff_build` | `0` | `1`: best friends of the owner can join the owner's build mode on the property. `0` (as live): only the owner builds. |

It is read when used; a config reload applies it to players already on the property.

## Who can build

`PropertyManagementComponent::CanBuild(player)`:

- The owner: always. Only the owner starts build mode (`PropertyEditorBegin`).
- A best friend of the owner (`friends.best_friend = 3`), with `property_bff_build=1`:
  - while the owner is in build mode: can enter build mode and build;
  - after the owner left build mode (or the world): keeps building while still in build mode; once they leave build
    mode they can't enter it again until the owner enters it again.
- Anyone else, and anyone on an unclaimed property: never.

Best friend status is looked up from the database once per player (when they load or are first checked) and again
when the property's owner changes.

Building means: entering build mode (`PropertyEditorBegin`/`End`), placing, moving and picking up models
(`UpdateModelFromClient`, `DeleteModelFromClient`), saving and placing brick by brick models (`BBBSaveRequest`,
`UnUseBBBModel` with a world position) and editing behaviors (`ControlBehaviors`). Each uses the builder's own
inventory, missions and client; `ControlBehaviors` answers the player who sent it.

Owner only, whatever the setting: privacy (`SetPropertyAccess`) and the property's name and description
(`UpdatePropertyOrModelForFilterCheck`).

## Property data per player

The client allows editing only when `DownloadPropertyData.ownerId` is its own character (without it the client shows
`PRECONDITION_OWN_PROPERTY`). Every `DownloadPropertyData` is sent to each player on their own address: a player who
can build and is not the owner gets their own character id as `ownerId`; everyone else gets the owner's.
`ownerName` is always the owner's. The plaque then shows a builder the owner's management screen; privacy and name
changes from it are refused.

The rights each player was last sent are kept. When they change (the owner enters or leaves build mode, a best friend
leaves build mode, a builder leaves the world, the setting changes, the property is claimed) each player whose
rights changed gets `DownloadPropertyData` again. A player in build mode who lost the right (the setting turned off)
is sent `SetBuildModeConfirmed` with `start=false`, their building ends as if they had left build mode, and they get a
chat message.

`SetBuildModeConfirmed` goes only to the player who asked. `GetModelsOnProperty` goes to each player, targeting their
own character.

## Building together

- The first builder to enter build mode (the owner) makes the property private (keeping the previous privacy),
  pauses and resets the models, smashes spawned property enemies and sends every player who can't build to the
  property's launch zone. Best friends who can build stay.
- Later builders only push their equipped items.
- Each builder leaving build mode saves the property. The last one sets the property back to pending moderation,
  restores the previous privacy, resumes the models and smashes spawned property enemies.
- A builder leaving the world while in build mode counts as leaving build mode.

Saving uses the property's id and owner id; it does not need the owner to be in the world.

## Who placed a model

`properties_contents.placed_by` (MySQL migration 93, SQLite migration 76) is the character who placed the model:
written when a model is placed from an inventory or saved from brick by brick building. `NULL` (models placed before
the column, dashboard imports) means the owner. A model's behaviors are saved under its placer's character.

A model taken off the property (`DeleteModelFromClient`) goes to its placer:

| Taken off by | Placer in the world | Result |
|---|---|---|
| The placer | - | Into the placer's MODELS, as before (carried when picked up). |
| Another builder | Yes | Into the placer's MODELS, not carried. Both get a chat message. The builder gets `PlaceModelResponse` 16 (removed). |
| Another builder | No | Stays placed; the builder gets a chat message. |
| Another builder, taking it apart | - | Stays placed; only the placer takes their model apart. |

If the placer's inventory can't take it, the model stays placed. Nothing is deleted or mailed.
