# UGC Server

The UGC server turns what players build into the files the game client downloads for them: an optimized mesh
(`.nif`) and a 128x128 icon (`.dds`) for every brick-built model, and icons for modular builds (cars and rockets).
It also serves those files, and the models' LXFML, to the client over HTTP, so the dashboard never has to answer the
client's UGC traffic. It generates no physics (`.hkx`).

It is a separate process like auth, chat and the dashboard: the master starts it when `enable_ugc_server=1` (in
`masterconfig.ini`, default 0) and starts it again when its link to the master drops.

## What the client asks for

The client (1.10.64) has two ways of fetching UGC files, picked by `UGCUSE3DSERVICES` in its `boot.cfg`. Both use
`UGCSERVERIP`, `UGCSERVERPORT` and `UGCSERVERDIR` (default: the patch server's directory plus `/UserBrickModels`).
Resource types are 0 LXFML, 1 NIF, 2 HKX, 3 DDS.

* `UGCUSE3DSERVICES=7:1` (what this server supports). The folder becomes `UGCSERVERDIR/UGCC<datacenter>/` (the
  datacenter id is `DATACENTERID` in `boot.cfg`) and files are
  * `<folder><type folder><datacenter><blueprint id><.lxfml|.nif|.hkx|.dds>.gz`: the file, gzip compressed. The
    type folder is `3DOPTIMIZED/` for NIF and HKX, `IMAGE128DDS/` for DDS and nothing for LXFML.
  * the same name with `.checksum` instead of `.gz`: `<Checksum><MD5>32 hex digits</MD5><Filesize>n</Filesize></Checksum>`,
    the MD5 and size of the uncompressed file. The client checks the download against it.
  * HTTP 408 makes the client try again later; other errors are logged as failed downloads.
* Without 3D services the client asks the world server for each file's MD5 (`REQUEST_UGC_MANIFEST_INFO`, answered
  with `UGC_MANIFEST_RESPONSE`) and downloads `BrickModels/UserMade/<id % 1000, 3 digits>/<id, 20 digits><ext>.sd0`.
  That needs the world to answer the manifest requests, which it doesn't yet, so it is not supported.

A model's render component uses the downloaded NIF when the model's spawn data has `renderUserGen=1` with its
`blueprintid`; `NotifyClientUGCModelReady` (game message 909) makes the client fetch the blueprint's NIF and HKX
again. Worlds don't send either yet (they still send every model's LXFML when a property loads and the client builds
the models itself); see "Not done yet".

Client settings for a server at 203.0.113.5 with the default port:

```
UGCUSE3DSERVICES=7:1,
UGCSERVERIP=0:203.0.113.5,
UGCSERVERPORT=1:2008,
UGCSERVERDIR=0:/ugc,
DATACENTERID=1:150,
```

## Processing

Player models (`ugc` rows):

1. The LXFML is read from `ugc.lxfml` (an sd0 stream). Parts come from `Bricks/Brick/Part` (LXFML 5: row-major
   rotation and translation per bone) or `Scene/Model/Group/Part` (LXFML 4: axis angle).
2. Brick geometry is the client's LDD primitives, `res/brickprimitives/lod<n>/<design>.g`, `.g1`, ... (sub-part `i` uses
   the part's `i`-th material, material 0 meaning the part's first). Colors and opacity come from `Materials.xml` in
   `res/brickdb.zip`.
