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
* `UGCUSE3DSERVICES=7:0` (the client's default, without 3D services). The client asks its world for each file's MD5
  and size (`REQUEST_UGC_MANIFEST_INFO`, answered with `UGC_MANIFEST_RESPONSE`, below) and downloads
  `UGCSERVERDIR/BrickModels/UserMade/<id % 1000, 3 digits>/<id, 20 digits><.lxfml|.nif|.hkx|.dds>.sd0`: the file as
  an sd0 stream (the bytes `s` `d` `0` 0x01 0xff, then chunks of a u32 size and zlib data, each inflating to at most
  256 KiB). It inflates it, saves it as `res/BrickModels/UserMade/<id % 1000>/<id><ext>` and checks its MD5 against
  the world's answer; a file it has already is only downloaded again when its MD5 differs. HTTP 408 makes it try
  again later. The worlds answer from checksums the UGC server stores (`ugc_file_checksums`), and the UGC server serves
  the `.sd0` files; see "Without 3D services" below for what keeps this off by default.

### The manifest packets (checked in the client)

Both are plain packets (not game messages); offsets are after the 0x53 byte and the header (connection type, message
id, a padding byte).

| Packet | Layout |
| --- | --- |
| `REQUEST_UGC_MANIFEST_INFO` (world message 27), client to world | u64 blueprint id, u8 resource type (16 bytes after the 0x53). Sent by `SendRequestUGCManifestInfoPacket` when the client needs a blueprint's file and 3D services are off. |
| `UGC_MANIFEST_RESPONSE` (client message 60), world to client | u64 blueprint id, u8 resource type, then 21 bytes of manifest info: u8 valid, u32 size of the inflated file, 16 bytes MD5 of it. The client ignores an answer that isn't exactly 37 bytes after the 0x53 (`PacketHandler_MSG_CLIENT_UGC_MANIFEST_RESPONSE`). |

The answer is cached per blueprint and type (the same message the 3D services `.checksum` download produces). With
it, the client uses a file it has when its MD5 matches and downloads it otherwise; with valid 0 it uses a file it has as
it is and downloads it only when it has none. Structs: `WorldPackets::RequestUgcManifestInfo`,
`ClientPackets::UgcManifestResponse` (`eUgcResourceType`).

The blueprint id of an inventory item's icon (`LWOInventoryComponent_Client::LoadBlueprintIcon`) is the item's
`blueprintid` config when it has one (Brick-by-Brick models, LOT 6662), else its subkey (cars and rockets: their
`ugc_modular_build` id, which DLU gives them as subkey when they are built). Cars and rockets from before builds were
stored have subkey 0 and no build row, so the client never asks for their icons.

A placed model (LOT 14) always loads its blueprint's NIF, HKX and LXFML through these requests; see "Models without
3D services" for how the worlds answer them.

Client settings (`boot.cfg`) for a server at 203.0.113.5 with the default port, with 3D services:

```
UGCUSE3DSERVICES=7:1,
UGCSERVERIP=0:203.0.113.5,
UGCSERVERPORT=1:2008,
UGCSERVERDIR=0:/ugc,
DATACENTERID=1:150,
```

and without them (the client's default mode; `ugc_manifest=1` on the server):

```
UGCSERVERIP=0:203.0.113.5,
UGCSERVERPORT=1:2008,
UGCSERVERDIR=0:/ugc,
```

**Every line of `boot.cfg` needs its trailing comma.** One line without it (e.g. `AUTHSERVERPORT=1:1500`) makes the
client reject the whole file: its log says "No boot configuration found; it's likely that the working directory is
incorrect." and it silently uses its built-in defaults for everything (the patch and UGC servers at
`http://127.0.0.1:80/lwoclient`). The client reads `boot.cfg` only when it starts. `UGCSERVERIP`, `UGCSERVERPORT` and
`UGCSERVERDIR` default to `PATCHSERVERIP`, `PATCHSERVERPORT` and `PATCHSERVERDIR` + `/UserBrickModels`
(`LWOResMgr2Interface::DownloadThread_Run`, 0x01058aa0). A UGC server on the same address and port as the patch server
shares its connection ("UGC site Info: Sharing main download connection" in the client's log); otherwise the log shows
"UGC site Info: Host '...' - Connected '...'".

### What the 1.10.64 client does in practice (checked in the client)

* With `UGCUSE3DSERVICES=7:1` the client downloads a property's models (`.lxfml.checksum`, `3DOPTIMIZED/*.nif.checksum`
  and `*.hkx.checksum` for each). Every failed download is reported to the world as `UgcDownloadFailed` (world message
  120), and the models then don't show (the server makes no `.hkx`, so this mode isn't usable for models).
* With `UGCUSE3DSERVICES=7:0` (the client's own default) the client builds the models itself from the LXFML the world
  sends, and property models load. Opening the backpack's models asks the world for each car's, rocket's and model's
  icon manifest. With `ugc_manifest=1` the world answers (checked: the answers are read and the client goes on to
  download each icon from `BrickModels/UserMade/<bucket>/<id>.dds.sd0` under its `UGCSERVERDIR`).
* Earlier tests concluded that the client ignores the `UGCSERVER*` and `PATCHSERVER*` lines of `boot.cfg` and always
  downloads from `http://127.0.0.1:80/lwoclient`. That was wrong: the test clients' `boot.cfg` had a line without its
  trailing comma, so the client rejected the whole file and used its defaults (see the settings above). The UGC
  server still serves `/lwoclient/UserBrickModels/...` as well as `client_path`, for clients left on the defaults.
* A download that can't connect (HTTP status 0) counts as a UGC connection failure, and the client logs the player out
  for it ("connection failed downloading UGC assets", `MainThread_LogoutDueToConnectionFailures`, 0x0102b930). Any
  other failure (404, ...) only loses that file and is reported as `UgcDownloadFailed`.
* A NIF the UGC server makes, put in place of one of the game's own models and spawned, renders in the client with its
  colors: the NIFs are written like the game's `res/BrickModels/ndmade` files (nif.xml's 20.3.0.9, user version 0;
  every shape with a material, alpha blending by the vertex alpha, specular off and vertex colors, in that order).
