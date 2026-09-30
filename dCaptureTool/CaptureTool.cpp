/**
 * The capture tool (docs/CaptureReplay.md): looks into packet bundles, converts the 2014 live captures into bundles,
 * makes anonymous test fixtures, and replays bundles against a throwaway sandbox server stack, comparing the
 * server's answers with the recorded ones.
 */
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "BinaryPathFinder.h"
#include "CaptureBundle.h"
#include "CaptureTools.h"
#include "LiveImport.h"
#include "GameMessageDecoder.h"
#include "PacketDecoder.h"
#include "CDClientDatabase.h"
#include "MessageIdentifiers.h"
#include "ReplicaDecoder.h"
#include "Replayer.h"
#include "Sandbox.h"

#include <random>

#include "dConfig.h"
#include "dServer.h"
#include "Game.h"
#include "Logger.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

// What every program linking the game's libraries defines (the tool runs no server of its own)
namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dConfig* config = nullptr;
	Game::signal_t lastSignal = 0;
	std::mt19937 randomEngine;
	// Also defined by every program that links the game's libraries (it decodes game messages with the game's own
	// message structs); the dashboard runs no game, so they stay empty
	dChatFilter* chatFilter = nullptr;
	AssetManager* assetManager = nullptr;
	RakPeerInterface* chatServer = nullptr;
	SystemAddress chatSysAddr;
	EntityManager* entityManager = nullptr;
	dZoneManager* zoneManager = nullptr;
	std::string projectVersion = PROJECT_VERSION;
}

namespace {
	void Usage() {
		std::cout <<
			"CaptureTool: packet bundles (docs/CaptureReplay.md)\n"
			"  info <bundle>                               what is in a bundle\n"
			"  decode <bundle> [--fields] [--limit N] [--cdserver <CDServer.sqlite>]\n"
			"                                              its packets on one timeline (replica packets too with --cdserver)\n"
			"  anonymise <in> <out>                        a test fixture: names and chat replaced, IDs placeholders\n"
			"  import-live <folder> <out-dir>              convert live captures (every folder of *_traffic.zip under <folder>)\n"
			"  replay <bundle>... [options]                replay against a fresh sandbox stack per bundle and compare\n"
			"      --client <path>        the game client's files (default: client_location of the servers' sharedconfig.ini)\n"
			"      --server-dir <path>    the built servers (default: this tool's folder)\n"
			"      --cdserver <path>      CDServer.sqlite to copy (default: <server-dir>/resServer/CDServer.sqlite)\n"
			"      --sandbox-root <path>  where sandboxes are made (default: the system's temporary folder)\n"
			"      --port <n>             first of the sandbox's ports (default 41000; uses n to n+200)\n"
			"      --mode setup|as-is     setup (default): make the bundle's characters first; as-is: only an account\n"
			"      --speed <x>            how much faster than recorded (default 4)\n"
			"      --keep | --keep-on-failure   keep the sandbox folder\n"
			"      --report <file>        write the results as JSON\n"
			"  replay-target <bundle> --host <h> --auth-port <p> --username <u> --password <p> --i-know-this-is-not-a-sandbox\n"
			"                                              replay against a running server (it must not be a live one)\n";
	}

	std::string Arg(const std::vector<std::string>& args, const std::string& name, const std::string& fallback = "") {
		for (size_t i = 0; i + 1 < args.size(); i++) if (args[i] == name) return args[i + 1];
		return fallback;
	}
	bool Flag(const std::vector<std::string>& args, const std::string& name) {
		for (const auto& arg : args) if (arg == name) return true;
		return false;
	}
	// Arguments that aren't options (nor option values)
	std::vector<std::string> Positional(const std::vector<std::string>& args, size_t from) {
		static const std::vector<std::string> withValue{ "--client", "--server-dir", "--cdserver", "--sandbox-root", "--port", "--mode", "--speed", "--report",
			"--host", "--auth-port", "--username", "--password", "--limit" };
		std::vector<std::string> out;
		for (size_t i = from; i < args.size(); i++) {
			if (std::find(withValue.begin(), withValue.end(), args[i]) != withValue.end()) i++;
			else if (!args[i].starts_with("--")) out.push_back(args[i]);
		}
		return out;
	}

