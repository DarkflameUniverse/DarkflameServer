#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <sys/types.h>

#include "json.hpp"

/**
 * A replay sandbox (docs/CaptureReplay.md): a throwaway server stack for one replay, in a folder of its own. The
 * server binaries are copied in (they read their settings and database from their own folder), the settings point
 * at a fresh SQLite database inside that folder, ports that don't collide with a normal server, and
 * replay_sandbox=1, with which every server refuses to start on any other database. The client files are shared
 * read-only; CDServer.sqlite is copied. The folder is deleted afterwards unless it is kept.
 */
namespace Sandbox {
	struct Options {
		std::filesystem::path serverDir;      // built servers (MasterServer, AuthServer, ChatServer, WorldServer, migrations, ...)
		std::filesystem::path root;           // sandboxes are made in here
		std::filesystem::path clientLocation; // the game client's files (read only)
		std::filesystem::path cdServer;       // CDServer.sqlite to copy
		std::filesystem::path liveDatabase;   // the live SQLite database, which a sandbox must never be (optional)
		uint16_t basePort{ 41000 };
		bool keep{};
	};

	class Stack {
	public:
		static std::unique_ptr<Stack> Create(const Options& options, std::string& error);
		~Stack();

		// Makes the database (migrations), the replay's account and, from the bundle's setup section, its characters.
		// `ids`: symbol -> {placeholder, id} of the characters made.
		bool Setup(const std::filesystem::path& bundle, const std::string& username, const std::string& password, bool characters,
			nlohmann::json& ids, std::string& error);

		// Starts master (which starts auth, chat and the character select world) and waits until auth answers
		bool Start(std::string& error);
		void Stop();

		void Keep() { m_Keep = true; }
		uint16_t AuthPort() const { return m_Options.basePort + 10; }
		const std::filesystem::path& Dir() const { return m_Dir; }

	private:
		Options m_Options;
		std::filesystem::path m_Dir;
		pid_t m_Master{};
		bool m_Keep{};
	};

	// The sandbox-setup command, run by Stack::Setup inside the sandbox folder
	int SetupCommand(const std::filesystem::path& bundle, const std::string& username, const std::string& password, bool characters,
		const std::filesystem::path& out);
}
