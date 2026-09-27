# The client's brick database (res/brickdb.zip)

LEGO Universe uses LEGO Digital Designer's brick library. The client (1.10.64) keeps it in two places:

| Where | What |
| --- | --- |
| `res/brickdb.zip` | `info.xml` (`<Bricks version="457"/>`), `Materials.xml`, `Primitives/<designID>.xml` (1879), `Assemblies/<designID>.lxfml` (35) |
| `res/brickprimitives/lod0..lod2/<designID>.g`, `.g1`, `.g2`... | the geometry, loose files, **not** in the zip |

Adding a brick is therefore: a primitive XML inside the zip, its geometry in all three `brickprimitives/lod*` folders,
and a `BrickIDTable` row (LOT -> design ID) in the CDClient database so the game has an object for it.

## Short version

The client is not picky about the zip. It does not hash, sign or cache it, and a zip rebuilt by Python's `zipfile`,
7-Zip or libarchive, stored or deflated, in any order, with or without directory entries, extra fields or an archive
comment, with extra files, loads fine. Every edit that broke it in testing came down to one of these:

1. **The files are not at the top of the zip.** A wrapping folder (`brickdb/Primitives/...`, what you get from
   zipping the extracted folder rather than its contents) or a `./` prefix (`bsdtar -cf x.zip .`). The client then
   has no `Primitives` directory and drops the **whole** database.
2. **Backslashes in entry names** (`Primitives\3001.xml`). The client splits paths on `/` only, so it again finds no
   `Primitives` directory. Some Windows zip writers do this.
3. **ZIP64 end records**: the end-of-central-directory record must directly follow the central directory.
4. **Data descriptors with the sizes also in the local header**: when general-purpose flag bit 3 is set, the local
   header's CRC and sizes must be zero.

(1) to (4) all lose the whole database with no error saying why. The client logs one line per LOT,
`BrickIDTable: LOT n refers to invalid design ID m` (1909 of them), gets very slow, and in two of the tests died soon
after. A primitive that is missing, misnamed or has bad XML only loses that one brick.

`tools/brickdb/repack.py` writes the shipped layout and checks a zip against every rule below:

```sh
# rebuild (with no overlay the shipped file comes back byte for byte)
python3 tools/brickdb/repack.py build <client>/res/brickdb.zip new-brickdb.zip --overlay my-bricks/
# my-bricks/Primitives/99001.xml etc. add or replace entries; the source may also be an extracted folder

# what would the client reject? --res also looks for each primitive's geometry
python3 tools/brickdb/repack.py check new-brickdb.zip --res <client>/res
```

## How the client loads it

Addresses are in the 1.10.64 client; all are named and bookmarked (category `BrickDB`) in the Ghidra project.

1. `LWOBBBInterface::Startup` (00b6ae00) reads `BrickIDTable` from the CDClient database, then checks every LOT's
   design ID with `LWOBBBInterface::ResolveDesignID` (00b682e0). The first call runs `BrickKitHelper::InitBrickKit`
   (00b63a40).
2. `InitBrickKit` asks the resource manager for `brickdb.zip` (`ResMgr2GetResourceImmediate`). The file is read
   **whole into memory** and there's no hash check. `Failed to load brick DB: %S` is logged **only when the file is
   missing**. Any other failure leaves `m_pBrickKit` null without a message, and because `ResolveDesignID` calls
   `InitBrickKit` again each time, the client rereads and reparses the zip once per LOT. That is why a broken zip makes
   startup crawl.
3. `LEGO::BrickKit::CreateFromMemory` (00908840) registers LDD's storage classes (`LiffStorageDirectory`,
   `ZipStorageDirectory`, `MemoryDirectory`). `LEGO::BrickKitImplementation::InitFromZipMemory` (00901550) requires
   more than 31 bytes and mounts the buffer with `LEGO::MountZipFromMemory` (008e68b0).
