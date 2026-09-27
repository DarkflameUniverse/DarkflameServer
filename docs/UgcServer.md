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

Player models (`ugc` rows) are made the way LU Toolbox (the Blender add-on the community makes LU models with) makes
them: its importer, Process Model, Bake Lighting (AO only) and its icon renderer, with its defaults as the settings'
defaults. The table below goes through it step by step.

1. The LXFML is read from `ugc.lxfml` (an sd0 stream). Parts come from `Bricks/Brick/Part` (LXFML 5: row-major
   rotation and translation per bone) or `Scene/Model/Group/Part` (LXFML 4: axis angle).
2. Each level of detail in `lods` (default `0,2`, as LU Toolbox imports) is built from the client's LDD primitives,
   `res/brickprimitives/lod<n>/<design>.g`, `.g1`, ... (sub-part `i` uses the part's `i`-th material, material 0 meaning
   the part's first). Colors come from LU Toolbox's palette (`color_palette=lu_toolbox`; `brickdb` uses the client's
   `Materials.xml`): LU's colors, the LDD colors LU doesn't have mapped onto the nearest LU one, unknown ones black. A
   brick is transparent only when all of its materials are; transparent bricks get `transparent_opacity` (58.82%).
3. Color variation: each material of each brick has its brightness shifted like LU Toolbox's "Apply Color Variation":
   the color's HSV value is taken to a 1/2.224 gamma, moved by a random amount of up to `color_variation`/200 (5%:
   0.025) either way, times the color's own amount (black 0.4, orange 1.5, ...), clamped and taken back; hue and
   saturation stay. The random number comes from the model's id, the brick's index and the material, so making a
   model again gives the same colors, and every LOD the same (LU Toolbox restarts its random sequence per LOD).