	bool LoadBundle(const std::string& path, CaptureBundle::Bundle& bundle) {
		std::string error;
		bool truncated = false;
		if (!CaptureBundle::Load(path, bundle, error, &truncated)) {
			std::cerr << path << ": " << error << "\n";
			return false;
		}
		if (truncated) std::cerr << path << ": the last record is cut short (a capture still being written?)\n";
		return true;
	}

	std::string ConfigValue(const fs::path& file, const std::string& key) {
		std::ifstream in(file);
		std::string line, value;
		while (std::getline(in, line)) {
			if (line.starts_with(key + "=")) value = line.substr(key.size() + 1);
		}
		return value;
	}

	int Info(const std::string& path) {
		CaptureBundle::Bundle bundle;
		if (!LoadBundle(path, bundle)) return 1;
		std::map<std::string, size_t> names;
		for (const auto& record : bundle.records) names[PacketDecoder::Decode(record.bytes, CaptureTools::FromClient(record.header)).name]++;
		auto meta = bundle.meta;
		if (meta.contains("setup")) for (auto& character : meta["setup"]["characters"]) character["xml"] = std::to_string(character.value("xml", std::string()).size()) + " bytes";
		std::cout << meta.dump(2) << "\n" << bundle.records.size() << " packets\n";
		for (const auto& [name, count] : names) std::cout << "  " << count << "\t" << name << "\n";
		return 0;
	}

	int Decode(const std::string& path, bool fields, size_t limit, const std::string& cdServer) {
		CaptureBundle::Bundle bundle;
		if (!LoadBundle(path, bundle)) return 1;
		CaptureTools::SortTimeline(bundle.records);
		// Replica packets need the components of each LOT, from the CDClient
		ReplicaDecoder::ComponentTable components;
		if (!cdServer.empty()) {
			try {
				CDClientDatabase::Connect(cdServer);
				ReplicaDecoder::LoadComponentTable(components);
			} catch (const std::exception& e) {
				std::cerr << "Can't read " << cdServer << ": " << e.what() << "\n";
				return 1;
			}
		}
		ReplicaDecoder::Session replicas(components);
		size_t constructions = 0, unmatched = 0;
		const auto start = bundle.records.empty() ? 0 : bundle.records.front().header.timeUs;
		for (size_t i = 0; i < bundle.records.size() && i < limit; i++) {
			const auto& record = bundle.records[i];
			std::optional<json> replica;
			if (!cdServer.empty() && !(record.header.flags & PacketRecordFlags::GAP) && !CaptureTools::FromClient(record.header)) {
				replica = replicas.Decode(record.bytes, CaptureTools::ReplicaConnection(record.header));
				if (replica && !record.bytes.empty() && static_cast<uint8_t>(record.bytes[0]) == ID_REPLICA_MANAGER_CONSTRUCTION) {
					constructions++;
					if (replica->contains("(layout did not match)")) unmatched++;
				}
			}
			const auto j = CaptureTools::RecordJson(record, i, start, fields, replica ? &*replica : nullptr);
			std::printf("%7zu %10.3f %-12s %-18s %s%s\n", i, j["t"].get<double>() / 1000.0, (j.value("from", std::string()) + ">" + j.value("to", std::string())).c_str(),
				j.value("source", std::string()).c_str(), j.value("name", std::string()).c_str(), fields && j.contains("fields") ? (" " + j["fields"].dump()).c_str() : "");
		}
		if (!cdServer.empty()) std::printf("%zu constructions, %zu whose components didn't read exactly\n", constructions, unmatched);
		return 0;
	}

	int Anonymise(const std::string& in, const std::string& out) {
		CaptureBundle::Bundle bundle;
		if (!LoadBundle(in, bundle)) return 1;
		if (!bundle.meta.value("portable", false)) CaptureTools::MakePortable(bundle);
		const auto changed = CaptureTools::Anonymise(bundle);
		// The setup section's names too
		if (bundle.meta.contains("setup")) for (auto& character : bundle.meta["setup"]["characters"]) character["name"] = character.value("symbol", std::string("replay"));
		if (!CaptureBundle::Save(out, bundle)) {
			std::cerr << "Can't write " << out << "\n";
			return 1;
		}
		std::cout << "Wrote " << out << " (" << changed << " packets changed). Keep it local: tests/fixtures-local is never committed.\n";
		return 0;
	}

