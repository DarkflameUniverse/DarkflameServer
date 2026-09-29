#include "UgcToolbox.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <fstream>
#include <sstream>
#include <thread>

#include "UgcKeys.h"

#if !defined(_WIN32)
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
extern char** environ;
#endif

namespace UgcToolbox {
	namespace {
		bool IsFile(const std::filesystem::path& path) {
			std::error_code ec;
			return std::filesystem::is_regular_file(path, ec);
		}

		bool IsDirectory(const std::filesystem::path& path) {
			std::error_code ec;
			return std::filesystem::is_directory(path, ec);
		}

		std::string Quote(const std::filesystem::path& path) { return "\"" + path.string() + "\""; }

		double Since(std::chrono::steady_clock::time_point start) {
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		}

		// How long Blender may take to be ready: the first start unpacks the brick database and links the primitives
		constexpr auto START_TIMEOUT = std::chrono::seconds(180);
		// How often a waiting worker looks at Blender (the CPU budget's pace)
		constexpr int PACE_MS = 100;
		// After this many failed starts in a row, no new start is tried for RETRY_AFTER (each make falls back meanwhile)
		constexpr uint32_t MAX_FAILED_STARTS = 3;
		constexpr auto RETRY_AFTER = std::chrono::minutes(10);
	}

	std::string Problem(const Config& config) {
#if defined(_WIN32)
		(void)config;
		return "toolbox-blender runs Blender as a worker on Linux (and other POSIX systems) only for now";
#else
		if (config.blender.empty()) return "toolbox_blender (the Blender executable) is not set";
		if (!IsFile(config.blender) || access(config.blender.c_str(), X_OK) != 0) return "toolbox_blender " + Quote(config.blender) + " is not an executable file";
		if (config.standalone.empty()) return "toolbox_standalone_dir (LU-Toolbox-Standalone) is not set";
		if (!IsFile(config.standalone / "lu_batch_driver.py")) return "toolbox_standalone_dir " + Quote(config.standalone) + " has no lu_batch_driver.py";
		if (!config.scripts.empty()) {
			for (const auto* addon : { "lu_toolbox", "io_scene_niftools" }) {
				if (!IsFile(config.scripts / "addons" / addon / "__init__.py")) return "toolbox_scripts_dir " + Quote(config.scripts) + " has no addons/" + addon;
			}
		}
		if (!IsFile(config.worker)) return "the worker script " + Quote(config.worker) + " is missing (the build copies it next to the servers)";
		if (std::find(std::begin(DEVICES), std::end(DEVICES), config.device) == std::end(DEVICES)) return "toolbox_device \"" + config.device + "\" isn't cpu, cuda, optix, hip or auto";
		if (config.brickdb.empty()) return "toolbox_brickdb_dir is not set";
		if (!IsDirectory(config.brickdb / "Assemblies") && !IsFile(config.res / "brickdb.zip")) return "the client's res folder " + Quote(config.res) + " has no brickdb.zip for LU Toolbox's brick folder";
		if (config.work.empty()) return "toolbox_work_dir is not set";
		return {};
#endif
	}

	std::string_view Resolve(std::string_view wanted, const std::string& problem, std::string& why) {
		why.clear();
		if (wanted != UgcProcessOptions::TOOLBOX_BLENDER) return UgcProcessOptions::NATIVE;
		if (!problem.empty()) {
			why = problem;
			return UgcProcessOptions::NATIVE;
		}
		return UgcProcessOptions::TOOLBOX_BLENDER;
	}

#if defined(_WIN32)
	struct Worker::Process {};
	Worker::~Worker() = default;
	void Worker::Configure(const Config& config) { std::lock_guard lock(m_ConfigMutex); m_Config = config; }
	Config Worker::GetConfig() const { std::lock_guard lock(m_ConfigMutex); return m_Config; }
	Result Worker::Make(uint64_t, const std::string&, const std::vector<uint32_t>&, const std::function<void(double, const Pause&)>&) {
		return { false, Problem(GetConfig()) };
	}
	void Worker::Stop() {}
	bool Worker::StartLocked(std::string& error) { error = Problem(m_Config); return false; }
	void Worker::StopLocked(bool) {}
	std::string Worker::LogTail() const { return {}; }
	void Worker::PublishStatus() {}
	nlohmann::json Worker::Status() const { return { { "running", false }, { "problem", Problem(GetConfig()) } }; }
#else
	struct Worker::Process {
		pid_t pid{ -1 };
		int toWorker{ -1 };   // its stdin
		int fromWorker{ -1 }; // its replies (its original stdout)
		UgcToolboxProtocol::LineReader reader;
		std::filesystem::path log;
		double lastCpu{};
		bool paused{};
		int status{};         // its exit status once reaped
		bool exited{};

