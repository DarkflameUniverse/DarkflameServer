#include "Sandbox.h"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <sstream>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "BinaryPathFinder.h"
#include "CaptureBundle.h"
#include "Database.h"
#include "dConfig.h"
#include "eGameMasterLevel.h"
#include "eObjectBits.h"
#include "FakeClient.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "MigrationRunner.h"
#include "bcrypt/BCrypt.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
	// The running stack's process group: stopped too if the tool is interrupted or crashes
	volatile sig_atomic_t g_Running = 0;

	void StopOnSignal(int signal) {
		if (g_Running > 0) kill(-g_Running, SIGKILL);
		std::signal(signal, SIG_DFL);
		std::raise(signal);
	}

	/**
	 * Settings every sandbox gets over the server's own files. The first value of a key counts, so each key's line
	 * is replaced where it is, and the ones the file doesn't have are added.
	 */
	void Append(const fs::path& file, const std::string& lines) {
		std::map<std::string, std::string> settings;
		std::istringstream wanted(lines);
		for (std::string line; std::getline(wanted, line);) {
			const auto equals = line.find('=');
			if (equals != std::string::npos) settings[line.substr(0, equals)] = line;
		}
		std::ifstream in(file);
		std::string out;
		for (std::string line; std::getline(in, line);) {
			const auto equals = line.find('=');
			const auto key = equals == std::string::npos || line.starts_with("#") ? "" : line.substr(0, equals);
			const auto it = settings.find(key);
			if (it != settings.end()) {
				out += it->second + "\n";
				settings.erase(it);
			} else {
				out += line + "\n";
			}
		}
		in.close();
		out += "\n# Replay sandbox (docs/CaptureReplay.md)\n";
		for (const auto& [key, line] : settings) out += line + "\n";
		std::ofstream(file, std::ios::trunc) << out;
	}

	pid_t Launch(const fs::path& dir, const fs::path& program, const fs::path& output, const std::vector<std::string>& args = {}) {
		const pid_t pid = fork();
		if (pid != 0) return pid;
		// The child: its own process group, so the whole stack (master and what it starts) stops together
		setpgid(0, 0);
		// Settings can come from the environment too (dConfig): not in a sandbox
		for (const auto* key : { "REPLAY_SANDBOX", "DATABASE_TYPE", "SQLITE_DATABASE_PATH", "MASTER_SERVER_PORT", "WORLD_PORT_START", "AUTH_SERVER_PORT",
			"CHAT_SERVER_PORT", "MASTER_IP", "EXTERNAL_IP", "CLIENT_LOCATION", "MYSQL_HOST", "MYSQL_DATABASE", "DLU_CONFIG_DIR" }) unsetenv(key);
		if (chdir(dir.c_str()) != 0) _exit(127);
		const int fd = open(output.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
		if (fd >= 0) {
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			close(fd);
		}
		std::vector<char*> argv{ const_cast<char*>(program.c_str()) };
		for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
		argv.push_back(nullptr);
		execv(program.c_str(), argv.data());
		_exit(127);
	}
}

namespace Sandbox {
	std::unique_ptr<Stack> Stack::Create(const Options& options, std::string& error) {
		auto stack = std::unique_ptr<Stack>(new Stack());
		stack->m_Options = options;
		stack->m_Keep = options.keep;
		std::error_code ec;
		const auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count() / 1000000) + "-" + std::to_string(getpid());
		stack->m_Dir = fs::absolute(options.root / ("sandbox-" + stamp));
		const auto& dir = stack->m_Dir;
		if (!fs::create_directories(dir / "resServer", ec) || !fs::create_directories(dir / "logs", ec)) {
			error = "Can't make " + dir.string();
			return nullptr;
		}
		// Binaries are copied: they find their settings and database next to themselves
		for (const auto* name : { "MasterServer", "AuthServer", "ChatServer", "WorldServer" }) {
			fs::copy_file(options.serverDir / name, dir / name, ec);
			if (ec) {
				error = "Can't copy " + (options.serverDir / name).string() + ": " + ec.message();
				return nullptr;
			}
		}
		fs::copy_file(BinaryPathFinder::GetBinaryDir() / "CaptureTool", dir / "CaptureTool", ec);
		for (const auto* name : { "migrations", "navmeshes", "vanity", "blocklist.dcf", "libmariadbcpp.so" }) {
			if (fs::exists(options.serverDir / name)) fs::create_symlink(fs::absolute(options.serverDir / name), dir / name, ec);
		}
		for (const auto* name : { "sharedconfig.ini", "masterconfig.ini", "authconfig.ini", "chatconfig.ini", "worldconfig.ini" }) {
			fs::copy_file(options.serverDir / name, dir / name, ec);
		}
		fs::copy_file(options.cdServer, dir / "resServer" / "CDServer.sqlite", ec);
		if (ec) {
			error = "Can't copy CDServer.sqlite from " + options.cdServer.string() + ": " + ec.message();
			return nullptr;
		}