	int ImportLive(const fs::path& root, const fs::path& outDir) {
		std::error_code ec;
		fs::create_directories(outDir, ec);
		auto scenarios = LiveImport::FindScenarios(root);
		if (scenarios.empty() && fs::is_regular_file(root)) scenarios.push_back(root);
		size_t written = 0;
		for (const auto& scenario : scenarios) {
			auto result = LiveImport::Import(scenario);
			if (!result.error.empty()) {
				std::cout << "skipped " << scenario << ": " << result.error << "\n";
				continue;
			}
			// A file name from the folders it came from
			auto name = fs::relative(scenario, fs::is_directory(root) ? root : root.parent_path(), ec).string();
			for (auto& c : name) if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '_';
			const auto out = outDir / (name + ".bundle");
			if (!CaptureBundle::Save(out, result.bundle)) {
				std::cout << "can't write " << out << "\n";
				continue;
			}
			written++;
			std::cout << out.filename().string() << ": " << result.zips << " zip(s), " << result.packets << " packets, " << result.skipped << " skipped, "
				<< result.bundle.meta["setup"]["characters"].size() << " character(s)\n";
		}
		std::cout << written << " bundle(s) in " << outDir << "\n";
		return written ? 0 : 1;
	}

	void PrintResult(const std::string& name, const Replayer::Result& r) {
		std::cout << "== " << name << "\n"
			<< "   connections " << r.connectionsReached << "/" << r.connections << ", sent " << r.sent << ", received " << r.received
			<< (r.stoppedAt.empty() ? "" : ", stopped: " + r.stoppedAt) << "\n"
			<< "   answers: " << r.diff.expected << " recorded, " << r.diff.matched << " same, " << r.diff.differing << " different, " << r.diff.missing
			<< " missing, " << r.diff.extra << " extra\n";
		const auto top = [](const std::map<std::string, size_t>& counts, const char* what) {
			std::vector<std::pair<size_t, std::string>> sorted;
			for (const auto& [n, c] : counts) sorted.push_back({ c, n });
			std::sort(sorted.rbegin(), sorted.rend());
			if (sorted.empty()) return;
			std::cout << "   " << what << ":";
			for (size_t i = 0; i < sorted.size() && i < 6; i++) std::cout << " " << sorted[i].second << " (" << sorted[i].first << ")";
			std::cout << "\n";
		};
		top(r.diff.differingByName, "different");
		top(r.diff.missingByName, "missing");
		top(r.diff.extraByName, "extra");
		for (const auto& note : r.notes) std::cout << "   note: " << note << "\n";
	}

	int Replay(const std::vector<std::string>& args) {
		const auto bundles = Positional(args, 2);
		if (bundles.empty()) {
			Usage();
			return 1;
		}
		Sandbox::Options options;
		options.serverDir = fs::absolute(Arg(args, "--server-dir", BinaryPathFinder::GetBinaryDir().string()));
		options.root = Arg(args, "--sandbox-root", fs::temp_directory_path().string());
		options.cdServer = Arg(args, "--cdserver", (options.serverDir / "resServer" / "CDServer.sqlite").string());
		options.clientLocation = Arg(args, "--client", ConfigValue(options.serverDir / "sharedconfig.ini", "client_location"));
		options.basePort = static_cast<uint16_t>(std::stoi(Arg(args, "--port", "41000")));
		options.keep = Flag(args, "--keep");
		// The servers' own database, which a sandbox must never be
		const auto live = ConfigValue(options.serverDir / "sharedconfig.ini", "sqlite_database_path");
		if (!live.empty()) options.liveDatabase = fs::absolute(options.serverDir / live);
		if (options.clientLocation.empty()) {
			std::cerr << "Where is the game client? Pass --client <path>\n";
			return 1;
		}
		const bool setup = Arg(args, "--mode", "setup") != "as-is";
		json report = json::array();
		int failures = 0;
		for (const auto& path : bundles) {
			CaptureBundle::Bundle bundle;
			if (!LoadBundle(path, bundle)) {
				failures++;
				continue;
			}
			std::string error;
			auto stack = Sandbox::Stack::Create(options, error);
			if (!stack) {
				std::cerr << error << "\n";
				return 1;
			}
			const std::string username = "replay", password = "replay-sandbox";
			json ids;
			Replayer::Result result;
			if (!stack->Setup(path, username, password, setup, ids, error) || !stack->Start(error)) {
				result.stoppedAt = error;
			} else {
				Replayer::Options replay;
				replay.authPort = stack->AuthPort();
				replay.username = username;
				replay.password = password;
				replay.speed = std::stod(Arg(args, "--speed", "4"));
				for (const auto& [symbol, character] : ids.items()) {
					const auto placeholder = std::stoll(character["placeholder"].get<std::string>());
					const auto id = std::stoll(character["id"].get<std::string>());
					replay.ids[placeholder] = id;
					if (!replay.character) replay.character = id;
				}
				result = Replayer::Replay(bundle, replay);
				stack->Stop();
			}
			// Where the bundle came from and what it ran on, so differences in data aren't read as server bugs
			auto entry = result.ToJson();
			entry["bundle"] = path;
			entry["origin"] = bundle.meta.value("origin", "");
			entry["recordedOn"] = bundle.meta.value("server", json::object());
			entry["replayedOn"] = { {"version", PROJECT_VERSION} };
			entry["sandbox"] = stack->Dir().string();
			report.push_back(entry);
			PrintResult(fs::path(path).filename().string(), result);
			// Written after every bundle, so a long run's results survive it being stopped
			if (!Arg(args, "--report").empty()) std::ofstream(Arg(args, "--report")) << report.dump(1);
			std::cout.flush();
			const bool failed = !result.stoppedAt.empty();
			if (failed) failures++;
			if (options.keep || (failed && Flag(args, "--keep-on-failure"))) {
				stack->Keep();
				std::cout << "   sandbox kept: " << stack->Dir() << "\n";
			}
		}
		const auto reportPath = Arg(args, "--report");
		if (!reportPath.empty()) std::ofstream(reportPath) << report.dump(1);
		return failures ? 2 : 0;
	}