* The server logs every client download with its answer (`Client download <path> -> <status>`), and answers the LXFML
  from the database, so it is never waited for or evicted.

## Processing

Player models (`ugc` rows) are made the way LU Toolbox (the Blender add-on the community makes LU models with) makes
them: its importer, Process Model, Bake Lighting (AO only) and its icon renderer, with its defaults as the settings'
defaults. The table below goes through it step by step.

1. The LXFML is read from `ugc.lxfml` (an sd0 stream). Parts come from `Bricks/Brick/Part` (LXFML 5: row-major
   rotation and translation per bone) or `Scene/Model/Group/Part` (LXFML 4: axis angle).
2. Each level of detail in `lods` (default `0,2`, as LU Toolbox imports) is built from the client's LDD primitives,
   `res/brickprimitives/lod<n>/<design>.g`, `.g1`, ... (read like the game does: loose files first, then the client's
   packs, so packed clients and bricks added to them work) (sub-part `i` uses the part's `i`-th material, material 0 meaning
   the part's first). Colors come from LU Toolbox's palette (`color_palette=lu_toolbox`; `brickdb` uses the client's
   `Materials.xml`): LU's colors, the LDD colors LU doesn't have mapped onto the nearest LU one, colors LU Toolbox doesn't know but the
   client's `Materials.xml` has (colors added to the brick database) from `Materials.xml`, unknown ones black. A
   brick is transparent only when all of its materials are; transparent bricks get `transparent_opacity` (58.82%).
   `transparent_colors` (default 129; `none`: no colors) names colors that are transparent whatever `Materials.xml`
   says (129, "Tr. Bright Bluish Violet with Glitter", has alpha 255 there); a color named there gets
   `transparent_opacity`.
   `color_brightness` (percent, default 100: unchanged) scales the models' colors after the variation below, not
   the icons'.
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
   framing is LU Toolbox's icon renderer's (its UGC render add-on's BrickBuild scene: a 50 mm lens, 39.6 degrees, from
   53.4 degrees around and 19.5 above, framed at 1.03, the sun from 21 degrees around and 50.3 above). The light (world
   light, a fill from the camera, the sun with soft shadows, a highlight, exposure and contrast) is set so the icons are
   as bright as the game's own model icons (`res/textures/ui/inventory/models`: mean luminance 120 of 255 over 150 of
   them; ours 118 on a set of player models). Drawn by a software rasterizer (no GPU, no display), 4x4 supersampled,
   on a transparent background: `icon.png` for the dashboard and an `icon.dds` for the client, written like
   the client's own 128x128 icons (DXT5, no mipmaps, header flags 0x81007 with the linear size, caps 0x1000).

   The light settings are `icon_world_light`, `icon_sun_light`, `icon_fill`, `icon_specular`, `icon_shininess`,
   `icon_exposure`, `icon_contrast`, `icon_shadow_strength` and `icon_ao_strength` (new names: the older
   `icon_ambient`, `icon_sun_strength` and `icon_shadows` lines of existing ugcconfig.ini files, with the darker
   values, are no longer read). Every framing and light value (key, `icon_*` setting, range, default) is listed once in `UgcIconParams`; the
   settings, the dashboard's settings page and its icon editor are built from that list. Values come from the settings,
   then the kind's preset (player models, or a car or rocket build type from the client's `ModularBuildComponent`),
   then the item's own (a model, or a combination of car or rocket modules), the last two in `ugc_icon_settings`.

   **The pose.** The camera, the model's turn and the crop are worked out in `UgcIconPose` (shared with the
   dashboard's editor, see below), in this order: the model is turned about its origin by `modelYaw` (around +Y),
   then `modelPitch` (around +X), then `modelRoll` (around +Z), i.e. R = Ry * Rx * Rz (three.js's Euler order `YXZ`;
   settings `icon_model_yaw`, `icon_model_pitch`, `icon_model_roll`, all 0 by default so icons stay as they were). A car
   or rocket is first turned by its build type's `AdditionalModelRotation`. The camera then looks at the centre of the
   turned model's bounds from `yaw` (around +Y, from +Z towards +X) and `pitch` (up), as far away as makes the bounding
   sphere fill the field of view `fov` (a perspective projection, the camera's up is +Y). Last the picture is cropped
   to the model's projected bounds: scaled so their larger side fills the icon divided by `margin` (1 fills it, more
   leaves a border), centred, then moved by `offsetX` and `offsetY` (shares of the icon's width and height). The
   camera's distance follows from the field of view and the border, and the shift is the target moved in the picture,
   so these parameters describe every pose the icon can have.
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
| Setup LOD data: SceneNode, NiLODNode per shape name, LOD nodes, near/far by the levels there are, `S01_Opaque_`/`S01_Alpha_` names cut at 60 | Same (`lod_distance_0..3`, `lod_cull`, `shader_opaque`); LU Toolbox's glow, metal and superemissive shader settings are unused by it too; the UGC server's own metal and glow groups are opt in (see below) |
| Bake Lighting, AO Only: 64 AO samples, distance 5, transparent skipped, glow strength 3 x 2, smooth vertex colors | Same (`ao_samples`, `ao_distance`, `glow_strength`); smoothing averages a vertex's corners, and the occlusion is per vertex already |
| NifTools export for LU: 20.3.0.9, user version 0 | Same |
| Physics (`.hkx`) | Intentionally not done: `.hkx` requests answer 404, so the client makes its own |
| Icon: LOD 0 imported again with its own color corrections (white and black toned down) and no color variation | Different on purpose: the icon is drawn from the generated `.nif` (LOD 0), so it matches the game, variation and baked lighting included; no icon-only color corrections |
| Icon: Bevel Edges and Subdivide | Not done (the rasterizer draws the bricks as they are) |
| Icon: principled materials (roughness 0.16), hashed transparency, Cycles | Approximated: world light, camera fill, sun with soft shadow-mapped shadows and a highlight, exposure and contrast, matched to the game's own icons' brightness; no bounced light; transparent bricks sorted and blended |
| Icon scene BrickBuild / Car: 50 mm lens, camera 53.4 / 19.5 degrees, sun at 21 / 50.3, 128 px, framing 1.03, transparent film | Same framing (`icon_*`); the light is brighter, to match the game's icons |
| Icon scene Rocket: 35 mm lens, other angles, two suns | Not built in; a preset for the rocket build type can be set in the icon editor |

### Metal and glow (on by default, not how live looked)

Live's models, LU Toolbox's exports and the client's own builder (`LUNifBuilder_BK`, which writes only `S01_Opaque`
and `S01_Alpha`) all draw every brick with the LEGO shader, so metal colors look like grey plastic, glowing colors
like bright plastic and glitter like plain transparent plastic. The UGC server gives them the client's metal,
emissive and animated UV shaders by default. Set the four shader ids to 0 (and `satin_colors` to `none`) for live's
look: off writes exactly the files it wrote before these settings existed (the same bytes, tested).