		const auto port = [&](int offset) { return std::to_string(options.basePort + offset); };
		Append(dir / "sharedconfig.ini",
			"replay_sandbox=1\n"
			"database_type=sqlite\n"
			"sqlite_database_path=resServer/sandbox.sqlite\n"
			"replay_live_sqlite_path=" + options.liveDatabase.string() + "\n"
			"client_location=" + options.clientLocation.string() + "\n"
			"external_ip=localhost\n"
			"master_ip=localhost\n"
			"bind_ip=127.0.0.1\n"
			"master_server_port=" + port(0) + "\n"
			"chat_server_port=" + port(20) + "\n"
			"skip_account_creation=1\n"
			"log_to_console=1\n");
		Append(dir / "masterconfig.ini",
			"master_server_port=" + port(0) + "\n"
			"world_port_start=" + port(100) + "\n"
			"prestart_servers=1\n"
			"enable_dashboard=0\n"
			"enable_ugc_server=0\n");
		Append(dir / "authconfig.ini", "auth_server_port=" + port(10) + "\ndont_use_keys=1\n");
		Append(dir / "chatconfig.ini", "port=" + port(20) + "\nweb_server_enabled=0\n");
		Append(dir / "worldconfig.ini", "check_fdb=0\n");

		// Read the settings back as the servers will (the first value of a key counts): never start a stack that could
		// reach a normal server's ports or database
		const auto first = [&](const char* file, const std::string& key) {
			std::ifstream in(dir / file);
			for (std::string line; std::getline(in, line);) if (line.starts_with(key + "=")) return line.substr(key.size() + 1);
			return std::string();
		};
		if (first("sharedconfig.ini", "replay_sandbox") != "1" || first("sharedconfig.ini", "sqlite_database_path") != "resServer/sandbox.sqlite" ||
			first("sharedconfig.ini", "master_server_port") != port(0) || first("masterconfig.ini", "master_server_port") != port(0) ||
			first("masterconfig.ini", "world_port_start") != port(100) || first("authconfig.ini", "auth_server_port") != port(10)) {
			error = "The sandbox's settings didn't take (see " + dir.string() + ")";
			return nullptr;
		}
		return stack;
	}

	Stack::~Stack() {
		Stop();
		std::error_code ec;
		if (!m_Keep && !m_Dir.empty()) fs::remove_all(m_Dir, ec);
	}

	bool Stack::Setup(const fs::path& bundle, const std::string& username, const std::string& password, bool characters, json& ids, std::string& error) {
		const auto out = m_Dir / "setup.json";
		const pid_t pid = Launch(m_Dir, m_Dir / "CaptureTool", m_Dir / "logs" / "setup.out",
			{ "sandbox-setup", fs::absolute(bundle).string(), username, password, characters ? "1" : "0", out.string() });
		int status = 0;
		waitpid(pid, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			error = "Setting up the sandbox failed (see " + (m_Dir / "logs" / "setup.out").string() + ")";
			return false;
		}
		std::ifstream file(out);
		ids = json::parse(file, nullptr, false);
		if (ids.is_discarded()) {
			error = "The sandbox setup wrote no result";
			return false;
		}
		return true;
	}

	bool Stack::Start(std::string& error) {
		for (const int signal : { SIGINT, SIGTERM, SIGSEGV, SIGABRT }) std::signal(signal, StopOnSignal);
		m_Master = Launch(m_Dir, m_Dir / "MasterServer", m_Dir / "logs" / "master.out");
		g_Running = m_Master;
		// Auth answering is the sign the stack is up (master starts it after chat and the character select world)
		const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(120);
		while (std::chrono::steady_clock::now() < until) {
			int status = 0;
			if (waitpid(m_Master, &status, WNOHANG) == m_Master) {
				m_Master = 0;
				error = "The sandbox's master server stopped (see " + (m_Dir / "logs").string() + ")";
				return false;
			}
			FakeClient probe;
			if (probe.Connect("127.0.0.1", AuthPort(), std::chrono::milliseconds(500))) {
				probe.Disconnect();
				// The character select world takes a moment longer
				std::this_thread::sleep_for(std::chrono::seconds(3));
				return true;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		error = "The sandbox didn't start within 2 minutes (see " + (m_Dir / "logs").string() + ")";
		return false;
	}

	void Stack::Stop() {
		if (m_Master <= 0) return;
		kill(-m_Master, SIGTERM);
		const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		int status = 0;
		while (waitpid(m_Master, &status, WNOHANG) == 0 && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(std::chrono::milliseconds(100));
		kill(-m_Master, SIGKILL);
		waitpid(m_Master, &status, 0);
		m_Master = 0;
		g_Running = 0;
	}

	int SetupCommand(const fs::path& bundlePath, const std::string& username, const std::string& password, bool characters, const fs::path& out) {
		Game::logger = new Logger((BinaryPathFinder::GetBinaryDir() / "logs" / "setup.log").string(), true, false);
		Game::config = new dConfig("masterconfig.ini");
		// Only ever in a sandbox: Database::Connect refuses any database but the sandbox's own too
		if (Game::config->GetValue("replay_sandbox") != "1") {
			LOG("sandbox-setup only runs in a replay sandbox (replay_sandbox=1)");
			return 2;
		}
		CaptureBundle::Bundle bundle;
		std::string error;
		if (!CaptureBundle::Load(bundlePath, bundle, error)) {
			LOG("%s", error.c_str());
			return 3;
		}
		try {
			Database::Connect();
			MigrationRunner::RunMigrations();
			char salt[BCRYPT_HASHSIZE], hash[BCRYPT_HASHSIZE];
			bcrypt_gensalt(4, salt);
			bcrypt_hashpw(password.c_str(), salt, hash);
			Database::Get()->InsertNewAccount(username, hash, eGameMasterLevel::CIVILIAN);
			const auto account = Database::Get()->GetAccountInfo(username);
			if (!account) {
				LOG("Couldn't make the replay account");
				return 4;
			}
			json ids = json::object();
			if (characters && bundle.meta.contains("setup")) {
				auto range = Database::Get()->GetPersistentIdRange();
				for (const auto& character : bundle.meta["setup"].value("characters", json::array())) {
					LWOOBJID id = static_cast<LWOOBJID>(range.minID++);
					GeneralUtils::SetBit(id, eObjectBits::CHARACTER);
					auto name = character.value("name", character.value("symbol", std::string("replay")));
					for (int n = 2; Database::Get()->IsNameInUse(name); n++) name = character.value("name", std::string("replay")) + std::to_string(n);
					ICharInfo::Info info;
					info.name = name;
					info.id = id;
					info.accountId = account->id;
					Database::Get()->InsertNewCharacter(info);
					Database::Get()->InsertCharacterXml(id, character.value("xml", std::string()));
					ids[character.value("symbol", std::string())] = { {"placeholder", character.value("placeholder", json()).is_string() ? character["placeholder"] : json(std::to_string(character.value("placeholder", 0LL)))},
						{"id", std::to_string(id)}, {"name", name} };
					LOG("Made character %s (%llu) for %s", name.c_str(), id, character.value("symbol", std::string()).c_str());
				}
			}
			std::ofstream(out) << ids.dump(1);
		} catch (const std::exception& e) {
			LOG("Sandbox setup failed: %s", e.what());
			return 5;
		}
		Game::logger->Flush();
		return 0;
	}
}
