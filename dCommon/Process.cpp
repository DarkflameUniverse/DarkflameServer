#include "Process.h"

#include <algorithm>
#include <cerrno>

#ifdef _WIN32
#include <process.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace Process {
	bool HasShellMetacharacters(std::string_view text) {
		return std::ranges::any_of(text, [](char c) {
			switch (c) {
			case '"': case '\'': case '`': case '$': case ';': case '|': case '&': case '<': case '>':
			case '\n': case '\r': case '\0':
				return true;
			default:
				return false;
			}
		});
	}

#ifdef _WIN32
	int Run(const std::vector<std::string>& arguments, const std::string& stdoutFile, const std::string& stderrFile) {
		if (arguments.empty()) return -1;
		// _spawnvp takes an argument vector; cmd.exe is not involved. Output redirection is not supported here.
		std::vector<std::string> quoted;
		for (const auto& argument : arguments) {
			std::string out = "\"";
			for (const char c : argument) if (c != '"') out += c;
			quoted.push_back(out + "\"");
		}
		std::vector<const char*> argv;
		for (const auto& argument : quoted) argv.push_back(argument.c_str());
		argv.push_back(nullptr);
		const auto status = _spawnvp(_P_WAIT, arguments[0].c_str(), argv.data());
		return status < 0 ? -1 : static_cast<int>(status);
	}
#else
	int Run(const std::vector<std::string>& arguments, const std::string& stdoutFile, const std::string& stderrFile) {
		if (arguments.empty()) return -1;
		std::vector<char*> argv;
		for (const auto& argument : arguments) argv.push_back(const_cast<char*>(argument.c_str()));
		argv.push_back(nullptr);

		posix_spawn_file_actions_t actions;
		if (posix_spawn_file_actions_init(&actions) != 0) return -1;
		const auto redirect = [&actions](int fd, const std::string& file) {
			return posix_spawn_file_actions_addopen(&actions, fd, file.empty() ? "/dev/null" : file.c_str(),
				O_WRONLY | O_CREAT | O_TRUNC, 0600);
		};
		pid_t pid = 0;
		int status = -1;
		if (posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0) == 0 &&
			redirect(1, stdoutFile) == 0 && redirect(2, stderrFile) == 0 &&
			posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ) == 0) {
			int waitStatus = 0;
			while (waitpid(pid, &waitStatus, 0) < 0) {
				if (errno != EINTR) { waitStatus = -1; break; }
			}
			// posix_spawnp can report a missing program as exit code 127 from the child
			status = waitStatus != -1 && WIFEXITED(waitStatus) ? WEXITSTATUS(waitStatus) : -1;
		}
		posix_spawn_file_actions_destroy(&actions);
		return status;
	}
#endif
}