	int ReplayTarget(const std::vector<std::string>& args) {
		if (!Flag(args, "--i-know-this-is-not-a-sandbox")) {
			std::cerr << "replay-target sends a recording's packets to a running server as the account you give. Never point it at a live\n"
				"server. If this really is a test server, add --i-know-this-is-not-a-sandbox. Otherwise use: replay (a fresh sandbox).\n";
			return 1;
		}
		const auto bundles = Positional(args, 2);
		if (bundles.size() != 1) {
			Usage();
			return 1;
		}
		CaptureBundle::Bundle bundle;
		if (!LoadBundle(bundles[0], bundle)) return 1;
		Replayer::Options replay;
		replay.host = Arg(args, "--host", "127.0.0.1");
		replay.authPort = static_cast<uint16_t>(std::stoi(Arg(args, "--auth-port", "1001")));
		replay.username = Arg(args, "--username");
		replay.password = Arg(args, "--password");
		replay.speed = std::stod(Arg(args, "--speed", "4"));
		std::cout << "!! Replaying against " << replay.host << ":" << replay.authPort << ", which is NOT a sandbox.\n";
		const auto result = Replayer::Replay(bundle, replay);
		PrintResult(bundles[0], result);
		const auto reportPath = Arg(args, "--report");
		if (!reportPath.empty()) std::ofstream(reportPath) << result.ToJson().dump(1);
		return result.stoppedAt.empty() ? 0 : 2;
	}
}

int main(int argc, char** argv) {
	// Game messages are read with the game's own message structs
	PacketDecoder::SetGameMessageDecoder(GameMessageDecoder::Decode);
	const std::vector<std::string> args(argv, argv + argc);
	if (args.size() < 2) {
		Usage();
		return 1;
	}
	const auto& command = args[1];
	if (command == "info" && args.size() >= 3) return Info(args[2]);
	if (command == "decode" && args.size() >= 3) return Decode(args[2], Flag(args, "--fields"), std::stoul(Arg(args, "--limit", "1000000")), Arg(args, "--cdserver", ""));
	if (command == "anonymise" && args.size() >= 4) return Anonymise(args[2], args[3]);
	if (command == "import-live" && args.size() >= 4) return ImportLive(args[2], args[3]);
	if (command == "replay") return Replay(args);
	if (command == "replay-target") return ReplayTarget(args);
	if (command == "sandbox-setup" && args.size() >= 7) return Sandbox::SetupCommand(args[2], args[3], args[4], args[5] == "1", args[6]);
	Usage();
	return 1;
}