4. Faces nobody can see are removed from the opaque bricks (they're rendered from 42 directions and triangles that
   never show are dropped; transparent bricks hide nothing and aren't touched). `hsr_ground_plane=1` also drops what
   can only be seen from below.
5. Ambient occlusion is baked like LU Toolbox's Bake Lighting with AO Only: 64 rays per vertex (`ao_samples`) that count
   as blocked when they hit an opaque triangle within 5 (`ao_distance`); transparent bricks are neither baked nor
   occlude; glowing colors add their glow times 6 (Glow Strength 3 x Glow Multiplier 2). The light is multiplied into
   the vertex colors (the NIF has one color set; LU Toolbox keeps it in a "Lit" layer beside "Col").
6. The meshes are written as a Gamebryo 20.3.0.9 NIF (user version 0, the client's own version) laid out like LU
   Toolbox's exports and the game's own brick models (`res/BrickModels/ndmade`): the root `SceneNode_Model`, an
   `NiLODNode` `S01_Opaque_Model` (and `S01_Alpha_Model` for transparent bricks) with `NiRangeLODData` holding each
   level's distances (LU Toolbox's: with LODs 0 and 2, 0-100 and 100-10000), a node `LOD_<n>` per level and its
   shapes under it, named like the group. Shapes have vertex colors, a white material and, when transparent, alpha
   blending. Opaque shapes are divided at 65535 vertices along their longest side (LU Toolbox's divide_mesh);
   transparent bricks are one shape each unless `combine_transparent=1`. Vertices are in LDD's Y-up space with
   identity transforms, like the game's own brick models.
7. The icon is drawn from the finished `.nif`: it is read back with the same reader as the client's files (NifFile,
   LOD 0) and rasterized, so it shows exactly what the game shows, with the color variation, the faces that were
   removed and the lighting baked into the vertex colors (so the icon adds no occlusion of its own). The camera and
   lights are LU Toolbox's icon renderer's (its UGC render add-on's BrickBuild scene), drawn by a software rasterizer
   (no GPU, no display), 4x4 supersampled: a 50 mm lens (39.6 degrees) from 53.4 degrees around and 19.5 above,
   framed at 1.03, a sun of strength 2.5 from 21 degrees around and 50.3 above with soft shadows, and a grey (0.192)
   world light, on a transparent background: `icon.png` for the dashboard and a 32-bit `icon.dds` for the client.
8. `stats.json` records the bricks, each LOD's triangles before and after hidden faces were removed, vertices,
   shapes, how long each step took and the settings used; `model.noao.nif` is LOD 0 before the lighting bake. Both,
   and the icon and mesh of the version before (`previous.*`), are for the dashboard's viewer.

### Matching LU Toolbox

| LU Toolbox step (default) | UGC server |
| --- | --- |
| Import LXFML: LXFML 4/5, `brickprimitives/lod<n>`, sub-part materials, 0 = the part's first, missing bricks skipped | Same |
| Import LODs 0, 2 (LOD 1 off, LOD 3 doesn't exist in the client) | Same (`lods=0,2`); a design missing from a level uses the next more detailed one |
| Import: flex parts (several bones) bent per bone | Not done: flex parts are placed by their first bone |
| Import: custom normals from the `.g` files | Same |
| Import: LU palette colors, LDD colors mapped to LU ones, unknown ones black (26) | Same (`color_palette=lu_toolbox`) |
| Import: random scale for seams (its factor is 0, so none) | Same (none) |
| Keep UVs off (no UVs; decorations aren't imported) | Same: no UVs, no decorations |
| Combine Objects on (opaque bricks joined per LOD), Combine Transparent off | Same (`combine_transparent=0`) |
| Reset Orientation / Correct Orientation (Blender's Z-up) | Equivalent: LDD's Y-up with identity transforms, as the game's own brick models |
| Correct Colors off (the importer's colors are the palette already) | Same |
| Apply Color Variation on, 5%, per color amounts (CUSTOM_VARIATION) | Same (`color_variation=5`), stable per model, brick and material |
| Transparent Opacity 58.82% | Same (`transparent_opacity`) |
| Vertex color layers Col, Lit, Alpha (1), Glow | One color set: Col times Lit (glow added to Lit); alpha is the opacity |
| Setup Bake Material (VertexColor / VertexColorTransparent) | Equivalent: white NiMaterialProperty, vertex colors as ambient and diffuse, NiAlphaProperty on transparent shapes |
| Remove Hidden Faces: Cycles bakes with an overexposed world (VC pre-pass 32 samples, tris to quads, 5 pixels between vertices, 8 samples, threshold 0.01), autoremove, transparent bricks hidden | Same result by other means: depth renders from 42 directions (`optimize_resolution`), transparent bricks hidden and untouched; the pre-pass, quads and samples are details of Blender's baking |
| Use Ground Plane off | Same (`hsr_ground_plane=0`) |
| Split objects over 65536 vertices (divide_mesh, along the longest side, linked parts together) | Same, also keeping each shape under 65535 triangles (the format's limit) |
| Setup LOD data: SceneNode, NiLODNode per shape name, LOD nodes, near/far by the levels there are, `S01_Opaque_`/`S01_Alpha_` names cut at 60 | Same (`lod_distance_0..3`, `lod_cull`, `shader_opaque`); the glow, metal and superemissive shader settings aren't used by LU Toolbox either |
| Bake Lighting, AO Only: 64 AO samples, distance 5, transparent skipped, glow strength 3 x 2, smooth vertex colors | Same (`ao_samples`, `ao_distance`, `glow_strength`); smoothing averages a vertex's corners, and the occlusion is per vertex already |
| NifTools export for LU: 20.3.0.9, user version 0 | Same |
| Physics (`.hkx`) | Intentionally not done: `.hkx` requests answer 404, so the client makes its own |
| Icon: LOD 0 imported again with its own color corrections (white and black toned down) and no color variation | Different on purpose: the icon is drawn from the generated `.nif` (LOD 0), so it matches the game, variation and baked lighting included; no icon-only color corrections |
| Icon: Bevel Edges and Subdivide | Not done (the rasterizer draws the bricks as they are) |
| Icon: principled materials (roughness 0.16), hashed transparency, Cycles | Approximated: diffuse sun with soft shadow-mapped shadows plus world light (player models: their baked lighting; cars and rockets: ambient occlusion, `icon_ao`); no highlights or bounced light; transparent bricks sorted and blended |
| Icon scene BrickBuild / Car: 50 mm lens, camera 53.4 / 19.5 degrees, sun 2.5 at 21 / 50.3, world 0.192, 128 px, framing 1.03, transparent film | Same (`icon_*`, `modular_icon_*`) |
| Icon scene Rocket: 35 mm lens, other angles, two suns | Not done: rockets use the car camera |

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
ugc/models/<id % 1000>/<id>/model.nif, model.nif.gz, model.nif.checksum, model.lxfml.gz, model.lxfml.checksum,
                              icon.dds.gz, icon.dds.checksum, icon.png, model.noao.nif, stats.json,
                              previous.icon.png, previous.model.nif, previous.model.noao.nif, previous.stats.json
ugc/modular/<id % 1000>/<id>/icon.dds.gz, icon.dds.checksum, icon.png
```

A model's files are written to a temporary folder and renamed into place, so a half written model is never served.
When an item is made again, the icon, mesh and stats of the version before are kept as `previous.*` to compare.
`ugc_max_storage_mb` (default 2048, 0 for no limit) caps the folder: when it is over, the models whose files were used
longest ago are deleted. Their rows stay `is_optimized = 1`; when something asks for their files the server sets
them back to 0 and makes them again (the request answers 408 meanwhile), so deleted files are only made again when
wanted.

## Database state

Migrations `dlu/mysql/81_ugc_processing.sql` and `dlu/sqlite/64_ugc_processing.sql` (the MySQL one only adds columns
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

### Marking models for processing (for code that writes `ugc` rows)

* A new row needs nothing: `is_optimized` defaults to 0 (`InsertNewUgcModel` writes 0).
* When a model's LXFML changes, `UpdateUgcModelData` sets `is_optimized = 0, process_attempts = 0, process_error = ''`
  in the same statement. Anything that writes `ugc.lxfml` another way must do the same, or call
  `ResetUgcModelProcessing(id, false)`.
* Deleting the row is enough when a model is deleted; its files are left until the storage cap removes them.
* The same holds for `ugc_modular_build` (`InsertUgcBuild` rows start at 0; `ResetModularBuildProcessing`).

## Threads

The main thread owns RakNet (the master link), mongoose (HTTP) and the database. `worker_threads` (default: half the
CPUs) workers do only pure work: parse LXFML, build and write meshes and icons, compress files. They get their input
(LXFML text, module data) from the main thread and hand results back through a queue the main thread drains, which
then updates the database. Brick geometry and materials are loaded once and shared read-only (the cache has its own
lock).

## CPU and memory

Settings in `ugcconfig.ini` (and the dashboard's settings page), picked up while running when the config is reloaded:

* `worker_threads` (restart): how many models are made at once.
* `max_cpu_percent`: the workers together average at most this share of all CPU cores. The long loops (the renders
  for hidden faces, the occlusion rays, icons) account each thread's CPU time every few milliseconds against a budget
  that fills at that rate and sleep while it's overdrawn (UgcThrottle), so it holds for long jobs too and whatever
  `worker_threads` is. Short bursts (a quarter second) aren't slowed. 0: no limit.
* `worker_nice`: the workers' Linux scheduling priority (0 normal to 19), so the game servers go first.
* `max_memory_mb`: each job's memory is estimated from its brick count before it starts (the renders' buffers plus
  about 40 KB per brick per LOD); a job waits while the running ones and it together would be over the limit, and one
  bigger than the limit alone runs when nothing else does. 0: no limit. After each job the workers give freed memory
  back to the system.
* `max_model_bricks`: models with more bricks fail with "the model has N bricks, more than max_model_bricks (M)". 0: no
  limit.
* `pause_hours`: local hours in which no new jobs start, e.g. `18-23` or `22-6` (running ones finish).

`/status` reports the process's CPU use (percent of one core, and the core count), resident memory, the running jobs'
estimated memory and how often a job waited for memory, whether the workers were throttled in the last 5 seconds and
for how long in all, whether it is paused, and the limits. The traffic report (Diagnostics page) carries the gauges
`cpu_percent`, `memory_mb`, `job_memory_mb` and `throttled` besides the workers' ones.

Measured on a 16 core machine with 4 workers working through 15 items (models of 100 to 1500 bricks and cars): with
no limit the process used about 400% of one core (all 4 workers) and finished in about 27 seconds; with
`max_cpu_percent=12` (1.92 cores) it stayed at 190-195% in every 3 second sample through the backlog (one sample at
227% while a job finished) and finished in about 45 seconds. With `max_memory_mb=400` the running jobs' estimates
stayed under 375 MB (two jobs waited for memory) and the process peaked at 283 MB resident, back to 55 MB when idle.

## HTTP

`port` (default 2008) on `listen_ip` (default 0.0.0.0, the client has to reach it). All GET, no authentication (the
files are what every player on a property sees anyway):

* `<client_path>/UGCC<dc>/<dc><id>.lxfml.gz|.checksum`, `<client_path>/UGCC<dc>/3DOPTIMIZED/<dc><id>.nif.gz|.checksum`,
  `<client_path>/UGCC<dc>/IMAGE128DDS/<dc><id>.dds.gz|.checksum` for the client (`client_path`, default `/ugc`, is
  the client's `UGCSERVERDIR`). HKX answers 404. A model that exists but isn't made yet (or was evicted) is moved to
  the front of the queue and answers 408 so the client asks again.
* `/files/model/<id>/<file>` and `/files/modular/<id>/icon.png` for the dashboard (`icon.png`, `model.nif`,
  `model.noao.nif`, `stats.json` and their `previous.` versions), not cached by browsers since they change when an
  item is made again.
* `/status`: JSON with the queue length, what the workers are doing and totals since start.

Files are sent from disk (mongoose streams them, with its own ETag) with `Cache-Control: public, max-age=3600`.

`UgcServer --make-model <file.lxfml or sd0> <folder>` and `UgcServer --make-modular "1:4713+1:4714+1:4715" <folder>`
make one item's files into a folder without a database, for trying settings.

## Dashboard

The UGC server is a server like auth and chat: master starts it when `enable_ugc_server=1`, starts it again when its
link drops, passes settings reloads to it and waits for it on shutdown, and tells the dashboard about it in the server
list (enabled, connected, process ID). On the dashboard it shows on the home page (Server Status, and a UGC Server card
for `health_view`: up time, waiting/made/failed, busy workers and storage), on Server Health (uptime history and the
Servers table with its process memory and CPU), on Diagnostics (its packets and HTTP requests, from the traffic report
it sends every 5 seconds with its workers, totals and storage), in the `server` webhook alerts when it goes down or
comes back, in the System Log (`UgcServer_*.log`) and crash dumps (`Crash_UgcServer_<start time>_<pid>.log` in `dump_folder`), and in
Prometheus (`darkflame_ugc_up`, `darkflame_ugc_items`, `darkflame_server_ugc_*{server="ugc"}`). See docs/Dashboard.md.

The UGC Server page (`/ugc`, Server Admin menu; `properties_view` to look, the new `ugc_manage` permission to make
things again) reads the database: counts per state for models and for cars and rockets, and the items as a gallery of
their icons or a list (owner, state, attempts, last attempt, failure reason), filtered by kind, state and a search by
id or owner name, with buttons to make one item, the failed ones or everything again (these only reset the columns;
the UGC server picks the rows up).

Everything from the UGC server goes through the dashboard, which reaches it at `ugc_internal_url`
(`dashboardconfig.ini`; empty: `http://127.0.0.1:2008`, the same machine), so the page works from wherever the browser
is: `/api/ugc/server/status` (its `/status`, kept 2 seconds), `/api/ugc/files/<kind>/<id>/<file>` (kept 15 seconds)
and `/api/ugc/mesh/<id>?lod=&version=current|previous&ao=0|1` (its NIF converted by the dashboard's worker threads
with the scenery viewer's NifFile conversion). The fetches run on the dashboard's worker threads. `ugc_public_url` is
only used for an "open on the UGC server" link.

The status box shows the queue, workers, CPU (with its limit), memory, the jobs' estimated memory (with its limit) and
whether the workers are throttled or paused. Clicking an item opens the viewer: the generated NIF in 3D (any LOD, now
or before it was made again, with wireframe, vertex colors and baked lighting switches), the LXFML as built beside
it, the icon now and before, and the stats (triangles per LOD before and after hidden faces were removed, how many
were removed, vertices, shapes, timings, with the change since the version before).

### Finding creations and showing them elsewhere

**UGC Search** (`/ugc_search`, in the menu under Properties, `properties_view`; `?q=` fills the search in) finds
players' creations with `GET /api/ugc_links/search?q=`. A number matches the UGC / blueprint id, a placed model's
object id, the property it is placed on, the creator's character or account id, or a LOT (a model placed as that LOT,
or a car or rocket with that module); text matches the creator's character and account names, property names, the
name and description a player gave a placed model and the upload's file name. `owner:`, `property:`, `model:`, `id:`
and `lot:` search one field. Each result has its icon, state, creator and where it is: placed on a property (from
`properties_contents`), attached to a mail (a car's or rocket's subkey, a model item's blueprint in the attachment's
config) or in its creator's inventories (their saved XML); anything else is "Not found" (traded, sold or deleted).

Results, the property page and the character page link to a creation on the UGC page as
`/ugc?item=<id>&kind=model|modular`, for the page to open that item.

What the UGC server made shows on the pages that show a creation, to whoever may view that page:

* The property page: each player-built model's icon, state and link (`GET /api/ugc_links/property/<property id>`).
* The property 3D view: made models are drawn from their NIF (`GET /api/ugc_links/mesh/<ugc id>?property=`),
  switchable with **Generated models** (see docs/Dashboard.md); the rest from their LXFML.
* The character page's inventories: creations get their icon and link (`GET /api/ugc_links/character/<character id>`).
  A model item is known by its blueprint (`blueprintid`, which the item now keeps in its saved config as `x@bp`); a car
  or rocket by its subkey (its `ugc_modular_build` id).

Icons come from `GET /api/ugc_links/icon/model|modular/<id>` (the UGC server's `icon.png`, fetched by the dashboard
like the `/ugc` page's files). It is served with `properties_view`, for a creation one of your own characters made, or
with `?property=<id>` / `?character=<id>` naming a page you may view (properties: `properties_view`, or the owner with
`own_properties`; characters: `characters_view`, or your own) that holds it. Until the UGC server has made an icon the
pages show the item's own icon.

## Not done yet

* Worlds still send every model's LXFML to the client on property load and don't set `renderUserGen`, so the client
  keeps building its own meshes; switching them to the served NIFs (and sending `NotifyClientUGCModelReady` when a
  model is made) is the next step, and needs checking in game.
* HKX (physics) is not generated.
* The non-3D-services download path (world manifest packets) is not answered.