How the client picks the shader (checked in the 1.10.64 client; Ghidra bookmarks under "UGCShaders"): player models
(LOT 14, and 6662) have RenderComponent shader 100, mapShaders "Multishader" (gameValue 9999). For a downloaded model
`LWOBaseRenderComponent::WrapMultishaderNodes` (0x00c0d370) wraps each `NiLODNode` (or bare `NiGeometry`) of the
.nif, and `AddObjectToRenderPipe` (0x00cfbdb0) reads the wrapped node's name with `sscanf("S%d")` (else `"_S%d"`
after the first `_S`): the number is a mapShaders id, and its gameValue is the shader. A gameValue outside 3..108
falls back to 5 (LEGO) and logs "Multishaded node ... malformed name". There is one shader per `NiLODNode`, shared by
all of its levels, so each look needs a group of its own.

| Setting (`ugcconfig.ini`, dashboard: UGC models) | Default | What it writes |
| --- | --- | --- |
| `shader_metal` | 88 | `S<id>_Metal_Model` for metal colors: 88 is Polished Metal (gameValue 98). The client loads `textures/metal/metal_reflection_polished.dds` itself and tints it by the vertex color (`Metallic.fx`, `Technique_Lighting_PolishedMetal_VertColor`). |
| `shader_brushed` | 89 | `S<id>_Brushed_Model` for brushed steel colors: 89 is Brushed Steel (gameValue 99; it loads `metal_reflection_brushed.dds` and `_noise.dds`, the noise in object space). The textures are registered by the client (`RegisterBrushedSteelTextures`, 0x00467090) as global shader textures 6 and 7, so the .nif needs none. The client's Materials.xml has no brushed types, so this needs `brushed_colors` or a Materials.xml that names them. |
| `shader_glow` | 46 | `S<id>_Glow_Model` for opaque glowing colors: 46 is LEGO-Emissive (gameValue 53), which draws `lerp(lit, vertex color, vertex alpha * material emissive red)`, opaque. |
| `glow_emissive` | 1 | The glow shapes' `NiMaterialProperty` emissive (grey): how far the shader goes from lit to the plain color. |
| `metal_material_types` | `shinySteel` | Materials.xml `MaterialType`s that are metal (empty: the default; `none`: none). |
| `brushed_material_types` | `brushedSteel,matteSteel` | Materials.xml `MaterialType`s that are brushed steel. |
| `brushed_colors` | 298,300,1002,1004 (the drum lacquered colors) | LEGO color ids that are brushed steel whatever their type; they win over the metal and glow colors. Empty: the default; `none`: no colors. |
| `shader_glitter` | 21 | `S<id>_Glitter_Model` (opaque) and `S<id>_GlitterAlpha_Model` (transparent) for glitter colors: 21 is LEGO-AnimUV (gameValue 30), see Glitter below. |
| `glitter_material_types` | `glitter` | Materials.xml `MaterialType`s that are glitter. |
| `glitter_colors` | 114,117 | LEGO color ids that are glitter whatever their type (as `brushed_colors`). The default: the two colors LEGO's own color data (Studio's color categories, "Glitter Colors") files as glitter that the client's Materials.xml types `shinyPlastic` (114 Tr. Medium Reddish-Violet w. Glitter, 117 Transparent Glitter). |
| `glitter_size` | 1.6 | The glitter texture's tile, in model units (a stud is 0.8): the flecks' spacing, the same on every brick. |
| `glitter_density` | 50 | Flecks in one tile. |
| `glitter_speed` | 1 | How fast the flecks drift: a tile in U in 7 s and in V in 11 s at 1; 0 keeps them still (no controllers). |
| `satin_colors` | 360,362,363,364,365,366,367,376 | Satin (opal) colors, see Satin below. The default: LEGO's color data's "Satin Colors" category (the Transparent ... Opal colors). Empty: the default; `none`: off. |
| `satin_opacity` | 75 | Percent: the vertex alpha of transparent satin bricks, instead of `transparent_opacity` or the Materials.xml alpha. |
| `satin_whiten` | 20 | Percent: how far satin colors are moved towards white (in linear RGB, after the color variation). |

Which color has which look is data, not a list in the code (`UgcModel::LookOf`): glow is LU Toolbox's glow table
(`UgcPalette::Glow`: 50, 294, 329, 9000-9027), metal is LU Toolbox's metallic table (`UgcPalette::IsMetallic`) plus
the Materials.xml types above (the clients checked have 8 or 14 `shinySteel` colors, and 1 or 3 `glitter` ones: 129,
341, 351), glitter is the `glitter` type plus `glitter_colors`. Pearl stays plastic (the client has no shader for it).
Only opaque bricks get metal and glow: a transparent glowing color (294 with the brick database palette, alpha 150)
stays in `S01_Alpha_Model`. Transparent bricks can be glitter (every glitter color the clients have is transparent:
341 and 351 have alpha 150, 129 is in `transparent_colors`).