		// Its CPU time (all threads) in seconds; 0 where it can't be read
		double CpuSeconds() const {
#if defined(__linux__)
			std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
			std::string text;
			std::getline(stat, text);
			// The command is in parentheses and may hold spaces; utime and stime are fields 14 and 15
			const auto close = text.rfind(')');
			if (close == std::string::npos) return 0.0;
			std::istringstream fields(text.substr(close + 2));
			std::string field;
			uint64_t utime = 0, stime = 0;
			for (int i = 3; i <= 15 && fields >> field; i++) {
				if (i == 14) utime = std::strtoull(field.c_str(), nullptr, 10);
				if (i == 15) stime = std::strtoull(field.c_str(), nullptr, 10);
			}
			const auto ticks = sysconf(_SC_CLK_TCK);
			return ticks > 0 ? static_cast<double>(utime + stime) / static_cast<double>(ticks) : 0.0;
#else
			return 0.0;
#endif
		}

		bool Alive() {
			if (exited) return false;
			const auto reaped = waitpid(pid, &status, WNOHANG);
			if (reaped == pid) exited = true;
			return !exited;
		}

		std::string ExitReason() const {
			if (!exited) return "it stopped answering";
			if (WIFSIGNALED(status)) return "it was killed by signal " + std::to_string(WTERMSIG(status));
			if (WIFEXITED(status)) return "it exited with code " + std::to_string(WEXITSTATUS(status));
			return "it stopped";
		}

		void Pause(bool pause) {
			if (pause == paused || exited) return;
			kill(pid, pause ? SIGSTOP : SIGCONT);
			paused = pause;
		}

		bool Send(const nlohmann::json& message) {
			const auto line = UgcToolboxProtocol::Frame(message);
			size_t sent = 0;
			while (sent < line.size()) {
				// A socket, so a worker that is gone gives an error instead of SIGPIPE
				const auto n = send(toWorker, line.data() + sent, line.size() - sent, MSG_NOSIGNAL);
				if (n < 0 && errno == EINTR) continue;
				if (n <= 0) return false;
				sent += static_cast<size_t>(n);
			}
			return true;
		}

		/**
		 * The next message, waiting at most until `deadline`; `tick` is called every PACE_MS meanwhile. nullopt when the
		 * time is up, the worker is gone (`gone` set) or closed its side.
		 */
		std::optional<nlohmann::json> Receive(std::chrono::steady_clock::time_point deadline, bool& gone, const std::function<void()>& tick) {
			gone = false;
			while (true) {
				while (const auto line = reader.Next()) {
					if (auto message = UgcToolboxProtocol::Parse(*line)) return message;
				}
				if (std::chrono::steady_clock::now() >= deadline) return std::nullopt;
				pollfd fd{ fromWorker, POLLIN, 0 };
				const auto ready = poll(&fd, 1, PACE_MS);
				if (ready > 0) {
					char buffer[4096];
					const auto n = read(fromWorker, buffer, sizeof(buffer));
					if (n > 0) {
						reader.Feed(std::string_view(buffer, static_cast<size_t>(n)));
						continue;
					}
					if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
						// Its side closed: it exited (or is about to)
						for (int i = 0; i < 20 && Alive(); i++) std::this_thread::sleep_for(std::chrono::milliseconds(50));
						gone = true;
						return std::nullopt;
					}
				}
				if (!Alive()) {
					gone = true;
					return std::nullopt;
				}
				if (tick) tick();
			}
		}

