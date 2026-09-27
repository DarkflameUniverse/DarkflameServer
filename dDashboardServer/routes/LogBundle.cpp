#include "LogBundle.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>

#include "GeneralUtils.h"
#include "ZCompression.h"

namespace fs = std::filesystem;

namespace {
	constexpr std::string_view SERVERS[][2] = {
		{ "master", "MasterServer" }, { "auth", "AuthServer" }, { "chat", "ChatServer" },
		{ "dashboard", "DashboardServer" }, { "ugc", "UgcServer" }, { "world", "WorldServer" },
	};

	std::optional<uint32_t> Number(std::string_view text) {
		if (text.empty() || text.size() > 10 || !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; })) return std::nullopt;
		const auto value = std::stoull(std::string(text));
		if (value > UINT32_MAX) return std::nullopt;
		return static_cast<uint32_t>(value);
	}

	std::vector<std::string_view> Split(std::string_view text, char separator) {
		std::vector<std::string_view> parts;
		size_t start = 0;
		while (true) {
			const auto end = text.find(separator, start);
			parts.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
			if (end == std::string_view::npos) return parts;
			start = end + 1;
		}
	}

	bool KnownServer(std::string_view name) {
		return std::any_of(std::begin(SERVERS), std::end(SERVERS), [&](const auto& s) { return s[1] == name; });
	}

	std::tm LocalTime(int64_t time) {
		const auto t = static_cast<std::time_t>(time);
		std::tm out{};
#ifdef _WIN32
		localtime_s(&out, &t);
#else
		localtime_r(&t, &out);
#endif
		return out;
	}

	std::string TimeText(int64_t time) {
		if (time <= 0) return "-";
		const auto tm = LocalTime(time);
		char text[32];
		std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &tm);
		return text;
	}

	int64_t WriteTime(const fs::path& path) {
		std::error_code ec;
		const auto time = fs::last_write_time(path, ec);
		if (ec) return 0;
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::clock_cast<std::chrono::system_clock>(time).time_since_epoch()).count();
	}

	bool Matches(const LogBundle::LogName& name, const LogBundle::Filter& filter) {
		if (!filter.servers.empty() && !filter.servers.contains(name.server)) return false;
		if (name.server != "WorldServer") return true;
		if (!filter.zones.empty() && (!name.zone || !filter.zones.contains(*name.zone))) return false;
		if (filter.clone && name.clone != filter.clone) return false;
		if (filter.instance && name.instance != filter.instance) return false;
		return true;
	}

	bool Overlaps(int64_t started, int64_t written, const LogBundle::Filter& filter) {
		return (filter.to == 0 || started <= filter.to) && (filter.from == 0 || written >= filter.from);
	}

	bool IsHex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
	bool IsDigit(char c) { return c >= '0' && c <= '9'; }
	bool IsWordChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

	// Writes a zip file a piece at a time: each entry's sizes and CRC follow its data (flag bit 3), so nothing needs
	// to be known before it's compressed. Without zip64, so everything must stay under 4 GB (the size limit sees to it).
	class ZipWriter {
	public:
		explicit ZipWriter(const fs::path& path) : m_File(path, std::ios::binary | std::ios::trunc) {}
		bool Ok() const { return m_File.good(); }
		uint64_t Size() const { return m_Offset; }

		bool Begin(const std::string& name, int64_t time) {
			m_Entry = { name, static_cast<uint32_t>(m_Offset), DosTime(time) };
			std::string header;
			Put32(header, 0x04034b50);
			Put16(header, 20); // version needed: deflate
			Put16(header, 0x0808); // sizes after the data, UTF-8 names
			Put16(header, 8); // deflate
			Put32(header, m_Entry.dosTime);
			Put32(header, 0); Put32(header, 0); Put32(header, 0);
			Put16(header, static_cast<uint16_t>(name.size()));
			Put16(header, 0);
			header += name;
			m_Crc = 0;
			m_Deflater = std::make_unique<ZCompression::RawDeflater>([this](std::string_view data) { return Raw(data); });
			return Raw(header);
		}

		bool Write(std::string_view data) {
			m_Crc = ZCompression::Crc32(m_Crc, data);
			return m_Deflater->Write(data);
		}

		bool End() {
			if (!m_Deflater->Finish()) return false;
			m_Entry.crc = m_Crc;
			m_Entry.compressed = static_cast<uint32_t>(m_Deflater->BytesOut());
			m_Entry.size = static_cast<uint32_t>(m_Deflater->BytesIn());
			m_Deflater.reset();
			std::string descriptor;
			Put32(descriptor, 0x08074b50);
			Put32(descriptor, m_Entry.crc);
			Put32(descriptor, m_Entry.compressed);
			Put32(descriptor, m_Entry.size);
			m_Entries.push_back(m_Entry);
			return Raw(descriptor);
		}

		bool Close() {
			const auto start = m_Offset;
			std::string directory;
			for (const auto& entry : m_Entries) {
				Put32(directory, 0x02014b50);
				Put16(directory, 0x0314); // made by: Unix, 2.0 (so the permissions below count)
				Put16(directory, 20);
				Put16(directory, 0x0808);
				Put16(directory, 8);
				Put32(directory, entry.dosTime);
				Put32(directory, entry.crc);
				Put32(directory, entry.compressed);
				Put32(directory, entry.size);
				Put16(directory, static_cast<uint16_t>(entry.name.size()));
				Put16(directory, 0); Put16(directory, 0); Put16(directory, 0); Put16(directory, 0);
				Put32(directory, 0100644u << 16); // a plain file, rw-r--r--
				Put32(directory, entry.offset);
				directory += entry.name;
			}
			const auto directorySize = directory.size();
			Put32(directory, 0x06054b50);
			Put16(directory, 0); Put16(directory, 0);
			Put16(directory, static_cast<uint16_t>(m_Entries.size()));
			Put16(directory, static_cast<uint16_t>(m_Entries.size()));
			Put32(directory, static_cast<uint32_t>(directorySize));
			Put32(directory, static_cast<uint32_t>(start));
			Put16(directory, 0);
			if (!Raw(directory)) return false;
			m_File.close();
			return !m_File.fail();
		}

	private:
		struct Entry {
			std::string name;
			uint32_t offset{};
			uint32_t dosTime{};
			uint32_t crc{}, compressed{}, size{};
		};

		static void Put16(std::string& out, uint16_t value) { out += static_cast<char>(value & 0xFF); out += static_cast<char>(value >> 8); }
		static void Put32(std::string& out, uint32_t value) { Put16(out, value & 0xFFFF); Put16(out, value >> 16); }

		static uint32_t DosTime(int64_t time) {
			const auto tm = LocalTime(std::max<int64_t>(time, 315532800)); // zip times start in 1980
			const uint32_t date = ((tm.tm_year - 80) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday;
			const uint32_t clock = (tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec / 2);
			return (date << 16) | clock;
		}

		bool Raw(std::string_view data) {
			m_File.write(data.data(), static_cast<std::streamsize>(data.size()));
			m_Offset += data.size();
			return m_File.good();
		}

		std::ofstream m_File;
		uint64_t m_Offset{};
		Entry m_Entry;
		uint32_t m_Crc{};
		std::unique_ptr<ZCompression::RawDeflater> m_Deflater;
		std::vector<Entry> m_Entries;
	};
}

