# World hot reload

Replacing world instances when the zone files they loaded change on disk. The new instances load the files on disk
now; players are moved to them with the instance migration ([SeamlessTransfer.md](SeamlessTransfer.md)), the same
move a live update uses ([LiveUpdate.md](LiveUpdate.md)). Master keeps running, and nothing in the client's files or
the CDClient data is changed.

Code: `dCommon/ZoneFileLog` (what a world loaded), `dNet/master/WorldFiles.h` (messages),
`dMasterServer/WorldFileWatch.h` (watching and choosing, no master state; unit tested),
`dMasterServer/WorldReloader` (master's glue), `dDashboardServer/routes/WorldReloadRoutes` (dashboard).

## What a world reports

While it loads its zone, a world records each zone data file it reads, with its size and a 64-bit FNV-1a hash of the
bytes it read (taken once, from the buffer it already has):

| Kind | File | Recorded in |
| --- | --- | --- |
| `zone` | the `.luz` | `Zone::LoadZoneIntoMemory` |
| `scene` | every `.lvl` | `Level::Level` |
| `triggers` | every scene's `.lutriggers` | `Zone::LoadLUTriggers` |
| `terrain` | the `.raw` (only read for scene ghosting, format 30 and up) | `Zone::LoadSceneMap` |
| `navmesh` | `navmeshes/<zone>.bin` next to the server (hashed from disk after it is opened) | `dNavMesh::LoadNavmesh` |

A loose file is reported by its absolute path. A file read from the client's packs is reported by its name in the pack
and marked `packed`: it is listed, but not watched. Once ready (after `WORLD_READY`) the world sends the list to
master in `WORLD_FILES`. Master takes the zone, instance and clone from the instance it knows at that address.

## Watching

Master keeps one entry per path, shared by every instance that loaded it. Every `world_watch_seconds` (default 5,
0 = off) it:

1. reads (hashes) each file it has not read yet;
2. compares each file's size and mtime with the version it last read. A changed file is read once its size and mtime
   have stayed the same for one poll, so a file still being written isn't read halfway (`FdbSnapshot::Watcher`, as for
   `cdclient.fdb`). A file that disappears is marked missing; that alone is not a change.

Reading happens on a worker thread that only opens the files and hashes them; it never logs or touches RakNet, the
database, the CDClient connection or config. Master applies the results on its main thread.

An instance is **stale** when a file it loaded has another hash on disk now. This also catches a world that read a file
just before it changed: master's first read differs from the world's.

A touched file with the same bytes (same hash) is not a change.

## Reloading

After each poll master reloads the stale instances. Each is tried once per version of its files (a refusal or failure is
tried again when a file changes again, or on request). A reload can also be started by:

* `/reloadworld [zone, default this one] [warn seconds, 0-300, default 10]` (GM 9, paired with the `world_reload`
  dashboard permission): every instance of the zone, stale or not;
* the dashboard's **Reload** on the Instances page (`world_reload`), which sends the same `WORLD_RELOAD`.

What happens to each instance (`WorldFileWatch::Choose`):

| Instance | Action |
| --- | --- |
| Players there | Replaced: a new instance of the same zone and clone starts (a private instance keeps its password), the players are warned and moved, the old one stops once empty. A property is saved and frozen first (`MIGRATE_PREPARE`). |
| Empty, in a zone of `prestart_worlds` (public, clone 0) | A new instance starts, then the old one stops. Only one per zone, and none when a busy instance of the zone is being replaced anyway. |
| Empty | Stopped; a new instance starts when someone goes there. |
| Character selection, still starting, shutting down, or already being emptied | Left alone. An instance still starting loads the files on disk now and reports them. |

Moves use `MigrationCoordinator::Start` with the live update's options, so properties, private instances and activity
zones are moved too (an activity in progress is lost). The move is the Mythran shift (warning, then a short loading
screen); with `world_reload_seamless=1` it uses the experimental seamless mode instead (no loading screen; untested
with the real client). Automatic reloads warn players 10 seconds before they are moved.

The GM who asked hears how each move goes in chat. Master logs what changed (`World reload: <path> changed on disk`) and
what it did with each instance.

## Dashboard

The Instances page has a **World files** card (permission `world_reload`): for each zone that runs, its instances
(stale ones marked "old files", ones being replaced "being replaced"), the last reload, and its files with kind, size,
hash and a **changed on disk** badge. Zone names come from the client's locale (`GameText`). Master sends the status
(`WORLD_FILES_STATUS`) when something changes and when the dashboard connects; the page updates live on the
`world_files` socket topic.

API: `GET /api/worlds/files`, `POST /api/worlds/reload` `{zone, warnSeconds}`.

## Messages (appended to `MessageType::Master`)

| Message | Direction | Payload |
| --- | --- | --- |
| `WORLD_FILES` (47) | world → master | `WorldFilesReport` (zone, instance, clone; each file's kind, packed, size, hash, path) |
| `WORLD_RELOAD` (48) | world (GM) / dashboard → master | `WorldReloadRequest` (zone, warn seconds, requester, who) |
| `WORLD_FILES_STATUS` (49) | master → dashboard | `WorldFilesStatus` (settings; per zone its files, instances and last reload) |

Reload migrations get IDs from `0x80000000` up (live updates use `0xC0000000` up, worlds' own IDs are time based).

## Settings (masterconfig)

| Setting | Default | |
| --- | --- | --- |
| `world_watch_seconds` | 5 | seconds between checks; 0 turns watching off (`/reloadworld` and Reload still work) |
| `world_reload_seamless` | 0 | 1 = move players without a loading screen (experimental) |

## Not covered

* **In-place reloads.** A running world never loads new navmesh, scene objects, spawners, paths or triggers; it is
  always replaced by a new instance. Reloading those in place is out of scope for now.
* Files a world did not read are not watched: a navmesh or `.lutriggers` file added after the world started, terrain
  of zones that don't use scene ghosting, and files inside the client's packs.
* Other data worlds read at start (vanity files, `cdclient.fdb`, which has its own reload: [CDClientFdb.md](CDClientFdb.md))
  is not part of this.
* If master restarts, it knows no files until each world starts again (worlds report once, when ready).
* Master reads the paths worlds report, so worlds must run on the same machine as master (as master starts them).