4. `LEGO::BrickDatabase::BrickDatabase` (0095ff70) reads `info.xml` (`DB/Bricks@version`, which doesn't affect loading).
   `LEGO::BrickDatabase::Load` (0095ee50, called with *skip decorations* and *load assemblies*):
   - opens the `Primitives` directory, or fails the whole database with `Could not open Primitives directory` (to
     LDD's internal log, which LU never shows);
   - loads every `*.xml` in it. **The design ID is the file name** parsed as a number, not anything inside the XML.
     If a file won't open or parse, only that primitive is skipped;
   - then loads `Assemblies`. `Decorations` and `DecorationMapping.xml` aren't loaded in LU.
5. Geometry is read later, when a brick is drawn, from `BrickPrimitives/` (the loose `res/brickprimitives` folders).

## The zip reader (LDD's `LEGO::ZipStorage*`, not zlib's minizip)

`LEGO::ZipStorageFactory::ParseArchiveIndex` (0094bbd0):

- `LEGO::Zip::FindEndOfCentralDirectory` (00947910) scans backwards through the last 64 KiB for `PK\5\6`, so an
  archive comment is fine.
- Rejects the archive unless both disk numbers are 0 and *entries on this disk* == *total entries*.
- Requires **end-of-central-directory offset == central directory offset + size**, and that the difference, taken as
  the offset of the zip inside the buffer, is 0. So no ZIP64 end record or locator in between, and no data (like a
  self-extractor stub) before the zip. ZIP64 isn't supported at all: 32-bit offsets and sizes, at most 65535 entries.
- Walks the central directory (46-byte `PK\1\2` headers) until the signature stops matching. Each name is split on
  **`/` only** into a tree whose lookups are **case-insensitive** (`BasicMap<String, ZipTreeNode, StringCaseInsensitiveLess>`).
  A trailing `/` makes a directory entry, which is optional because folders are made implicitly. An empty component
  in the middle of a name (`a//b`, a leading `/`) fails the whole archive. `.` isn't special, so `./Primitives` is a
  folder named `.`.
- Entry order doesn't matter.

`LEGO::ZipStorageDirectory::OpenEntryValidateLocalHeader` (00949750) runs when an entry is opened:

- the local header must start with `PK\3\4`;
- local flags == central flags, local method == central method, and the entry must not be encrypted (bit 0);
- without a data descriptor (bit 3 clear), the local CRC, compressed size and size must equal the central ones.
  **With bit 3 set they must all be zero**;
- the method must be 0 (stored) or 8 (deflate). Deflate64, bzip2, LZMA and the rest are rejected;
- the data starts at local offset + 30 + *local* name length + *local* extra length, so local and central extra
  fields may differ.

`LEGO::ZipStorageFile::ReadAndInflate` (009490c0) copies stored data, or inflates with raw deflate (zlib 1.2.2,
`windowBits -15`) and needs `Z_STREAM_END` in one `inflate(Z_FINISH)` call. **It doesn't check the CRC.** File-name
encoding: names are ASCII in practice, and the UTF-8 flag isn't looked at.

The reader can also write (`LEGO::ZipStorageFile::FlushToArchive` 00948760, `LEGO::ZipStorageFactory::DeleteEntry`
0094a5d0), but LU only mounts the zip from a memory buffer and never writes it back.

## No verification or cache

- **No hash or signature.** Nothing compares brickdb.zip against a hash. `LWOResMgr2Interface::CompareFileMd5WithManifest`
  (0104ede0) compares a file's MD5 with the catalog or `versions/quickcheck.txt` (`LwoQuickcheckFile::LookupMd5`
  011021b0). It's only used by the runtime downloader (`DownloadResourceHttp`, `LoadBlueprintResource`), and only when
  a `versions/` folder exists. The unpacked client has none.
- **Patcher installs:** in an install made by the original patcher, `versions/trunk.txt` and `versions/frontend.txt`
  list `client/res/brickdb.zip` with its size and MD5 (`1808539,671f7fb9...` in 1.4.49), and `versions/quickcheck.txt`
  caches `path,mtime,size,md5`. The **patcher/launcher**, not the game, will see a changed brickdb.zip as damaged
  and download the original again. Start `legouniverse.exe` directly, or update those manifest lines, if you use one.
- **No cache.** Nothing is built from the brick library on disk. The prefix's `AppData` has only `lwo.xml`, the
  logs and per-account settings, and `Documents/LEGO Creations` holds only screenshots.

## Tests

Run in the unpacked 1.10.64 client with its usual mods loaded. The brickdb.zip in a copy of the client folder was
swapped for each variant. "Invalid" counts the `refers to invalid design ID` log lines after about 30 seconds. For the
new-brick tests, the copy's `cdclient.fdb` had LOT 3's `LEGOBrickID` changed from 3701 to 99001, so a primitive 99001
that loads brings the count from 1 to 0.

| Variant | Result |
| --- | --- |
| Python `zipfile` repack of the original | byte-identical to the shipped file |
| Python, directory walk order (`Materials.xml` before `info.xml`), with or without directory entries | loads |
| 7-Zip (`7z a -tzip`, directory entries, extra fields, version 6.3) | loads |
| libarchive `bsdtar --format zip` naming the top-level entries (data descriptors with zero local sizes, UT/ux extras) | loads |
| every entry stored (7.8 MB) | loads |
| an extra file (`readme.txt`), an archive comment | loads |
| new primitive `Primitives/99001.xml` (3701 with `aliases="99001"`) plus geometry `lod0..2/99001.g`, appended at the end | loads, 99001 resolves |
| an extra primitive with truncated XML, one with a UTF-8 BOM, one named `3701 - Copy.xml` | loads (only that file is affected) |
| `repack.py build` with an overlay (new 99001 + edited `3001.xml`) | loads, 99001 resolves |
| `bsdtar -cf x.zip .` (`./` prefix) | **whole database lost** |
| every name prefixed with `./` | **whole database lost** |
| backslash separators | **whole database lost** |
| ZIP64 end record and locator inserted before the end record | **whole database lost**, client died |
| bit 3 set with CRC and sizes also in the local headers | **whole database lost** |
| file truncated to 1 MB | **whole database lost**, client died |

## Adding a brick

1. Put the primitive at `Primitives/<designID>.xml`. The file name is the ID. Keep `aliases` from overlapping another
   primitive's IDs (`repack.py check` reports overlaps).
2. Put the geometry at `res/brickprimitives/lod0/<designID>.g` (and `.g1`, `.g2`... for more parts), and the same in
   `lod1` and `lod2`.
3. Rebuild the zip: `repack.py build <client>/res/brickdb.zip out.zip --overlay my-bricks/`, then
   `repack.py check out.zip --res <client>/res`.
4. Give the brick an LOT: a `BrickIDTable` row (`NDObjectID` = LOT, `LEGOBrickID` = design ID) plus its `Objects`
   and item rows, in the client's CDClient and the server's database. The client warns about any `BrickIDTable` row
   whose design ID isn't in the zip.

Not yet tested: placing such a brick in Brick-by-Brick. The test above only proves the client's brick library
resolves the new design ID.
