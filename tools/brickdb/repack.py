#!/usr/bin/env python3
"""Rebuild or check the client's res/brickdb.zip in the layout the LEGO Universe client (1.10.64) accepts.

The client mounts brickdb.zip with LEGO Digital Designer's own zip reader (see docs/BrickDb.md). It is lenient
about compression and entry order, but a few things other zip tools commonly do make it drop the whole brick
database: a wrapping folder or "./" prefix, backslash separators, ZIP64 end records, and data descriptors whose
local header still carries the sizes. This script always writes the same layout the shipped file uses.

  repack.py build SRC OUT.zip [--overlay DIR]...   SRC is an extracted brickdb folder or a brickdb.zip; each
                                                   --overlay folder (same layout) adds or replaces entries
  repack.py check FILE.zip [--res RES_DIR]         report everything the client would reject; with --res also
                                                   look for each primitive's geometry in brickprimitives/

Repacking the shipped brickdb.zip with no overlay gives back the identical file.
"""

import argparse
import os
import struct
import sys
import time
import zipfile
import zlib
import xml.etree.ElementTree as ElementTree

SIG_LOCAL = 0x04034B50
SIG_CENTRAL = 0x02014B50
SIG_END = 0x06054B50
SIG_END64 = 0x06064B50
SIG_END64_LOCATOR = 0x07064B50

# Where each file lives in the database and what the client does with it
ROOT_FILES = ("info.xml", "Materials.xml")
NUMBERED_DIRS = {"Primitives": ".xml", "Assemblies": ".lxfml"}


def entry_order(name):
	"""The shipped order: info.xml, Materials.xml, then primitives and assemblies together by design ID, then the rest."""
	if name in ROOT_FILES:
		return (0, ROOT_FILES.index(name), 0, name)
	folder, _, file = name.partition("/")
	stem, ext = os.path.splitext(file)
	if folder in NUMBERED_DIRS and ext.lower() == NUMBERED_DIRS[folder] and stem.isdigit():
		return (1, int(stem), 0 if folder == "Primitives" else 1, name)
	return (2, 0, 0, name.lower())


def read_source(src):
	"""{name: (bytes, date_time)} from an extracted folder or a zip."""
	entries = {}
	if os.path.isdir(src):
		for root, dirs, files in os.walk(src):
			dirs.sort()
			for file in files:
				path = os.path.join(root, file)
				name = os.path.relpath(path, src).replace(os.sep, "/")
				with open(path, "rb") as f:
					entries[name] = (f.read(), time.localtime(os.path.getmtime(path))[:6])
	else:
		with zipfile.ZipFile(src) as z:
			for info in z.infolist():
				if info.is_dir():
					continue
				entries[info.filename.replace("\\", "/")] = (z.read(info), info.date_time)
	# drop a "./" prefix or a wrapping folder if the source has one
	entries = {(n[2:] if n.startswith("./") else n): v for n, v in entries.items()}
	tops = {n.split("/", 1)[0] for n in entries}
	if "info.xml" not in entries and len(tops) == 1 and tops.pop() + "/info.xml" in entries:
		top = next(iter(entries)).split("/", 1)[0] + "/"
		entries = {n[len(top):]: v for n, v in entries.items()}
	return entries


def build(src, out, overlays):
	entries = read_source(src)
	for overlay in overlays:
		for name, value in read_source(overlay).items():
			if name.startswith("brickprimitives/"):
				continue
			entries[name] = value
	missing = [n for n in ("info.xml",) if n not in entries]
	if missing or not any(n.startswith("Primitives/") for n in entries):
		sys.exit(f"{src}: no info.xml or Primitives/ at the top of the database")
	tmp = out + ".tmp"
	# Plain deflate at zlib's default level, no directory entries, no extra fields, no data descriptors, no ZIP64
	with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, allowZip64=False) as z:
		for name in sorted(entries, key=entry_order):
			data, date_time = entries[name]
			info = zipfile.ZipInfo(name, date_time=max(tuple(date_time), (1980, 1, 1, 0, 0, 0)))
			info.compress_type = zipfile.ZIP_DEFLATED
			info.create_system = 3
			info.external_attr = 0o600 << 16
			z.writestr(info, data)
	os.replace(tmp, out)
	report(check(out, None), "warning: ")
	print(f"wrote {out}: {len(entries)} entries")