3. The mesh is built like LU Toolbox's "Process Model": every brick merged into opaque and transparent meshes with
   the material colors as sRGB vertex colors, faces nobody can see removed (the opaque mesh is rendered from 42
   directions and triangles that never show are dropped; transparent bricks don't hide anything) and lighting baked
   into the vertex colors as ambient occlusion computed from the same renders.
4. The meshes are written as a Gamebryo 20.3.0.9 NIF (user version 0, the client's own version): a root node with
   one `NiTriShape` per piece, named `S01_Opaque_...` / `S01_Alpha_...` like LU Toolbox names them, each with vertex
   colors, a material and, for transparent pieces, alpha blending. Pieces are split at 65535 vertices/triangles.
5. The icon is rendered by a software rasterizer (no GPU, no display), 4x4 supersampled, 3/4 view from the front
   left, framed to fit, on a transparent background: `icon.png` for the dashboard and a 32-bit `icon.dds` for the
   client.

Modular builds (`ugc_modular_build` rows, `ldf_config` like `1:4713+1:4714+1:4715`):

1. Each module LOT's `ModuleComponent` (component type 28) gives its part code and build
   type; the build type's `ModularBuildComponent.xml` gives the topology (root part, and which part connects to which
   named location) and `Placement/AdditionalModelRotation`.
2. Each module's mesh is its render asset (`RenderComponent.render_asset`, the NIF the client assembles in game). A
   connection places the connecting part so its node with the location's name (when it has one) sits on the parent
   part's node of that name, or its origin on that node; `ModuleComponent.xml`'s `connection` translation is used when
   the parent's NIF has no such node. (Module LXFMLs in `res/BrickModels` exist for only some modules and are
   authored in different spaces, so they aren't used.)
3. The assembled mesh gets an icon like a model's. No mesh is written: the client assembles modular builds itself.

## Storage

Files live under `ugc_output_dir` (default `ugc` next to the server binaries):

```
ugc/models/<id % 1000>/<id>/model.nif(.gz, .checksum), model.lxfml(.gz, .checksum), icon.dds(.gz, .checksum), icon.png
ugc/modular/<id % 1000>/<id>/icon.dds(.gz, .checksum), icon.png
```

A model's files are written to a temporary folder and renamed into place, so a half written model is never served.
`ugc_max_storage_mb` (default 2048, 0 for no limit) caps the folder: when it is over, the models whose files were used
longest ago are deleted and marked unprocessed, and are made again the next time they are asked for.

## Database state

Migrations `dlu/mysql/80_ugc_processing.sql` and `dlu/sqlite/63_ugc_processing.sql` (the MySQL one only adds columns
that aren't there yet).

`ugc`: `is_optimized` (existing) is 0 until the model has been processed, 1 once its files are made, 2 when processing
failed. New: `processed_at` (Unix seconds of the last attempt), `process_attempts`, `process_error` (the last failure's
reason, empty when none). `bake_ao` (existing) records whether lighting was baked into the model. Changing a model's
LXFML (`UpdateUgcModelData`) sets it back to unprocessed.

`ugc_modular_build`: new `is_optimized`, `processed_at`, `process_attempts`, `process_error`, meaning the same.

The database is the queue: the UGC server looks for rows with `is_optimized = 0` every `poll_interval_ms` (default
2000), newest first, so worlds need no change to have new models processed. Rows that failed are tried again up to
`max_attempts` (default 3) times. Reprocessing (dashboard) sets rows back to `is_optimized = 0, process_attempts = 0`.
No master messages are needed for any of it.

## Threads

The main thread owns RakNet (the master link), mongoose (HTTP) and the database. `worker_threads` (default: half the
CPUs) workers do only pure work: parse LXFML, build and write meshes and icons, compress files. They get their input
(LXFML text, module data) from the main thread and hand results back through a queue the main thread drains, which
then updates the database. Brick geometry and materials are loaded once and shared read-only (the cache has its own
lock).

## HTTP

`port` (default 2008) on `listen_ip` (default 0.0.0.0, the client has to reach it). All GET, no authentication (the
files are what every player on a property sees anyway):

* `<client_path>/UGCC<dc>/<dc><id>.lxfml.gz|.checksum`, `<client_path>/UGCC<dc>/3DOPTIMIZED/<dc><id>.nif.gz|.checksum`,
  `<client_path>/UGCC<dc>/IMAGE128DDS/<dc><id>.dds.gz|.checksum` for the client (`client_path`, default `/ugc`, is
  the client's `UGCSERVERDIR`). HKX answers 404. A model that exists but isn't made yet (or was evicted) is moved to
  the front of the queue and answers 408 so the client asks again.
* `/files/model/<id>/icon.png|model.nif|model.lxfml` and `/files/modular/<id>/icon.png` for the dashboard's previews
  (with `Access-Control-Allow-Origin: *`).
* `/status`: JSON with the queue length, what the workers are doing and totals since start.

Files are sent from disk (mongoose streams them) with `Cache-Control: public, max-age=86400` and an ETag of the MD5.

## Dashboard

The UGC page (`/ugc`) reads everything from the database: counts per state, a paged list of models and modular builds
with their state, attempts, last error and owner, and buttons to reprocess one model, all failed ones, or everything
(these only change the database columns). Previews (icon, mesh, LXFML) load from the UGC server's `/files/` URLs;
`ugc_public_url` in `dashboardconfig.ini` says where it is (default `http://<dashboard host>:2008`).

## Not done yet

* Worlds still send every model's LXFML to the client on property load and don't set `renderUserGen`, so the client
  keeps building its own meshes; switching them to the served NIFs (and sending `NotifyClientUGCModelReady` when a
  model is made) is the next step, and needs checking in game.
* HKX (physics) is not generated.
* The non-3D-services download path (world manifest packets) is not answered.