namespace LogBundle {
	std::optional<LogName> ParseLogName(std::string_view fileName) {
		if (!fileName.ends_with(".log")) return std::nullopt;
		fileName.remove_suffix(4);
		const auto parts = Split(fileName, '_');
		if (parts.empty() || !KnownServer(parts[0])) return std::nullopt;
		LogName name{ std::string(parts[0]) };
		std::vector<uint32_t> numbers;
		for (size_t i = 1; i < parts.size(); i++) {
			const auto number = Number(parts[i]);
			if (!number) return std::nullopt;
			numbers.push_back(*number);
		}
		if (name.server == "WorldServer") {
			// WorldServer_<zone>_<clone>_<instance>_<time>, or just WorldServer_<time> from before the zone was known
			if (numbers.size() == 4) {
				name.zone = numbers[0];
				name.clone = numbers[1];
				name.instance = numbers[2];
				name.started = numbers[3];
			} else if (numbers.size() == 1) {
				name.started = numbers[0];
			} else if (!numbers.empty()) return std::nullopt;
		} else {
			if (numbers.size() > 1) return std::nullopt;
			if (numbers.size() == 1) name.started = numbers[0];
		}
		return name;
	}

	std::optional<LogName> ParseCrashName(std::string_view fileName) {
		if (!fileName.starts_with("Crash_") || !fileName.ends_with(".log")) return std::nullopt;
		fileName.remove_prefix(6);
		fileName.remove_suffix(4);
		const auto pid = fileName.rfind('_'); // the process ID
		if (pid == std::string_view::npos || !Number(fileName.substr(pid + 1))) return std::nullopt;
		return ParseLogName(std::string(fileName.substr(0, pid)) + ".log");
	}

