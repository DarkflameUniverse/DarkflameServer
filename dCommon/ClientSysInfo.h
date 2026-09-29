#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

/**
 * The system description the client sends in its login request, as reported by the client. It comes from old Windows
 * calls and describes what Windows (or Wine) tells a 32-bit program without a compatibility manifest, which is not
 * necessarily the player's real hardware. docs/Dashboard.md (Client system info) says what each field means.
 */
namespace ClientSysInfo {
	/**
	 * The login request's memoryStats text, split into numbers. The client writes these parts one after another with no
	 * separator (numbers are right-aligned to 7 characters):
	 *   "<ws> p,<pf> vbytes." "<load> n-use." "<kb> TKb-pmem." "<kb> FKb pmem." "<kb> TKb pfile." "<kb> FKb pfile."
	 *   "<kb> TKbytes vmem. \n" "<kb> FKb vmem." "P <peak ws> p,<peak pf> v."
	 * The client cuts the text at 255 characters, so the last parts can be missing; a part that doesn't match stops the
	 * parse and leaves it and everything after it empty.
	 */
	struct MemoryStats {
		std::optional<int64_t> workingSetBytes;        // the client process's working set at login (bytes)
		std::optional<int64_t> pagefileUsageBytes;     // the client process's private (commit) bytes at login
		std::optional<int64_t> memoryLoadPercent;      // how much of the system's memory Windows says is in use
		std::optional<int64_t> totalPhysKb;            // physical memory Windows reports (KB)
		std::optional<int64_t> availPhysKb;            // free physical memory at login (KB)
		std::optional<int64_t> totalPageFileKb;        // commit limit: physical memory plus page files (KB)
		std::optional<int64_t> availPageFileKb;        // free commit at login (KB)
		std::optional<int64_t> totalVirtualKb;         // the 32-bit client's own address space, not the system's (KB)
		std::optional<int64_t> availVirtualKb;         // free address space in the client (KB)
		std::optional<int64_t> peakWorkingSetBytes;    // the client process's peak working set (bytes)
		std::optional<int64_t> peakPagefileUsageBytes; // the client process's peak private bytes

		// Every part was read
		bool complete{};
	};

	MemoryStats ParseMemoryStats(std::string_view text);
}
