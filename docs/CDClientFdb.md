# CDClient: the client's cdclient.fdb

## Reading rows from the fdb

ComponentsRegistry, ItemComponent and Objects read their rows from a memory-mapped copy of the client's `cdclient.fdb`
(`FdbReader`, `CDFdb`), shared between all server processes through the OS page cache. `CDServer.sqlite` stays the
source of truth: at load each table compares its rows in both files and reads the ids whose rows the cdserver
migrations changed from SQLite (`CDFdb::FindChangedKeys`). Other tables read `CDServer.sqlite` as before.

## Copies, not the client's file

No server opens the client's `<res>/cdclient.fdb`. Master copies it into `resServer`, named by the 64-bit FNV-1a hash of
its bytes, and makes the matching SQLite file:

| File | What |
| --- | --- |
| `resServer/cdclient-<hash>.fdb` | copy of the client's fdb; the hash is of the copy's bytes |
| `resServer/CDServer-<hash>.sqlite` | made from that copy with `FdbToSqlite` and every cdserver migration |
| `resServer/cdclient-current` | two lines: the fdb copy and the SQLite file every server opens |
| `resServer/CDServer.sqlite` | the file used before copies existed; still used when the pointer names it |

A new version of the fdb gets new names, so nothing is replaced or truncated while a process has it mapped or open.
Every file is written under a temporary name and renamed into place. Windows can't rename over or delete an open
file, and the content-addressed names avoid both. The client's file is only read and never locked, since it is copied
and not mapped.

On the first start with copies, master copies the fdb and points at the existing `CDServer.sqlite`. If the client's fdb
changed while master was down, master makes the new `CDServer-<hash>.sqlite` at startup. Master then runs the cdserver
migrations on whichever SQLite file is current, as before. Worlds read `cdclient-current` at startup
(`FdbSnapshot::Resolve`). If it is missing, or names a file that isn't there, they use `CDServer.sqlite` without an fdb.
A client with only packed files (no loose `cdclient.fdb`) keeps using `CDServer.sqlite`, and nothing is watched.

## Hot reload

Master checks the client's fdb every `cdclient_watch_seconds` (default 5, 0 = off). It compares the size and mtime and
starts a reload once they differ from the current version and have stayed the same for one poll, so a file still being
copied in isn't read halfway. A reload can also be started by:

* `/reloadcdclient` (GM 9, paired with the `cdclient_reload` dashboard permission);
* a `CDCLIENT_RELOAD` message with no names sent to master (what `/reloadcdclient` sends; master also accepts it from
  the dashboard).

A reload runs these steps:

1. **Worker thread (master):** copy and hash the file. If the hash is the current one, stop. Otherwise make
   `CDServer-<hash>.sqlite` on its own SQLite connection (convert, then every cdserver migration, recorded in its
   `migration_history`), and compare the old and new copies table by table (row count and a content hash). The worker
   never logs or touches RakNet, the game database, the shared CDClient connection or config.
2. **Main thread (master):** switch master's own CDClient connection and tables, write `cdclient-current`, log the
   changed tables (for example `Objects: 16012 -> 16015 rows`), and send `CDCLIENT_RELOAD` with both names to every
   ready world.
3. **Each world, between frames:** `CDClientDatabase::Reconnect` opens the new SQLite file before closing the old one,
   and `CDClientManager::Reload` empties every table, maps the new copy and loads again (the changed-row lists are
   rebuilt). The old table entries and the old fdb view are kept alive, so objects already spawned keep what they
   loaded, and objects made afterwards read the new data. The world logs how long the switch took.

Master keeps the current and previous copies and removes older ones after each reload and at startup. On POSIX, a
removed file that is still mapped stays readable until it is closed. On Windows the removal fails while a process maps
it, and master tries again next time.

## Limits

* The world reloads its cached tables on the main thread, so a reload costs about as long as the CDClient part of
  startup, once per reload.
* Data copied out of the tables into other caches (for example behaviors already built, or zone data read at world
  start) keeps the old values until those objects are made again.
* The UGC and dashboard servers don't switch on a reload yet; they read the current files when they start.
* The dashboard has no reload button yet; use `/reloadcdclient` or let master see the file change.
* A world that starts while master is making a new copy can read the old pointer and miss the broadcast; it catches up
  at the next reload or restart.