	std::optional<std::string> ServerFromShortName(std::string_view name) {
		for (const auto& server : SERVERS) {
			if (server[0] == name || server[1] == name) return std::string(server[1]);
		}
		return std::nullopt;
	}

	std::optional<Filter> Filter::FromQuery(const std::function<std::string(const std::string&)>& value, std::string& error) {
		Filter filter;
		const auto time = [&](const std::string& key, int64_t& out) {
			const auto text = value(key);
			if (text.empty()) return true;
			const auto parsed = GeneralUtils::TryParse<int64_t>(text);
			if (!parsed || *parsed < 0) { error = key + " must be a Unix time in seconds"; return false; }
			out = *parsed;
			return true;
		};
		if (!time("from", filter.from) || !time("to", filter.to)) return std::nullopt;
		if (filter.from && filter.to && filter.from > filter.to) { error = "from is after to"; return std::nullopt; }
		if (const auto servers = value("servers"); !servers.empty()) {
			for (const auto part : Split(servers, ',')) {
				const auto server = ServerFromShortName(part);
				if (!server) { error = "Unknown server " + std::string(part) + " (master, auth, chat, dashboard, ugc or world)"; return std::nullopt; }
				filter.servers.insert(*server);
			}
		}
		if (const auto zones = value("zones"); !zones.empty()) {
			for (const auto part : Split(zones, ',')) {
				const auto zone = Number(part);
				if (!zone) { error = "zones must be zone IDs separated by commas"; return std::nullopt; }
				filter.zones.insert(*zone);
			}
		}
		for (const auto& [key, out] : { std::pair{ "clone", &filter.clone }, std::pair{ "instance", &filter.instance } }) {
			const auto text = value(key);
			if (text.empty()) continue;
			*out = Number(text);
			if (!*out) { error = std::string(key) + " must be a number"; return std::nullopt; }
		}
		filter.crashDumps = value("crash") == "1";
		filter.trim = value("trim") == "1";
		filter.redactIps = value("redact") == "1";
		filter.text = value("text");
		if (filter.text.size() > 200) { error = "text is longer than 200 characters"; return std::nullopt; }
		std::transform(filter.text.begin(), filter.text.end(), filter.text.begin(), [](unsigned char c) { return std::tolower(c); });
		return filter;
	}

	nlohmann::json Filter::ToJson() const {
		nlohmann::json json{ {"from", from}, {"to", to}, {"servers", servers}, {"zones", zones}, {"crash_dumps", crashDumps},
			{"trim", trim}, {"text", text}, {"redact_ips", redactIps} };
		json["clone"] = clone ? nlohmann::json(*clone) : nlohmann::json(nullptr);
		json["instance"] = instance ? nlohmann::json(*instance) : nlohmann::json(nullptr);
		return json;
	}

