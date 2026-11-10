# Edit mode (plumbing)

A private instance of a zone for editing the zone in the game client. The client loads the zone's objects from its own
files; the server never reads the client's files, so in a normal instance its own copies of the same objects are there
too. In an edit mode instance the server constructs none of them.

**Not exposed yet:** no command or packet from a client starts an edit mode instance. This is what exists for one to use.

## The flag

| Where | What |
|---|---|
| World -> master | `MasterPackets::CreatePrivateZone::editMode`, one more byte written only when set (`ZoneInstanceManager::CreatePrivateZone(..., editMode)`) |
| Master | `Instance::GetIsEditMode`; `InstanceManager::CreatePrivateInstance(..., editMode)` |
| Master -> world | The launch argument `-editmode 1` (`StartWorldServer`) |
| World | `dZoneManager::GetEditMode`, set before the zone loads |

A reload (`/reloadworld`, a zone file change) or a live update replaces a private instance with one that has the same
password and edit mode. A request to join a private instance that is still starting is answered once it is ready.

## What an edit mode instance loads

| Loaded | Not loaded |
|---|---|
| The zone file: spawn point, scenes, scene transitions, boundaries, paths (as data) | Level objects (`.lvl`), carver-only blockers |
| The terrain's scene map, the navmesh and the physics world | Spawners (LOT 176) and spawner networks (spawner paths) |
| The level files' revisions (the map checksum sent to the client) | Triggers (`.lutriggers`) |
| The zone control object, with the zone's own template | The zone control object's server script |
| The player, their items, pets and mounts; GM commands | Vanity objects, live event objects |

The zone control object stays: the client makes its own from the `ZoneTable` template under the same fixed ID
(`LevelManager::LoadZoneControlObject`, 0x01082660), the server's construction must match it, and server code expects
the object. Its script is replaced by one that does nothing. Its other components (an activity, a race) stay but are
idle: a race track's zone control does not take the player into a race.

The client waits on nothing a level object sends: it reports the level loaded when its own resource queue is empty and
finishes the login on its character and the server's "done loading" (`LWONetworkInterface::Run`, 0x00b32360).

## The map checksum

The client compares the checksum the server sends in `LOAD_STATIC_ZONE` with the one of its own files
(`LWONetworkInterface::iSendMessage`, 0x00b35870), and logs out with "invalid map version" when they differ. The
checksum is made of the zone and scene files' revisions. Edit mode does not change that: the server's copy of the zone
must have the revisions the client's has.

## Who gets in

The world server of an edit mode instance disconnects accounts below `editor_gm_level` (worldconfig.ini, default 9).
Master never sends anyone to a private instance from a normal transfer or matchmaking.

## The save's `edit` attribute

A character's save has `edit="1"` when its account's GM level is at least `editor_gm_level`, `0` for everyone else.
The client takes anything but 0 as the editor being enabled.