What is written with a group on: per LOD, the opaque bricks are split by look before being divided at 65535 vertices,
and the .nif gets, in order, `S01_Opaque_Model`, `S88_Metal_Model`, `S89_Brushed_Model`, `S46_Glow_Model`,
`S21_Glitter_Model`, `S01_Alpha_Model` and `S21_GlitterAlpha_Model` (transparent glitter bricks, one shape per brick
like the other transparent ones, or one with `combine_transparent`), each only when it has triangles, and each with every LOD level (an empty `LOD_<n>` node where it has
none there), like the plastic groups. Metal shapes are like plastic ones (white material, no textures, the brick color
as vertex color with the lighting baked in). Glow shapes get a material of their own with emissive `glow_emissive`,
vertex alpha 1 (the shader's mask) and their plain color, not the baked one: the shader lights them itself, and the
glow added by the bake would glow twice. `stats.json` lists each LOD's triangles per group. `model.noao.nif` has the
same groups.

Turning a setting on or off changes only models made afterwards: the ones made already keep their look until they are
made again, with the UGC page's **Make everything again** (or Reprocess on one model); nothing is remade on its own.

The icon renderer and the dashboard know the groups: the icon reads each shape's tag back (the settings' ids and the
client's 88, 89 and 46) and draws glow at its plain color (by `glow_emissive`, unlit) and metal with a dimmed diffuse
light, a sky over dark ground reflection tinted by its color and a sun highlight (sharp for polished, broad for
brushed). This is an approximation of the game's environment maps. `NifFile::ShaderLookFor` gives 98 `REFLECTIVE`,
99 `REFLECTIVE | BRUSHED` and 53 `EMISSIVE`; the UGC page's 3D view gets each mesh's look (`/api/ugc/mesh`, "look")
and draws metal as reflective (metalness 1, the view's environment) and glow unlit, and the zone views draw
LEGO-Emissive objects going to their vertex color by its alpha (metal there stays lit like the rest).

#### Glitter

The client has no glitter shader. LEGO-AnimUV (mapShaders 21, gameValue 30, `LEGOPPLighting.fx` and its `_low`,
`_noenv`, `_noenv_nospec` versions) is the LEGO lighting with the UVs multiplied by `TEXTRANSFORMBASE` (the base map's
texture transform) in the vertex shader. A shape with vertex colors and a base texture gets
`Technique_LEGOPPLightingVertColorTextured_AnimUV` (technique names set up at 0x010ac110), whose pixel shader
(`LEGOPPLighting_PS_VertColorTextured`) is `lerp(vertex color, texture rgb, texture alpha)`, then the LEGO lighting
(`LEGOPP_PixelCommon4`), alpha = vertex alpha times the fade. So a white texture with flecks in its alpha puts white
flecks on a brick that is otherwise lit as plastic, and moving the texture transform moves them.

What a glitter shape has, beside what plastic shapes have (white material, alpha, specular, vertex colors):

- A UV set: each vertex's position on the axis plane its normal faces most, divided by `glitter_size`
  (`UgcGlitter::Uv`), so the flecks are as dense on every brick and every side.
- An `NiTexturingProperty` (one per file, shared by both glitter groups): apply mode decal (fixed function would do
  what the shader does), 9 slots, the base map only: wrap S and T, trilinear, UV set 0, a texture transform
  (translation 0, scale 1, Maya method, center 0.5).
- Its source, stored in the file as the client's own animated textures store theirs
  (`res/mesh/env/env_ag_ocean-maelstrom.nif`, RenderComponent 14356): `NiSourceTexture` (use external 0, name
  `ugc_glitter.dds`, pixel layout 6, mipmaps 2, alpha 3, static, persist render data) and
  `NiPersistentSrcTextureRendererData`: RGBA 32 bit, channels blue, green, red, alpha, platform DX9, 128 x 128 with 8
  mipmaps. RGB is white; the alpha is `glitter_density` soft dots (radius 1.2 to 2.2 px, peak 0.65 to 1) at places
  from a fixed seed, wrapping at the edges (`UgcGlitter::FleckAlpha`), each mipmap the 2x2 mean of the one above.
- Two `NiTextureTransformController`s on the property (the property's controller, the first linking the second):
  flags 0x48 (active, loop, app time), frequency 1, phase 0, start 0, stop the period, target the property,
  base map, operation translate U and translate V, each with an `NiFloatInterpolator` and `NiFloatData` of two linear
  keys (0, 0) and (period, 1): a tile in `7 / glitter_speed` s in U and `11 / glitter_speed` s in V, looping, and
  wrapping makes the loop seamless. The block layouts are the ocean file's (its controllers are 39 bytes, the property
  70). With `glitter_speed` 0 the property has no controllers.

The client finds the animation: `SetupRenderNodeExtraData` (0x00c746c0) sets `RenderNodeExtraData.flags0` bit 2 from
`NifHasAnimatedControllers` (0x00bf4160), which returns true for a shape whose `NiTexturingProperty`'s first
controller is an `NiTextureTransformController`. No node transform controllers are added (they would clear the
object's static flag).

Transparent glitter: every UGC shape has the same `NiAlphaProperty` (blend source alpha over one minus source alpha)
and transparent bricks are transparent by their vertex alpha; the LEGO-AnimUV techniques declare
`UsesNiRenderState = true` and their pixel shader outputs the vertex alpha, the same as the LEGO shader's that
`S01_Alpha_Model` is drawn with, so transparent glitter gets a group of its own. There is no shimmer:
LEGO-AnimUV's pixel shaders don't read the material's emissive (only the `_Emissive` ones do), so an
`NiMaterialColorController` would change nothing.

The icon draws the flecks where they are at the start (the same texture and UVs, before the light; `glitter_size`
and `glitter_density`), opaque and transparent. The UGC page's 3D view marks glitter meshes (`/api/ugc/mesh`: look
`GLITTER` 512, a mesh with a stored texture in a group tagged `shader_glitter` or 21) and draws moving flecks from
their UVs and `uvScroll` (what `NifFile` reads from the controllers); the property and zone views, which draw bricks
from the LXFML, draw them on the colors in `window.LDD_GLITTER` (`/api/bricks/materials.js`: the glitter colors by
the current settings) from their positions.

#### Satin

The client has no satin shader either: Clear Plastic (mapShaders 3) has no vertex color, so it can't show a colored
satin. Satin bricks stay in `S01_Alpha_Model` and are made to look satin when their colors are made: a transparent
brick of a `satin_colors` color gets `satin_opacity` as its vertex alpha, and its color (any brick's) is moved
`satin_whiten` percent towards white, milky. These colors are only in a client whose Materials.xml has them (the
opal colors, 360 to 376, are not in the 1.10.64 client's).

Modular builds (`ugc_modular_build` rows, `ldf_config` like `1:4713+1:4714+1:4715`):

1. Each module LOT's `ModuleComponent` (component type 28) gives its part code and build
   type; the build type's `ModularBuildComponent.xml` gives the topology (root part, and which part connects to which
   named location) and `Placement/AdditionalModelRotation`.
2. Each module's mesh is its render asset (`RenderComponent.render_asset`, the NIF the client assembles in game). A
   connection places the connecting part so its node with the location's name (when it has one) sits on the parent
   part's node of that name, or its origin on that node; `ModuleComponent.xml`'s `connection` translation is used when
   the parent's NIF has no such node. (Module LXFMLs in `res/BrickModels` exist for only some modules and are
   authored in different spaces, so they aren't used.)
3. The assembled mesh gets an icon like a model's (with the build type's preset and the combination's own values). No
   mesh is stored: the client assembles modular builds itself. For the dashboard's icon editor the assembled mesh is
   made on request (`/admin/assembly`, below).

Cars and rockets are put together from a fixed set of modules, so the icon is made once per combination of modules
and shared by every build of it. A build's combination is its `ldf_config`'s LOTs sorted (`UgcModularKey::Normalize`,
e.g. `4713-4714-4715`: each LOT belongs to one slot of one build type), stored under an id hashed from it. A build whose
combination is made already is marked made right away; builds of a combination being made wait for it and all get its
outcome. The client's downloads stay per blueprint id: the server looks up the build's combination and serves the
shared files. `combo.json` in the combination's folder says its LOTs and build type.

## Storage

Files live under `ugc_output_dir` (default `ugc` next to the server binaries):

```
ugc/models/<id % 1000>/<id>/model.nif.gz, model.nif.checksum, model.nif.sd0, icon.dds.gz, icon.dds.checksum,
                              icon.dds.sd0, icon.png, model.noao.nif.gz, stats.json, previous.icon.png,
                              previous.model.nif.gz, previous.stats.json
ugc/modular/<combination id % 1000>/<combination id>/icon.dds.gz, icon.dds.checksum, icon.dds.sd0, icon.png, combo.json
ugc/.checksums-stored
```

Every file the client downloads is written three ways: `.gz` and `.checksum` for 3D services and `.sd0` (dCommon's
`Sd0::Compress`) without them. When a worker writes an item, the checksums of its `.sd0` files (MD5 and size of the
inflated file, from the `.checksum`) go back to the main thread, which stores them in `ugc_file_checksums` for the
worlds. Items made before this get their `icon.dds.sd0` (from `icon.dds.gz`) and their icon's checksum once, a few per
tick on the main thread when the server starts; `.checksums-stored` marks that done. Their `.nif` gets its `.sd0` and
checksum when the model is made again; until then the worlds send such a model's LXFML (no `model.nif` checksum, see
"Models without 3D services").

A model's files are written to a temporary folder and renamed into place, so a half written model is never served.
Meshes are only stored compressed (a model took about 18 MB when the .nif was kept uncompressed beside its .gz, so the
2 GB cap held about 110 models and the server kept evicting and remaking them); the dashboard's copies are inflated
when it asks. When an item is made again, its icon, mesh and stats of the version before are kept as `previous.*`.
`ugc_max_storage_mb` (default 2048, 0 for no limit) caps the folder: when it is over, the models whose files were used
longest ago are deleted. Their rows stay `is_optimized = 1`; when something asks for their files the server sets
them back to 0 and makes them again (the request answers 408 meanwhile), so deleted files are only made again when
wanted. A combination's files are deleted like any others even while builds share them: the next request for any of
those builds makes them again.

### Deleting and purging

The dashboard can delete stored files (the UGC server does it on its main thread and says how many bytes it freed):
one item's, every item matching a filter (kind, state, owner, made more than N days ago, not asked for in N days) or
all of a kind (typing `PURGE ALL`). Afterwards the rows stay made (made again when a game client asks), are queued to
be made now, or are marked failed with "Deleted from the dashboard". Items being made at that moment are skipped, so a
worker never writes into a folder being deleted.

## Database state

Migrations `dlu/mysql/81_ugc_processing.sql` and `dlu/sqlite/64_ugc_processing.sql` (the MySQL one only adds columns
that aren't there yet).

`ugc`: `is_optimized` (existing) is 0 until the model has been processed, 1 once its files are made, 2 when processing
failed, 3 when there was nothing to make (the model has no bricks; not a failure, never retried, counted and shown as
"empty", answered with 404 like HKX so the client doesn't wait; migrations 87/70 move the rows that had failed only for
that). The names come from `IUgc::eProcessState` (`IUgc::ProcessStateName`). New: `processed_at` (Unix seconds of the last attempt), `process_attempts`, `process_error` (the last failure's
reason, empty when none). `bake_ao` (existing) records whether lighting was baked into the model. Changing a model's
LXFML (`UpdateUgcModelData`) sets it back to unprocessed.

`ugc_modular_build`: new `is_optimized`, `processed_at`, `process_attempts`, `process_error`, meaning the same.

Migrations `dlu/mysql/88_ugc_model_stats.sql` and `dlu/sqlite/71_ugc_model_stats.sql`: `ugc.brick_count` and
`ugc.triangle_count`, what the UGC server counted when it made a model (its bricks, and the triangles of the made
mesh's most detailed level, from `stats.json`; 0 until it has), so the dashboard can sort models by size.

Migrations `dlu/mysql/86_ugc_debounce_icon_settings.sql` and `dlu/sqlite/69_ugc_debounce_icon_settings.sql`:
`ugc.process_after` and the table `ugc_icon_settings` (`target`: `kind:<kind>`, `model:<id>` or `combo:<key>`; `params`
JSON; `updated_at`).

Migrations `dlu/mysql/89_ugc_file_checksums.sql` and `dlu/sqlite/72_ugc_file_checksums.sql`: the table
`ugc_file_checksums` (`kind` 0 a player model, 1 a combination of car or rocket modules; `storage_id` the model's ugc
id or the combination's id; `file` `icon.dds` or `model.nif`; `md5`, 32 lowercase hex digits, and `size` of the inflated
file), written by the UGC server whenever it writes a file, and `ugc_modular_build.combination_id`, the combination a
build shares its files with (0 until the UGC server has seen the build: it fills them in for existing builds when it
starts, -1 when the modules can't be told). `IUgc::GetUgcFileChecksum(blueprint, file)` looks a blueprint up as a model
first, then as a build through its combination.

## Without 3D services (`UGCUSE3DSERVICES=7:0`)

`UgcManifest` (dGame/dUtilities) answers `REQUEST_UGC_MANIFEST_INFO` when `ugc_manifest=1` (`sharedconfig.ini`,
default 0):

* The world's main thread handles the packet (`g_WorldHandlers`), looks the checksum up with one indexed query and sends
  `UGC_MANIFEST_RESPONSE` to that client in the same tick.
* Icons (DDS): an icon that isn't made yet isn't answered: the request waits (at most 512, for 15 minutes) and
  `UgcManifest::Update` (world tick) looks again every 5 seconds, answering once the UGC server has stored the checksum.
  A model still in its quiet period after a save is made right away (`ExpediteUgcModel`), as when a client asks the UGC
  server directly. Waiting requests are dropped when the client disconnects.
* Player models' files (NIF, HKX, LXFML) are always answered at once, see "Models without 3D services".
* No worker threads, HTTP or file reads in the world: the UGC server precomputes the checksums into the database (the
  LXFML's is worked out from the `ugc` row on the main thread and kept).

The UGC server serves `BrickModels/UserMade/<bucket>/<id>.<ext>.sd0` under `client_path`, under
`/<any folder>/UserBrickModels` (the client's default `UGCSERVERDIR`, `lwoclient/UserBrickModels` with its built-in
patch folder) and at the root; `.dds` is a model's icon or a car or rocket build's combination icon, `.nif` a model's
mesh, `.lxfml` the model's LXFML from the database, `.hkx` 404.

It is off by default because a client whose `boot.cfg` doesn't point at the UGC server downloads from its defaults (the
patch server's address, `http://127.0.0.1:80/lwoclient` when that isn't set either) and is logged out when it can't
connect there. Turn it on when the players' `boot.cfg` has `UGCSERVERIP`, `UGCSERVERPORT` and `UGCSERVERDIR` for the
UGC server (see the settings above), or when the patch server's address is the UGC server.

### Models without 3D services (`ugc_manifest_models`)

What the 1.10.64 client does with a placed player model (LOT 14, spawned with `blueprintid`), checked in the client:

* The model's BlueprintComponent (component 42) sets `renderUserGen=1` and `physicsUserGen=1` in the spawn data itself
  when it has a `blueprintid` and no `nif_name` / `hkx_name` (`LWOBlueprintComponent::PrepareConfigData`, 0x00c736f0),
  so the render component always loads the blueprint's NIF (resource type 1) and the physics its HKX (type 2) by
  blueprint id (`ObjectLoader2::LoadRenderComponent`, 0x010536b0; `LWOBasePhysComponent::LoadHkxDataFromConfig`,
  0x00c75220). There's no choice between LXFML and NIF on the render side, and the world needn't send `renderUserGen`.
  The ModelBehaviorComponent loads the blueprint's LXFML (type 0) as well (`RequestBlueprintData`, 0x00c24640).
* Every such request first needs the blueprint's manifest info (`UGCManifest_Base::GetOrRequestManifestInfo`,
  0x0101e0e0): a cached entry (kept 300 seconds; stored in `res/BrickModels/UserMade/manifest.cache`) is used at once,
  else the request waits and the client sends `REQUEST_UGC_MANIFEST_INFO`. **There is no timeout**: a manifest request
  the world never answers leaves that file, and the model, waiting for good
  (`LWOResMgr2Interface::RequestBlueprintManifestThenLoad`, 0x0105a910).
* With the answer, the client uses the file it has when its MD5 matches (or the answer's valid is 0 and it has one),
  else downloads it (`LoadBlueprintResource`, 0x01056700). A failed download (404) leaves the model without that file
  (no fallback to the LXFML) and is reported as `UgcDownloadFailed` (world message 120, with the status); status 0 logs
  the player out.
* The LXFML the world sends when a property loads (`BlueprintSaveResponse` with local id 0) makes the client build the
  NIF and HKX itself and cache the manifest info of the LXFML, NIF and HKX with its own files' MD5s
  (`LWOBBBInterface::MainThread_ProcessModelResponse`, 0x00b5a1e0), which answers its own requests, so nothing is
  downloaded. That's how DLU always showed models.
* `NotifyClientUGCModelReady` (game message 909, the blueprint id only) to a model: its BlueprintComponent flushes the
  cached NIF, HKX and LXFML of that blueprint and requests the NIF and HKX again
  (`LWOBlueprintComponent::OnNotifyClientUGCModelReady`, 0x00ca6430). It doesn't clear the manifest cache, so the new
  checksum has to reach the client first: a `UGC_MANIFEST_RESPONSE` the client didn't ask for updates its cache
  (`PacketHandler_MSG_CLIENT_UGC_MANIFEST_RESPONSE` caches whatever arrives).
* Live packet captures of a property load weren't available to compare with.

With `ugc_manifest=1` and `ugc_manifest_models=1` (`sharedconfig.ini`, default 0; the dashboard shows it under UGC
serving), the worlds:

* **Property load**: send the LXFML (one `BlueprintSaveResponse`) only for the models whose mesh the UGC server hasn't
  made (no `model.nif` in `ugc_file_checksums`). Made ones are left out: the client asks for their files, downloads
  the served mesh and draws it. Tried and dropped: sending every model's LXFML (the client builds its own NIF and HKX)
  and then switching to the served mesh with the served NIF's checksum, `NotifyClientUGCModelReady` and the model
  constructed again: the client kept drawing its own build.
* **NIF** of a made model: the UGC server's checksum; the client downloads `<id>.nif.sd0` from the UGC server.
* **LXFML** of a made model: the MD5 and size of the stored LXFML inflated (what the UGC server serves as
  `<id>.lxfml.sd0`), worked out once per model and kept.
* **HKX** of any model: the model's LXFML (the UGC server makes no physics). The client builds the model from it and
  loads its own HKX, so served models have collision; the mesh already drawn stays the served one.
* **Any model file of a model that isn't made** (e.g. a model someone else just placed, or `ugc_manifest_models=0`):
  the model's LXFML to that client (once per 10 seconds for the three requests), which builds it itself, so a model is
  never left waiting. A blueprint that isn't a player model gets valid 0.
* **Made again**: when the UGC server writes a model's mesh with a different checksum than before (made for the first
  time, or remade after a change), it sends `UGC_MODELS_MADE` (master message 37, the blueprint ids) to the master,
  which passes it to every world. A world with that model placed sends every player the new NIF checksum, then
  `NotifyClientUGCModelReady` and the model constructed again to each player it's shown to, so the served mesh
  replaces what the client showed.
  A model made again unchanged (after eviction) isn't sent. Nothing polls: one message per batch of made models.

* **`/reprocessproperty`** (GM 8): every model placed on the property the player is on goes back to the UGC server's
  queue (`ResetPropertyUgcModelProcessing`). The world checks every 5 seconds; once none is pending (or after 15
  minutes) it sends every player in the world the new `model.nif` checksums and transfers them back into the same
  zone and clone. The client loads the property again and downloads the new meshes (its manifest cache has the new
  checksums, which its files don't match). Models of a reprocess skip the "made again" switch.
  The models are queued as priority (`ugc.priority`, also set by the dashboard's Reprocess all models): the UGC
  server takes them before any other model, polls them even when its queue is full and puts them at its front (as
  cars and rockets); the flag clears once a model is made.

### Waiting while the owner is still building

A saved model isn't made right away: `InsertNewUgcModel` stores `process_after` = now + `ugc_debounce_seconds`
(`sharedconfig.ini`, default 120, 0: right away) and moves the owner's other waiting models to that time too, so each
save starts the wait again. The UGC server only takes models whose `process_after` has passed. The wait ends early when
a game client asks for the model's files, when the owner leaves the world or logs out, or when it is made again from
the dashboard. Every save is a new blueprint: versions deleted during the wait are never made. It survives restarts.

The database is the queue: the UGC server looks for rows with `is_optimized = 0` every `poll_interval_ms` (default
2000), newest first, so worlds need no change to have new models processed. Rows that failed are tried again up to
`max_attempts` (default 3) times. Reprocessing (dashboard) sets rows back to `is_optimized = 0, process_attempts = 0`.
No master messages are needed for any of it.

### Marking models for processing (for code that writes `ugc` rows)

* A new row needs nothing: `is_optimized` defaults to 0 (`InsertNewUgcModel` writes 0, and its `process_after`).
* When a model's LXFML changes, `UpdateUgcModelData` sets `is_optimized = 0, process_attempts = 0, process_error = ''`
  in the same statement. Anything that writes `ugc.lxfml` another way must do the same, or call
  `ResetUgcModelProcessing(id, false)`.
* Deleting the row is enough when a model is deleted; its files are left until the storage cap removes them.
* The same holds for `ugc_modular_build` (`InsertUgcBuild` rows start at 0; `ResetModularBuildProcessing`).

## Threads

The main thread owns RakNet (the master link), mongoose (HTTP) and the database. `worker_threads` (default: half the
CPUs) workers do only pure work: parse LXFML, build and write meshes and icons, compress files. They get their input
(LXFML text, module data) from the main thread and hand results back through a queue the main thread drains, which
then updates the database (including the files' checksums, parsed by the worker from the `.checksum` it wrote).
Brick geometry and materials are loaded once and shared read-only (the cache has its own lock). The start-up backfill
of old items' `.sd0` icons and checksums and of builds' combination ids runs on the main thread, a few items per tick.

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
  the client's `UGCSERVERDIR`), and the same under `/lwoclient/UserBrickModels` (the path the client really uses, see
  above). HKX answers 404. The LXFML comes from the database. A model that exists but isn't made yet (or was evicted) is moved to
  the front of the queue and answers 408 so the client asks again.
* `<client_path>/BrickModels/UserMade/<id % 1000>/<id, 20 digits>.<lxfml|nif|hkx|dds>.sd0`, the same under
  `/<folder>/UserBrickModels` and at the root, for clients without 3D services (see "Without 3D services"); 408 and 404
  as above.
* `/files/model/<id>/<file>` and `/files/modular/<id>/<file>` for the dashboard (`icon.png`, `model.nif` and
  `model.noao.nif` (inflated), `stats.json`, `combo.json` and `previous.` versions), not cached by browsers.
* `/admin/preview`, `/admin/assembly`, `/admin/regenerate-icons`, `/admin/delete` (POST, JSON) for the dashboard only:
  they need the header `X-Ugc-Admin-Key` with the master password. A preview draws an icon with given values (the
  whole pose included) on a worker (ahead of the queue, within the CPU and memory budgets) and returns the PNG without
  storing it. `assembly` (`{modules}`) returns a combination's assembled mesh as a .nif, put together exactly as for
  its icon and already turned by the build type's `AdditionalModelRotation`, made on a worker the same way and kept in
  a small cache (the last 32 combinations, at most 64 MB), so the editor can show what the icon renderer draws.
  regenerate-icons draws every stored icon of a kind again (player models' from their stored .nif, nothing else is
  made); delete is described under Storage.
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
things again and to save icon values) reads the database: counts per state for models and for cars and rockets, and the
items as a gallery of their icons or a list. Player models are listed one by one (owner, state, attempts, last attempt,
bricks and triangles, file name, failure reason). Cars and rockets are listed as **assemblies**, one per combination of
modules however many builds use it (the UGC server makes one icon per combination): its icon, build type (named after
the type's assembly object in `ModularBuildComponent`), its modules with their names and icons from the CDClient, its
state (made when any build of it is) and how many builds and owners use it.

Both lists are paged on the server (`GET /api/ugc?kind=model|modular&q=&state=&type=&sort=&page=&size=`, with the
total), with numbered pages, first and last, a page to jump to and a page size kept per user. The search box takes
plain text (names, owners, ids) or field prefixes: `owner:`, `account:`, `property:`, `name:`, `lot:` or `module:` (a
LOT, or for assemblies a module's name), `id:`, `state:` and `kind:` (a car or rocket type). Models sort by newest,
oldest, most bricks, most triangles, owner or file name; assemblies by newest, oldest, most builds or name. The kind,
search, filters, sort, page and view are kept in the address, so Back and Forward and shared links work. The search
is the UGC search's SQL (`UgcLookupSql`, the same on MySQL and SQLite, `IUgcLookup::ListUgc`); assemblies are grouped
from the builds (`UgcAssemblies`). Buttons make one item, the failed ones or everything again (these only reset the
columns; the UGC server picks the rows up).

An assembly opens with its modules, the icon editor and **References**: the builds that use it (`GET
/api/ugc/assembly/builds?modules=&q=&page=`), with owner character and account, state and where each is (placed on a
property, in a mail, in its creator's inventories; the same lookup as the UGC search), paged and searchable. A link
to a build, `/ugc?item=<build id>&kind=modular` (what the UGC search and the character pages link to), opens its
assembly (`GET /api/ugc/assembly/of/<id>`) with that build highlighted in References.

**The icon editor** (on every opened item, and per type under **Icon presets per type**, which opens each type on an
example: the newest made player model, the most used combination of each car or rocket type) is one panel for
everything an icon's look can be set to, all from the parameter list (`GET /api/ugc/icon/params`, with each
parameter's group):

* A 3D view (three.js) of the player model's made .nif (`/api/ugc/mesh/<id>?lod=0`, what its icon is drawn from) or of
  the assembly (`GET /api/ugc/assembly?modules=`, the UGC server's assembled .nif converted by the dashboard), seen
  through the icon renderer's own camera: `static/js/ugc-pose-math.js` is `UgcIconPose` in JavaScript (the same
  perspective, turn order, framing and crop; both are checked against one fixture, `tests/dUgcTests/
  ugc-pose-fixture.json`, by gtest and by node). The view shows a little more than the icon, with the icon's square
  outlined, so it matches the preview beside it. Dragging moves the camera around the model, turns the model (Ctrl+
  drag, or the Turn model mode; Alt+wheel rolls it), moves the sun (Alt+drag, shown as an arrow) or shifts the model in
  the icon (Shift+drag or right drag); the wheel changes the border. The light in the view is close to the icon's
  (world light, sun, fill, exposure, contrast) but has no shadows or highlights: the preview is the real thing.
* Sliders for every parameter, grouped (camera, border and shift, model turn, sun, light, look), kept in step with
  the view both ways.
* A live preview drawn by the UGC server (`POST /api/ugc/icon/preview`, sent a moment after the last change).
* Save as the preset for the item's type, save for this model or combination of modules, go back to the type's
  preset or the default settings, remove the item's own values, and draw every icon of the type again. Saving needs
  `ugc_manage` and is written to the audit log.

**Settings** for the UGC server have their own category on the Settings page (serving, processing, models, storage,
icons) and the UGC page shows each section beside what it changes (processing, models and serving under the server
status, storage with the purge tools, icon defaults with the presets), for those with the `settings` permission.
Both read the settings catalog and save the same way (the servers reload at once; settings marked restart are read
when the UGC server starts), and each links to its entry on the Settings page (`/settings#ugcconfig.ini/<name>`).

Everything from the UGC server goes through the dashboard, which reaches it at `ugc_internal_url`
(`dashboardconfig.ini`; empty: `http://127.0.0.1:2008`, the same machine), so the page works from wherever the browser
is: `/api/ugc/server/status` (its `/status`, kept 2 seconds), `/api/ugc/files/<kind>/<id>/<file>` (kept 15 seconds)
`/api/ugc/mesh/<id>?lod=&version=current|previous&ao=0|1` and `/api/ugc/assembly?modules=` (its NIFs converted by the
dashboard's worker threads with the scenery viewer's NifFile conversion). The fetches run on the dashboard's worker threads. `ugc_public_url` is
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

* Served models (`ugc_manifest_models`) are not checked in game yet: that the served mesh shows, that
  `NotifyClientUGCModelReady` swaps a model a client built itself for the served one while it's shown, and how models
  without collision behave.
* HKX (physics) is not generated, so models downloaded from the UGC server have no collision for clients that never
  built them.
* Cars and rockets built before builds were stored (subkey 0, no `ugc_modular_build` row) never get an icon: the client
  has no blueprint id to ask for.
* A model's `.nif` made before `.sd0` files were written has none (and no checksum, so clients keep building that
  model from its LXFML) until the model is made again (Reprocess on the dashboard).