	std::vector<File> Select(const fs::path& logFolder, const fs::path& dumpFolder, const Filter& filter) {
		std::vector<File> files;
		std::error_code ec;
		for (auto it = fs::recursive_directory_iterator(logFolder, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
			if (!it->is_regular_file(ec)) continue;
			const auto name = ParseLogName(it->path().filename().string());
			if (!name || !Matches(*name, filter)) continue;
			File file{ it->path(), "logs/" + fs::relative(it->path(), logFolder, ec).generic_string(), *name };
			file.written = WriteTime(it->path());
			file.started = name->started ? name->started : file.written;
			file.size = it->file_size(ec);
			if (Overlaps(file.started, file.written, filter)) files.push_back(std::move(file));
		}
		if (filter.crashDumps && !dumpFolder.empty()) {
			for (const auto& entry : fs::directory_iterator(dumpFolder, ec)) {
				if (!entry.is_regular_file(ec)) continue;
				if (!entry.path().filename().string().starts_with("Crash_")) continue;
				// A dump written by a process this can't name still goes in when every server is wanted
				auto name = ParseCrashName(entry.path().filename().string());
				if (name ? !Matches(*name, filter) : !filter.servers.empty() || !filter.zones.empty()) continue;
				File file{ entry.path(), "crash_dumps/" + entry.path().filename().string(), name.value_or(LogName{}) };
				file.crashDump = true;
				file.written = WriteTime(entry.path());
				file.started = file.name.started ? file.name.started : file.written;
				file.size = entry.file_size(ec);
				if (Overlaps(file.started, file.written, filter)) files.push_back(std::move(file));
			}
		}
		std::sort(files.begin(), files.end(), [](const File& a, const File& b) {
			return std::tie(a.started, a.archiveName) < std::tie(b.started, b.archiveName);
		});
		return files;
	}

	std::optional<int64_t> LineTime(std::string_view line) {
		// [dd-mm-yy HH:MM:SS
		if (line.size() < 19 || line[0] != '[' || line[3] != '-' || line[6] != '-' || line[9] != ' ' || line[12] != ':' || line[15] != ':') return std::nullopt;
		for (const size_t at : { 1, 2, 4, 5, 7, 8, 10, 11, 13, 14, 16, 17 }) {
			if (!IsDigit(line[at])) return std::nullopt;
		}
		const auto two = [&](size_t at) { return (line[at] - '0') * 10 + (line[at + 1] - '0'); };
		std::tm tm{};
		tm.tm_mday = two(1);
		tm.tm_mon = two(4) - 1;
		tm.tm_year = 100 + two(7);
		tm.tm_hour = two(10);
		tm.tm_min = two(13);
		tm.tm_sec = two(16);
		tm.tm_isdst = -1;
		const auto time = std::mktime(&tm);
		if (time == -1) return std::nullopt;
		return static_cast<int64_t>(time);
	}

	std::string RedactIps(std::string_view line) {
		std::string out;
		out.reserve(line.size());
		size_t i = 0;
		while (i < line.size()) {
			const bool boundary = i == 0 || (!IsWordChar(line[i - 1]) && line[i - 1] != '.' && line[i - 1] != ':');
			if (boundary && (IsHex(line[i]) || line[i] == ':')) {
				// The longest run of hex digits, dots and colons from here
				size_t end = i;
				while (end < line.size() && (IsHex(line[end]) || line[end] == '.' || line[end] == ':')) end++;
				// A run that ends in a word ("Foo::Bar" is not an address)
				if (end < line.size() && IsWordChar(line[end])) { out += line.substr(i, end - i); i = end; continue; }
				auto run = line.substr(i, end - i);
				// Trailing punctuation isn't part of an address ("from 1.2.3.4." or "at ::1:")
				while (!run.empty() && (run.back() == '.' || (run.back() == ':' && !run.ends_with("::")))) run.remove_suffix(1);
				const auto colons = std::count(run.begin(), run.end(), ':');
				const auto dots = std::count(run.begin(), run.end(), '.');
				bool address = false;
				if (colons == 0 && dots == 3) {
					// IPv4: four numbers up to 255
					const auto parts = Split(run, '.');
					address = std::all_of(parts.begin(), parts.end(), [](std::string_view p) {
						return !p.empty() && p.size() <= 3 && std::all_of(p.begin(), p.end(), IsDigit) && std::stoi(std::string(p)) <= 255;
					});
				} else if (colons >= 2 && (dots == 0 || dots == 3)) {
					// IPv6: eight groups, or fewer around a "::"; times like 09:49:54 have neither
					const bool compressed = run.find("::") != std::string_view::npos;
					const auto groups = Split(run.substr(0, dots ? run.rfind(':') : run.size()), ':');
					const bool groupsOk = std::all_of(groups.begin(), groups.end(), [](std::string_view g) { return g.size() <= 4 && std::all_of(g.begin(), g.end(), IsHex); });
					address = groupsOk && (compressed || colons == 7) && std::any_of(run.begin(), run.end(), IsHex);
				} else if (colons == 1 && dots == 3) {
					// IPv4 with a port: redact the address, keep the port
					const auto colon = run.find(':');
					const auto ip = RedactIps(run.substr(0, colon));
					if (ip != run.substr(0, colon)) { out += ip; out += run.substr(colon); i += run.size(); continue; }
				}
				if (address) out += "[ip]";
				else out += run;
				i += run.size();
				continue;
			}
			out += line[i++];
		}
		return out;
	}

	std::string SizeText(uint64_t bytes) {
		char text[32];
		if (bytes >= 1024ull * 1024 * 1024) std::snprintf(text, sizeof(text), "%.1f GB", bytes / 1073741824.0);
		else if (bytes >= 1024 * 1024) std::snprintf(text, sizeof(text), "%.1f MB", bytes / 1048576.0);
		else if (bytes >= 1024) std::snprintf(text, sizeof(text), "%.1f kB", bytes / 1024.0);
		else std::snprintf(text, sizeof(text), "%llu bytes", static_cast<unsigned long long>(bytes));
		return text;
	}

	Result WriteZip(const fs::path& out, const std::vector<File>& files, const Filter& filter, const std::string& manifestHeader,
		uint64_t maxBytes, const std::function<bool()>& cancelled) {
		Result result;
		std::string manifest = manifestHeader;
		manifest += "\nFiles (archive name, server, first and last time, source size, size in this bundle):\n";
		const auto fail = [&](std::string error) {
			std::error_code ec;
			fs::remove(out, ec);
			result.ok = false;
			result.error = std::move(error);
			return result;
		};
		if (files.size() > 60000) return fail("More than 60,000 files; pick a shorter date range or fewer servers");

		ZipWriter zip(out);
		if (!zip.Ok()) return fail("Can't write " + out.string());
		for (const auto& file : files) {
			if (cancelled && cancelled()) return fail("Cancelled");
			std::ifstream in(file.path, std::ios::binary);
			if (!in) {
				manifest += file.archiveName + "  (couldn't be read)\n";
				continue;
			}
			// Begun with the first bytes, so files with no lines left stay out
			bool begun = false;
			uint64_t written = 0;
			const auto put = [&](std::string_view data) {
				if (!begun && !(begun = zip.Begin(file.archiveName, file.written))) return false;
				written += data.size();
				result.bytesIn += data.size();
				if (maxBytes && result.bytesIn > maxBytes) {
					result.overLimit = true;
					return false;
				}
				return zip.Write(data);
			};
			bool ok = true;
			const bool trim = filter.trim && !file.crashDump && (filter.from || filter.to);
			const bool text = !filter.text.empty() && !file.crashDump;
			if (!trim && !text && !filter.redactIps) {
				// Whole files: copied a piece at a time
				char buffer[64 * 1024];
				while (ok && in) {
					in.read(buffer, sizeof(buffer));
					if (in.gcount() > 0) ok = put(std::string_view(buffer, static_cast<size_t>(in.gcount())));
				}
			} else {
				// A line without a time of its own (a continued message) belongs to the time of the line before
				int64_t lineTime = file.started;
				std::string line, lower, pending;
				while (ok && std::getline(in, line)) {
					if (const auto time = LineTime(line)) lineTime = *time;
					if (trim && ((filter.from && lineTime < filter.from) || (filter.to && lineTime > filter.to))) continue;
					if (text) {
						lower = line;
						std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
						if (lower.find(filter.text) == std::string::npos) continue;
					}
					pending += filter.redactIps ? RedactIps(line) : line;
					pending += '\n';
					if (pending.size() >= 64 * 1024) {
						ok = put(pending);
						pending.clear();
					}
				}
				if (ok && !pending.empty()) ok = put(pending);
			}
			if (!ok) {
				if (result.overLimit) return fail("The bundle would hold more than " + SizeText(maxBytes) + " (log_bundle_max_mb); pick a shorter date range, fewer servers, or only lines with some text");
				return fail("Couldn't write the bundle (is the disk full?)");
			}
			if (begun && !zip.End()) return fail("Couldn't write the bundle (is the disk full?)");
			if (begun) result.files++;
			manifest += file.archiveName + "  " + file.name.server + (file.name.zone ? " zone " + std::to_string(*file.name.zone) : "") +
				"  " + TimeText(file.started) + " to " + TimeText(file.written) + "  " + SizeText(file.size) + "  " +
				(written == 0 ? "(no lines left)" : SizeText(written)) + "\n";
		}
		if (files.empty()) manifest += "(none)\n";
		if (!zip.Begin("manifest.txt", std::time(nullptr)) || !zip.Write(manifest) || !zip.End() || !zip.Close()) {
			return fail("Couldn't write the bundle (is the disk full?)");
		}
		result.ok = true;
		result.archiveSize = zip.Size();
		return result;
	}
}