def check(path, res):
	"""Everything the client's zip reader and brick database would reject, as a list of messages."""
	problems = []
	data = open(path, "rb").read()
	end = data.rfind(struct.pack("<I", SIG_END), max(0, len(data) - 0xFFFF - 22))
	if end < 0:
		return ["no end of central directory record"]
	_, disk, cd_disk, count_disk, count, cd_size, cd_offset, _ = struct.unpack_from("<IHHHHIIH", data, end)
	if disk or cd_disk or count_disk != count:
		problems.append("multi-disk archive (disk numbers must be 0)")
	if cd_offset + cd_size != end:
		gap = data[cd_offset + cd_size:end]
		kind = "ZIP64 end records" if gap[:4] == struct.pack("<I", SIG_END64) else f"{len(gap)} unexpected bytes"
		problems.append(f"{kind} between the central directory and its end record (the client needs them adjacent)")
	if cd_offset + cd_size > end or cd_offset > len(data):
		return problems + ["central directory offset is outside the file (data before the zip?)"]

	names = set()
	owners = {}  # design ID or alias -> primitives that claim it
	pos = cd_offset
	for _ in range(count):
		if struct.unpack_from("<I", data, pos)[0] != SIG_CENTRAL:
			problems.append(f"central directory entry at {pos} has a bad signature")
			break
		(_, _, _, flags, method, _, _, crc, csize, usize, name_len, extra_len, comment_len,
			_, _, _, local) = struct.unpack_from("<IHHHHHHIIIHHHHHII", data, pos)
		raw = data[pos + 46:pos + 46 + name_len]
		name = raw.decode("utf-8" if flags & 0x800 else "cp437")
		pos += 46 + name_len + extra_len + comment_len
		where = f"{name!r}:"
		if "\\" in name:
			problems.append(f"{where} backslash in the path (the client only splits on '/')")
		parts = name.rstrip("/").split("/")
		if name.startswith("/") or "" in parts or "." in parts or ".." in parts:
			problems.append(f"{where} empty, '.' or '..' path component")
		if flags & 1:
			problems.append(f"{where} encrypted")
		if method not in (0, 8):
			problems.append(f"{where} compression method {method} (only stored and deflate are read)")
		if struct.unpack_from("<I", data, local)[0] != SIG_LOCAL:
			problems.append(f"{where} local header missing")
			continue
		_, _, lflags, lmethod, _, _, lcrc, lcsize, lusize, lname, lextra = struct.unpack_from("<IHHHHHIIIHH", data, local)
		if lflags != flags or lmethod != method:
			problems.append(f"{where} local header flags/method differ from the central directory")
		if flags & 8:
			if lcrc or lcsize or lusize:
				problems.append(f"{where} data descriptor flag set but the local header has sizes (the client needs zeros)")
		elif (lcrc, lcsize, lusize) != (crc, csize, usize):
			problems.append(f"{where} local header CRC/sizes differ from the central directory")
		if not name.endswith("/"):
			start = local + 30 + lname + lextra
			body = data[start:start + csize]
			try:
				content = body if method == 0 else zlib.decompressobj(-15).decompress(body)
				if zlib.crc32(content) & 0xFFFFFFFF != crc:
					problems.append(f"{where} CRC mismatch (the client does not check it, other tools will)")
			except zlib.error as e:
				problems.append(f"{where} does not inflate ({e})")
				content = None
			names.add(name)
			folder, _, file = name.partition("/")
			if folder in NUMBERED_DIRS and "/" not in file and file.lower().endswith(NUMBERED_DIRS[folder]):
				stem = os.path.splitext(file)[0]
				if not stem.isdigit():
					problems.append(f"{where} name is not a design ID; the client parses the file name as the ID")
				if content is not None:
					try:
						root = ElementTree.fromstring(content)
					except ElementTree.ParseError as e:
						problems.append(f"{where} XML does not parse ({e}); this brick will be skipped")
						root = None
					if root is not None and folder == "Primitives":
						ids = {stem}
						for annotation in root.iter("Annotation"):
							ids.update(a for a in annotation.get("aliases", "").split(";") if a)
						for i in ids:
							owners.setdefault(i, []).append(name)

	for i, claimed in sorted(owners.items()):
		if len(claimed) > 1:
			problems.append(f"design ID/alias {i} is claimed by {', '.join(claimed)}")
	lower = {n.lower() for n in names}
	if "info.xml" not in lower:
		problems.append("no info.xml at the top level")
	primitives = sorted(n for n in names if n.lower().startswith("primitives/") and n.lower().endswith(".xml"))
	if not primitives:
		problems.append("no Primitives/*.xml at the top level (wrapping folder?); the client drops the whole database")
	if res:
		for n in primitives:
			stem = os.path.splitext(n.split("/")[-1])[0]
			if stem.isdigit() and not os.path.exists(os.path.join(res, "brickprimitives", "lod0", stem + ".g")):
				problems.append(f"{n}: no brickprimitives/lod0/{stem}.g")
	return problems


def report(problems, prefix=""):
	"""Prints the problems, at most three entries for each kind."""
	kinds = {}
	for p in problems:
		kind = p.split(": ", 1)[1] if p.startswith("'") and ": " in p else p
		kinds.setdefault(kind, []).append(p)
	for kind, items in kinds.items():
		for p in items[:3]:
			print(prefix + p)
		if len(items) > 3:
			print(f"{prefix}... and {len(items) - 3} more entries: {kind}")
	print(f"{len(problems)} problem(s)")


def main():
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	sub = parser.add_subparsers(dest="command", required=True)
	b = sub.add_parser("build", help="write a brickdb.zip the client accepts")
	b.add_argument("src")
	b.add_argument("out")
	b.add_argument("--overlay", action="append", default=[])
	c = sub.add_parser("check", help="list what the client would reject")
	c.add_argument("zip")
	c.add_argument("--res", help="the client's res folder, to look for geometry")
	args = parser.parse_args()
	if args.command == "build":
		build(args.src, args.out, args.overlay)
	else:
		problems = check(args.zip, args.res)
		report(problems)
		sys.exit(1 if problems else 0)


if __name__ == "__main__":
	main()
