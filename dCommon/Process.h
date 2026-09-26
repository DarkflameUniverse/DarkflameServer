#pragma once

#include <string>
#include <string_view>
#include <vector>

// Run external programs without a shell: arguments are passed as a vector, so nothing in them is interpreted
// ($(...), backticks, ;, quotes, redirections all stay literal).
namespace Process {
	/**
	 * Run a program and wait for it.
	 * @param arguments The program (looked up on PATH when it has no slash) followed by its arguments
	 * @param stdoutFile Where the program's standard output goes; empty discards it
	 * @param stderrFile Where the program's error output goes; empty discards it
	 * @return The program's exit code, or -1 when it could not be started or was killed by a signal
	 */
	int Run(const std::vector<std::string>& arguments, const std::string& stdoutFile = "", const std::string& stderrFile = "");

	// Whether text contains characters a shell uses to run or chain commands (quotes, $, `, ;, |, &, <, >, newlines).
	// Parentheses, spaces and backslashes are allowed: they are common in real folder names.
	// Used to refuse suspicious paths even though Run never uses a shell.
	bool HasShellMetacharacters(std::string_view text);
}
