#!/usr/bin/env python3
"""
Writes the member lists the capture viewer shows packets with, from the packet structs themselves:

  dGame/dGameMessages/GameMessageFields.inc   from dGame/dGameMessages/*Messages.h (game messages, NetGameMsg)
  dNet/PacketFields.inc                       from dNet/*Packets.h (LU packets, LUBitStream)

For every struct: its members by name, in declaration order (what is shown after the struct's own Deserialize read
the packet); for every wire struct: its message ID, direction and whether it can be read. The lists come from the
struct definitions, so a member added to a struct shows up in the viewer, and the build fails if a member's type has
no ToJson overload (GameMessageDecoder.cpp, PacketDecoder.cpp). Members that hold secrets are never listed.

  gen_game_message_fields.py            rewrite the files
  gen_game_message_fields.py --check    exit 1 if a file is out of date (the PacketFieldsUpToDate test)
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

NAMESPACE = re.compile(r"^\s*namespace\s+(\w+)\s*\{")
STRUCT = re.compile(r"^(\s*)struct\s+(\w+)\s*(?:final\s*)?(?::\s*public\s+(\w+))?\s*\{")
MEMBER = re.compile(r"^\s*((?:const\s+)?[\w:]+(?:<.*>)?\s*\*?)\s+(\w+)\s*(?:\{.*\}|=.*)?;\s*$")
SERVICES = {"Auth": "AUTH", "Chat": "CHAT", "Client": "CLIENT", "Master": "MASTER", "Server": "COMMON", "World": "WORLD"}
SKIP_TYPES = ("static", "using", "friend", "return", "virtual", "typedef", "enum", "constexpr", "inline")
# Never shown, even blanked
SECRETS = {"password", "sessionKey", "userKey", "cdnKey", "passwordHash"}
# Set by the server, not read from the wire
NOT_WIRE = {"sysAddr"}
# Have their own ToJson (private members)
SKIP_STRUCTS = {"LWOZONEID"}


def strip_comment(line):
	line = re.sub(r"/\*.*?\*/", "", line)
	return line.split("//", 1)[0]


def parse(path):
	structs = []
	stack = []  # (kind, value, depth): kind "ns" or "struct"
	depth = 0
	in_block_comment = False
	for raw in path.read_text().splitlines():
		line = raw
		if in_block_comment:
			if "*/" not in line:
				continue
			line = line.split("*/", 1)[1]
			in_block_comment = False
		line = strip_comment(line)
		if "/*" in line:
			line = line.split("/*", 1)[0]
			in_block_comment = True
		namespaces = [v for k, v, _ in stack if k == "ns"]
		parents = [v for k, v, _ in stack if k == "struct"]
		if (n := NAMESPACE.match(line)) and not parents:
			depth += line.count("{") - line.count("}")
			stack.append(("ns", n.group(1), depth))
			continue
		m = STRUCT.match(line)
		if m and not line.rstrip().endswith(";"):
			name = m.group(2)
			qualified = "::".join(namespaces + [p["short"] for p in parents] + [name])
			s = {"name": qualified, "short": "::".join([p["short"] for p in parents] + [name]), "base": m.group(3),
				 "members": [], "types": [], "id": None, "enum": None, "handle": False, "deserialize": False, "file": path.name}
			structs.append(s)
			depth += line.count("{") - line.count("}")
			stack.append(("struct", s, depth))
			continue
		current = parents[-1] if parents else None
		if current is not None and stack[-1][0] == "struct" and depth == stack[-1][2]:
			own = current["short"].split("::")[-1]
			if (mid := re.search(rf"\b{own}\s*\(\s*\)\s*:.*MessageType::(\w+)::(\w+)", line)):
				current["enum"], current["id"] = mid.group(1), mid.group(2)
			if re.search(r"\bvoid\s+Handle\s*\(", line):
				current["handle"] = True
			if re.search(r"\bbool\s+Deserialize\s*\(", line):
				current["deserialize"] = True
			mm = MEMBER.match(line)
			if mm and "(" not in line.split("{")[0].split("=")[0] and ")" not in line:
				typ, name = mm.group(1).strip(), mm.group(2)
				if typ.split()[0] not in SKIP_TYPES and not typ.endswith("*"):
					current["members"].append(name)
					current["types"].append(typ)
		depth += line.count("{") - line.count("}")
		while stack and depth < stack[-1][2]:
			stack.pop()
	return structs


def reachable(structs, wire):
	"""The wire structs, their bases and the structs their members hold: the only ones shown"""
	by_short = {}
	for s in structs:
		by_short.setdefault(s["name"].split("::")[-1], []).append(s)
	keep, todo = [], list(wire)
	while todo:
		s = todo.pop()
		if s in keep:
			continue
		keep.append(s)
		words = re.findall(r"\w+", " ".join(s["types"])) + [s["base"] or ""]
		for word in words:
			todo += by_short.get(word, [])
	return [s for s in structs if s in keep]


def member_lines(structs):
	known = {s["name"].split("::")[-1]: s["name"] for s in structs}
	lines = ["// Every struct's members, in declaration order (a base struct's first)"]
	for s in structs:
		lines.append(f"json ToJson(const {s['name']}& m);")
	lines.append("")
	for s in structs:
		lines.append(f"json ToJson(const {s['name']}& m) {{")
		base = known.get(s["base"] or "")
		lines.append(f"\tjson j = ToJson(static_cast<const {base}&>(m));" if base else "\tjson j = json::object();")
		for member in s["members"]:
			if member in SECRETS or member in NOT_WIRE:
				continue
			lines.append(f"\tj[\"{member}\"] = ToJson(m.{member});")
		lines.append("\treturn j;")
		lines.append("}")
	lines.append("")
	return lines


def game_messages():
	directory = ROOT / "dGame" / "dGameMessages"
	structs = []
	for path in sorted(directory.glob("*Messages.h")):
		if path.name == "GameMessages.h":
			continue
		structs += [s for s in parse(path) if s["base"] != "GameMsg"]
	# Constructors defined in the .cpp
	for s in structs:
		source = directory / s["file"].replace(".h", ".cpp")
		if s["base"] == "NetGameMsg" and not s["id"] and source.exists():
			own = s["short"].split("::")[-1]
			if (mid := re.search(rf"\b{own}::{own}\s*\(\s*\)\s*:\s*NetGameMsg\(\s*MessageType::Game::(\w+)", source.read_text())):
				s["enum"], s["id"] = "Game", mid.group(1)
	wire = [s for s in structs if s["enum"] == "Game" and s["id"]]
	lines = member_lines(reachable(structs, wire))
	lines.append("// Wire messages: (ID, direction, reader). A struct with Handle is what the server reads from a client.")
	lines.append("// Structs with members but no Deserialize can't be read.")
	lines.append("const std::vector<Entry>& Entries() {")
	lines.append("\tstatic const std::vector<Entry> entries{")
	for s in wire:
		direction = "eDirection::TO_SERVER" if s["handle"] else "eDirection::TO_CLIENT"
		readable = s["deserialize"] or not s["members"]
		reader = f"&ReadWith<{s['name']}>" if readable else "nullptr"
		lines.append(f"\t\t{{ MessageType::Game::{s['id']}, {direction}, \"{s['short']}\", {reader} }},")
	lines.append("\t};")
	lines.append("\treturn entries;")
	lines.append("}")
	unreadable = [s["name"] for s in wire if s["members"] and not s["deserialize"]]
	return directory / "GameMessageFields.inc", lines, f"{len(wire)} game messages, unreadable: {unreadable}"


def packets():
	directory = ROOT / "dNet"
	structs = []
	for path in sorted(directory.glob("*Packets.h")) + [directory / "WorldRoutePacket.h"] + sorted((directory / "master").glob("*.h")) + [ROOT / "dCommon" / name for name in ("PositionUpdate.h", "Profiler.h", "TrafficStats.h", "ZoneFileLog.h", "dEnums/dCommonVars.h")]:
		structs += [s for s in parse(path) if s["name"] not in SKIP_STRUCTS]
	wire = [s for s in structs if s["enum"] in SERVICES and s["id"]]
	lines = member_lines(reachable(structs, wire))
	lines.append("// Wire packets: (service, ID, struct, reader). An ID can have more than one struct (one per direction):")
	lines.append("// the first that reads the whole packet is shown.")
	lines.append("const std::vector<Entry>& Entries() {")
	lines.append("\tstatic const std::vector<Entry> entries{")
	for s in wire:
		enum, service = s["enum"], SERVICES[s["enum"]]
		readable = s["deserialize"] or not s["members"]
		reader = f"&ReadWith<{s['name']}>" if readable else "nullptr"
		lines.append(f"\t\t{{ ServiceType::{service}, static_cast<uint32_t>(MessageType::{enum}::{s['id']}), \"{s['short']}\", {s['handle'] and 'true' or 'false'}, {reader} }},")
	lines.append("\t};")
	lines.append("\treturn entries;")
	lines.append("}")
	unreadable = [s["name"] for s in wire if s["members"] and not s["deserialize"]]
	return directory / "PacketFields.inc", lines, f"{len(wire)} packets, unreadable: {unreadable}"


def main():
	status = 0
	for out, lines, summary in (game_messages(), packets()):
		header = [
			"// Generated by tools/gen_game_message_fields.py from the packet structs. Do not edit: run the script.",
			"",
		]
		text = "\n".join(header + lines) + "\n"
		if "--check" in sys.argv:
			if not out.exists() or out.read_text() != text:
				print(f"{out.relative_to(ROOT)} is out of date: run tools/gen_game_message_fields.py", file=sys.stderr)
				status = 1
			continue
		out.write_text(text)
		print(f"{out.relative_to(ROOT)}: {summary}")
	return status


if __name__ == "__main__":
	sys.exit(main())