		void Close() {
			if (toWorker >= 0) close(toWorker);
			if (fromWorker >= 0) close(fromWorker);
			toWorker = fromWorker = -1;
		}
	};

	Worker::~Worker() {
		std::lock_guard lock(m_JobMutex);
		StopLocked(false);
	}

	void Worker::Configure(const Config& config) {
		std::lock_guard lock(m_ConfigMutex);
		m_Config = config;
	}

	Config Worker::GetConfig() const {
		std::lock_guard lock(m_ConfigMutex);
		return m_Config;
	}

	std::string Worker::LogTail() const {
		if (!m_Process) return {};
		std::ifstream file(m_Process->log, std::ios::binary);
		if (!file) return {};
		file.seekg(0, std::ios::end);
		const auto size = static_cast<std::streamoff>(file.tellg());
		file.seekg(std::max<std::streamoff>(0, size - 4096));
		std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		// The last few lines with something in them
		std::deque<std::string> lines;
		std::istringstream stream(text);
		std::string line;
		while (std::getline(stream, line)) {
			if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
			lines.push_back(line);
			if (lines.size() > 6) lines.pop_front();
		}
		std::string out;
		for (const auto& kept : lines) out += (out.empty() ? "" : " | ") + kept;
		return out.substr(0, 600);
	}

	bool Worker::StartLocked(std::string& error) {
		m_Running = GetConfig();
		const auto& config = m_Running;
		if (error = Problem(config); !error.empty()) return false;
		std::error_code ec;
		std::filesystem::create_directories(config.work, ec);
		if (ec) {
			error = "can't make toolbox_work_dir " + Quote(config.work) + ": " + ec.message();
			return false;
		}
		auto process = std::make_unique<Process>();
		process->log = config.work / "blender.log";
		int input[2]{ -1, -1 }, output[2]{ -1, -1 };
		if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, input) != 0 || socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, output) != 0) {
			error = std::string("socketpair: ") + std::strerror(errno);
			for (const int fd : { input[0], input[1], output[0], output[1] }) if (fd >= 0) close(fd);
			return false;
		}
		const int logFd = open(process->log.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

		std::vector<std::string> args{ config.blender.string(), "-b", "--factory-startup", "-t", std::to_string(std::max<uint32_t>(config.threads, 1)),
			"--python", config.worker.string(), "--", "--standalone", config.standalone.string(), "--brickdb", config.brickdb.string(),
			"--res", config.res.string(), "--device", config.device };
		std::vector<char*> argv;
		for (auto& arg : args) argv.push_back(arg.data());
		argv.push_back(nullptr);
		// The environment, with BLENDER_USER_SCRIPTS when the add-ons are in a folder of their own (made before fork: the
		// child may only make async-signal-safe calls)
		std::vector<std::string> environment;
		for (char** entry = environ; entry && *entry; entry++) {
			if (!config.scripts.empty() && std::strncmp(*entry, "BLENDER_USER_SCRIPTS=", 21) == 0) continue;
			environment.emplace_back(*entry);
		}
		if (!config.scripts.empty()) environment.push_back("BLENDER_USER_SCRIPTS=" + config.scripts.string());
		std::vector<char*> envp;
		for (auto& entry : environment) envp.push_back(entry.data());
		envp.push_back(nullptr);
		const int nice = config.nice;
		rlimit files{};
		const int maxFd = getrlimit(RLIMIT_NOFILE, &files) == 0 && files.rlim_cur != RLIM_INFINITY ? static_cast<int>(std::min<rlim_t>(files.rlim_cur, 65536)) : 4096;

		const pid_t pid = fork();
		if (pid == 0) {
			// The child: only async-signal-safe calls until exec
			dup2(input[1], 0);
			dup2(output[1], 1);
			if (logFd >= 0) dup2(logFd, 2);
			// None of the server's sockets and files (its listening port above all) go to Blender
			for (int fd = 3; fd < maxFd; fd++) close(fd);
#if defined(__linux__)
			// Blender goes when the UGC server goes, however it goes
			prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
			if (nice > 0) setpriority(PRIO_PROCESS, 0, nice);
			execve(argv[0], argv.data(), envp.data());
			_exit(127);
		}
		close(input[1]);
		close(output[1]);
		if (logFd >= 0) close(logFd);
		if (pid < 0) {
			error = std::string("fork: ") + std::strerror(errno);
			close(input[0]);
			close(output[0]);
			return false;
		}
		process->pid = pid;
		process->toWorker = input[0];
		process->fromWorker = output[0];
		m_Process = process.release();
		m_Jobs = 0;
		m_Starts++;

		bool gone = false;
		const auto deadline = std::chrono::steady_clock::now() + START_TIMEOUT;
		while (true) {
			const auto message = m_Process->Receive(deadline, gone, {});
			if (!message) {
				error = gone ? "Blender stopped while starting (" + m_Process->ExitReason() + ")" : "Blender wasn't ready in " + std::to_string(START_TIMEOUT.count()) + " s";
				break;
			}
			const auto type = UgcToolboxProtocol::TypeOf(*message);
			if (type == UgcToolboxProtocol::eType::READY) {
				m_Versions = "Blender " + message->value("blender", std::string("?")) + ", LU Toolbox " + message->value("toolbox", std::string("?")) +
					", niftools " + message->value("niftools", std::string("?")) + ", " + message->value("device", std::string("?"));
				m_FailedStarts = 0;
				return true;
			}
			if (type == UgcToolboxProtocol::eType::FAILED) {
				error = "the worker couldn't start: " + message->value("error", std::string("no reason"));
				break;
			}
		}
		const auto tail = LogTail();
		if (!tail.empty()) error += " (Blender's log: " + tail + ")";
		StopLocked(true);
		return false;
	}

	void Worker::StopLocked(bool kill) {
		if (!m_Process) return;
		auto& process = *m_Process;
		process.Pause(false);
		if (!kill && process.Alive()) {
			process.Send({ { "cmd", "quit" } });
			for (int i = 0; i < 100 && process.Alive(); i++) std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		if (process.Alive()) {
			::kill(process.pid, SIGKILL);
			waitpid(process.pid, &process.status, 0);
			process.exited = true;
		}
		process.Close();
		delete m_Process;
		m_Process = nullptr;
		m_Jobs = 0;
	}

	void Worker::Stop() {
		std::lock_guard lock(m_JobMutex);
		StopLocked(false);
		PublishStatus();
	}

	void Worker::PublishStatus() {
		nlohmann::json status{ { "running", m_Process != nullptr }, { "pid", m_Process ? m_Process->pid : 0 }, { "models", m_Jobs }, { "starts", m_Starts },
			{ "versions", m_Versions }, { "lastError", m_LastError } };
		std::lock_guard lock(m_StatusMutex);
		m_Status = std::move(status);
	}

	nlohmann::json Worker::Status() const {
		const auto config = GetConfig();
		std::lock_guard lock(m_StatusMutex);
		auto status = m_Status;
		status["problem"] = Problem(config);
		status["device"] = config.device;
		status["threads"] = config.threads;
		return status;
	}

	Result Worker::Make(uint64_t id, const std::string& lxfml, const std::vector<uint32_t>& lods, const std::function<void(double, const Pause&)>& pace) {
		Result result;
		const auto asked = std::chrono::steady_clock::now();
		std::unique_lock lock(m_JobMutex);
		// Given up while waiting for Blender (the server stopping): before starting one
		if (pace) pace(0.0, [](bool) {});
		const auto config = GetConfig();
		// New settings, or it made its share of models: a new Blender
		if (m_Process && (!(config == m_Running) || (m_Running.restartAfterJobs > 0 && m_Jobs >= m_Running.restartAfterJobs) || !m_Process->Alive())) StopLocked(false);
		if (!m_Process) {
			if (m_FailedStarts >= MAX_FAILED_STARTS && std::chrono::steady_clock::now() < m_RetryAfter) {
				result.error = "Blender failed to start " + std::to_string(m_FailedStarts) + " times; trying again later (" + m_LastError + ")";
				return result;
			}
			std::string error;
			result.started = true;
			if (!StartLocked(error)) {
				m_FailedStarts++;
				if (m_FailedStarts >= MAX_FAILED_STARTS) m_RetryAfter = std::chrono::steady_clock::now() + RETRY_AFTER;
				m_LastError = error;
				result.error = "Blender couldn't be started: " + error;
				PublishStatus();
				return result;
			}
			PublishStatus();
		}
		result.waitedMs = Since(asked);
		result.blender = m_Versions;
		auto& process = *m_Process;

		const auto base = m_Running.work / ("model-" + std::to_string(id));
		const auto input = base.string() + ".lxfml", output = base.string() + ".nif";
		std::error_code ec;
		std::filesystem::remove(output, ec);
		{
			std::ofstream file(input, std::ios::binary);
			file.write(lxfml.data(), static_cast<std::streamsize>(lxfml.size()));
			if (!file) {
				result.error = "can't write " + input;
				return result;
			}
		}
		const auto cleanUp = [&] {
			std::filesystem::remove(input, ec);
			std::filesystem::remove(output, ec);
		};

		const auto requestId = m_NextId++;
		if (!process.Send(UgcToolboxProtocol::MakeRequest(requestId, input, output, lods))) {
			result.error = "Blender isn't taking work (" + process.ExitReason() + ")";
			StopLocked(true);
			cleanUp();
			PublishStatus();
			return result;
		}
		const double cpuStart = process.CpuSeconds();
		process.lastCpu = cpuStart;
		const Pause pause = [&process](bool paused) { process.Pause(paused); };
		const auto tick = [&] {
			if (!pace) return;
			const double cpu = process.CpuSeconds();
			const double used = std::max(0.0, cpu - process.lastCpu);
			process.lastCpu = cpu;
			pace(used, pause);
		};
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(std::max<uint32_t>(m_Running.timeoutSeconds, 1));
		try {
			bool gone = false;
			while (true) {
				const auto message = process.Receive(deadline, gone, tick);
				if (!message) {
					result.blenderCpuSeconds = std::max(0.0, process.lastCpu - cpuStart);
					const auto tail = LogTail();
					if (gone) {
						result.error = "Blender stopped while making the model (" + process.ExitReason() + ")";
					} else {
						result.error = "LU Toolbox took longer than toolbox_timeout_seconds (" + std::to_string(m_Running.timeoutSeconds) + " s)";
					}
					if (!tail.empty()) result.error += "; Blender's log: " + tail;
					StopLocked(true);
					break;
				}
				const auto done = UgcToolboxProtocol::ParseDone(*message);
				if (!done || done->id != requestId) continue;
				process.Pause(false);
				result.blenderCpuSeconds = std::max(0.0, process.CpuSeconds() - cpuStart);
				m_Jobs++;
				result.ms = nlohmann::json::object();
				for (const auto& [name, ms] : done->ms) result.ms[name] = ms;
				if (!done->ok) {
					result.error = "LU Toolbox failed: " + done->error;
					break;
				}
				std::ifstream file(output, std::ios::binary);
				result.nif.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
				if (result.nif.empty()) {
					result.error = "LU Toolbox wrote no .nif";
					break;
				}
				result.ok = true;
				break;
			}
		} catch (...) {
			// Given up (the server stopping): Blender stops too, mid-model
			StopLocked(true);
			cleanUp();
			PublishStatus();
			throw;
		}
		if (!result.ok) m_LastError = result.error;
		cleanUp();
		PublishStatus();
		return result;
	}
#endif
}
