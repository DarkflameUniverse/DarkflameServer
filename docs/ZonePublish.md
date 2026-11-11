# Edited zone files

Taking a zone's edited files from an external level editor, keeping them apart from the client's files, and loading
them by replacing the zone's instances ([WorldHotReload.md](WorldHotReload.md)). The server's client files are never
written.

Code: `dCommon/ZonePublish` (checking a set), `dCommon/ZoneOverrideStore` (the override folder: apply, backups,
restore, clear), `dCommon/ZoneEditLock.h` (who may: lock and reload rules), `dCommon/dClient/AssetManager` (reading
overrides), `dDatabase/.../IZoneEditing.h` (locks and editors), `dWeb/MultipartFile` and `Web::RegisterUploadRoute`
(large uploads), `dDashboardServer/auth/ZonePublishAuth.h` (the API's gate),
`dDashboardServer/routes/ZonePublishRoutes` (the API), `dCommon/ZoneFilePack` and `dGame/dUtilities/ZoneEditor` (the
game client's level editor, on a world server). The first four know nothing of how files arrive: the API and the
world server use the same checks, store, locks and reload.

## Override folder

`zone_override_dir` (sharedconfig.ini; empty: off; relative to the servers' folder unless absolute).

| Path | Holds |
| --- | --- |
| `maps/<zone folder>/...` | the files in use |
| `maps/<zone folder>/.zone-<id>.json` | which files are zone `<id>`'s set, who sent it, when, its note |
| `staging/<id>/` | a set while it is checked and built |
| `backups/<zone id>/<unix time>-<id>/` | the sets a zone had before (`files/`, `manifest.json`) |

For a name under `maps/` the asset manager reads `<zone_override_dir>/maps/...` when the file is there, else the
client's. Names are lowercase with forward slashes. Master watches the same file a world would read: an override that
appears, changes or goes away makes the instances that read another version stale.

A zone's set is replaced whole. The folder's new contents are built in `staging`, flushed to disk and swapped in with
one rename (an exchange where the system has one). The set replaced is copied to `backups` first; the newest
`zone_publish_max_backups` are kept. Restore and clear are the same swap. Files of another zone in the same folder
stay.

## Who may

- An API key (`Authorization: Bearer`) whose own permission list has `zones_publish` (also written `zones.publish`
  when a key is made). A key with all of its owner's permissions does not count. Keys are stored as a SHA-256 hash.
- The key's owner at GM level `zone_publish_gm_level` or higher, read from the database on every request.
- HTTPS, or the server's own machine. The dashboard speaks plain HTTP: with `behind_proxy=1` the proxy's
  `X-Forwarded-Proto: https` counts; without it only loopback does. Anything else is refused and audited.
- Ten refused requests from one address in ten minutes refuse the address for the rest of that time, on top of the
  key's own rate limit.
- Every use and every refusal is in the audit log (action `zone_publish`: key ID, zone, action, result; the account is
  the entry's actor). Reads are written at most once a minute per key, zone and action.

Locks: a zone has up to 10 assigned editors and one lock holder. Taking the lock needs an assigned editor, or GM level
`zone_lock_admin_gm_level`. Releasing: the holder, or an admin. Sending, restoring and clearing need the caller to
hold the lock. Locks do not expire. Results where a number is given: 0 ok, 1 level too low, 2 not assigned, 3 locked
by another account, 4 no change.

## Checks

All files are checked before any is written; one failure refuses the set.

| Code | Meaning |
| --- | --- |
| `name_not_allowed` | Not a path in the zone's folder the `.luz` lists: `..`, absolute, backslash, NUL, non-ASCII, another zone's folder, an unlisted file |
| `duplicate` | The same path twice (compared in lowercase) |
| `missing_luz` | The set lacks the zone's `.luz` (its path comes from the ZoneTable) |
| `too_large` | A file over `zone_publish_max_file_mb`, the set over `zone_publish_max_total_mb` or `zone_publish_max_files` |
| `parse_failed` | The server's own reader does not read the file to its end (`offset`: where it stopped) |
| `unsupported_version` | `.luz` outside 30 to 41, `.lvl` over 48 |
| `scene_mismatch` | The `.luz` lists a scene the zone does not have in the client files, or one that is in neither the set nor the client |
| `terrain_mismatch` | The `.raw` sent is not the one the `.luz` names, or the one named is nowhere |
| `trigger_missing_target` | A trigger command without a `target` |
| `unknown_trigger_event`, `unknown_trigger_command` | An ID the server does not know |
| `bad_object_config` | An object setting that does not parse |
| `too_many_objects` | Over `zone_publish_max_objects` objects in the set's scenes |

Allowed names: the `.luz`, each listed scene's `.lvl` and `.lutriggers`, the `.raw` the `.luz` names. A set cannot add
scenes. Before a file is written its path is resolved (`std::filesystem::weakly_canonical`) and must be inside the
override folder.

## API

Base `/api/v1`. Errors are `{"error", "message"}`. `zoneId` is the ZoneTable ID.

| Route | Does |
| --- | --- |
| `POST /zones/{zoneId}/publish` | multipart/form-data: one file part per file (filename: its path relative to `res/maps/`), fields `reload` (default true), `dryRun`, `note`, `ifPlayersPresent` (`hold` default, `move`) |
| `GET /zones/{zoneId}/publish/{publishId}` | `{publishId, zoneId, action, state, players, reload: {instances, playersMoved, errors}, finishedAt}`; states `validating`, `applying`, `held`, `reloading`, `done`, `failed` |
| `POST /zones/{zoneId}/publish/{publishId}/reload` | `{"ifPlayersPresent": "move"}` goes on with a held reload |
| `GET /zones/{zoneId}/overrides` | `{current: {files: [{path, bytes, sha256, publishedBy, publishedAt}]} or null, backups: [{backupId, createdAt, createdBy, note}]}` |
| `POST /zones/{zoneId}/overrides/restore` | `{backupId, reload, ifPlayersPresent}`; answers `{publishId, zoneId}` at once |
| `DELETE /zones/{zoneId}/overrides` | optional `{reload, ifPlayersPresent}`; answers `{publishId, zoneId}` at once |
| `GET /zones/{zoneId}/lock` | `{holder, since, holder_is_you, editors}` |
| `POST /zones/{zoneId}/lock` | take (200 again for the holder, `holder_is_you: true`); 409 `{error: zone_locked, holder, since, holder_is_you: false}` |
| `DELETE /zones/{zoneId}/lock` | release (holder, or an admin) |

A publish answers `{publishId, zoneId, backupId, files: [{path, bytes, sha256, version}], reload: {state}}` with
`state` `started`, `held` or `skipped`. A dry run answers `{zoneId, dryRun: true, files, reload}` and keeps nothing.
Restore and clear answer with their `publishId` and are followed with the status route like a publish. A `failed`
status has `error` and `message` beside its state; reload problems are also in `reload.errors`. Every file part's form
field name is `file`. Names are compared without regard to case and kept lowercase, as the servers read them. Times
are unix seconds.

| Status | When |
| --- | --- |
| 400 | `malformed` |
| 401 | `unauthorized`, `api_key_required` |
| 403 | `https_required`, `scope_required`, `read_only_key`, `gm_level`, `lock_required`, `not_an_editor` |
| 404 | `unknown_zone`, `unknown_publish`, `unknown_backup` |
| 409 | `publish_in_progress`, `zone_locked`, `not_held` |
| 411 | `length_required` (no Content-Length) |
| 413 | `too_large` |
| 422 | `validation_failed` with `files: [{path, ok: false, errors: [{code, message, offset?}]}]` |
| 429 | `rate_limited`, or the key's own limit |
| 503 | `disabled` (no `zone_override_dir`), `master_unavailable` |

The publish request's headers are checked (key, level, transport, zone, lock, size) before any of its body is taken.
The body is written to `staging` as it arrives, not held in memory.

## Reload

Only the zone's instances, through master's world reload. With players in the zone and `ifPlayersPresent=hold` the
state is `held` until `.../reload` says to move them, or nobody is left when it is asked again with `hold`. From
before the files change until a reload of the zone is asked for, the dashboard tells master to leave the zone's
instances alone (`WORLD_RELOAD_HOLD`), so the file watch does not reload a held zone by itself. `reload=false` leaves
the instances as they are; the files are read when an instance next starts, or on a later reload.

The dashboard's Worlds page lists each zone's editors and lock holder (permission `zone_editors_manage` and GM level
`zone_lock_admin_gm_level`): add and remove editors, take a lock away.

## The game client's level editor

A world server answers the client's own level editor packets with the same backend. World packets, IDs as the
1.10.64 client has them:

| From the client | Holds | Answer |
| --- | --- | --- |
| 8 editor mode | u8 on | none; the server remembers it per connection |
| 9 reload map | nothing | 15, then the zone's reload |
| 10 push request | u8 fileType, i32 levelFileId | 18: u8 result, u8 fileType, i32 levelFileId, UTF-16 lockedBy with its null |
| 11 push | u8 fileType, i32 levelFileId, packed files | none; when the set is applied 15 to every client of the instance, then the zone's reload |
| 12 pull | u8 fileType, i32 levelFileId | 19: u8 fileType, i32 levelFileId, packed files |
| 13 lock | u8 lock (1 take, 0 give up) | 20: u8 result, 33 UTF-16 characters lockedBy, u8 locked (the client holds it now) |

fileType: 0 the whole map, 1 a scene, 2 the terrain, 3 the zone file, 4 a scene's triggers. levelFileId is the
scene's ID in the zone file for 1 and 4 when pulling; a push is matched by its file's name. Results: 0 ok, 1 the
account's editor level is too low, 2 not one of the zone's editors, 3 locked by another account (lockedBy says who; a
push without any lock held is 3 with a hint to lock first), 4 no change (lock only).

Packed files (`ZoneFilePack`): an entry is u32 nameBytes (UTF-16LE with its null), u32 dataBytes, the name, the data.
A whole map is the zone entry, the terrain entry, u16 N and N scene entries, u16 M and M trigger entries; any other
type is one entry. Data is always sd0, each chunk inflating to at most 256 KiB; anything else is refused. Only a
name's base name is used, lowercased into the zone's folder and checked like any other name. There is no checksum.

Gates, looked up on the server for every packet: the account's editor level (its GM level when at
`editor_gm_level`, else 0) at `zone_publish_gm_level` or above; one of the zone's editors or an admin to lock, pull or
reload; the lock's holder to push. A push is only taken after a push request for the same file type and ID was
answered with 0. A single file replaces that file in the zone's set and keeps the rest; a whole map replaces the set.
A push that is refused changes nothing and tells the sender why in chat. Every request is in the audit log (action
`zone_publish`, "in game").

While a client's editor is on it is never sent a zone load. The level load complete it sends when its editor ends is
answered as any other: its player is here, so the world is sent to it again. The reload after a push moves normal
clients to the new instance as any world reload does; a client with its editor on loses its connection there.

A character's editor level is told to the client in the character's save (`edit`), with the character (the editor
flag and level after the GM level) and in answer to the GetEditorLevel game message.
