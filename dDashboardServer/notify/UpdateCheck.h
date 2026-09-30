#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "BuildInfo.h"

/**
 * The pure half of the update check: what build is running, how to read GitHub's answers about the configured
 * repository, and how to say what they mean. The network half (a worker thread, ETags, the schedule) is in
 * UpdateChecker. Nothing here reads the config or the network, so it is all testable from recorded answers.
 */
namespace UpdateCheck {
	struct Version {
		uint32_t major{};
		uint32_t minor{};
		uint32_t patch{};

		auto operator<=>(const Version&) const = default;
		std::string ToString() const; // "3.0.1"
	};

	// "v3.0.1", "3.0.1", "V3.1" or "v3.0.1-rc1": the numbers up to the first thing that isn't one. Empty without a number.
	std::optional<Version> ParseVersion(std::string_view tag);

	// What was built: filled from BuildInfo in the server, by hand in tests
	struct Build {
		BuildInfo::eBuildKind kind{};
		Version version;
		std::string commit; // full hash, empty without git
		std::string branch; // empty when detached or without git
		bool dirty{};

		std::string ShortCommit() const { return commit.substr(0, 8); }
	};

	Build CurrentBuild();

	// "release v3.0.0", "a local development build of commit 1a2b3c4d with uncommitted changes", "a CI build of commit
	// 1a2b3c4d": the running build in a phrase that fits after "This server is running "
	std::string DescribeBuild(const Build& build);

	// Short form for a badge: "v3.0.0", "Local dev build 1a2b3c4d*", "CI build 1a2b3c4d"
	std::string BuildLabel(const Build& build);

	// ---- GitHub's answers ----

	// GET /repos/{repo}/releases/latest (the newest release that is neither a draft nor a pre-release)
	struct Release {
		std::string tag;
		std::string name;
		std::string url; // the release notes on github.com
		std::string publishedAt; // ISO 8601, as GitHub sends it
		std::optional<Version> version; // from the tag
	};
	// Empty (with error set) for anything that isn't a release object
	std::optional<Release> ParseRelease(const std::string& body, std::string& error);

	// GET /repos/{repo}/compare/{build commit}...{branch}: where the branch is relative to the build
	struct Comparison {
		std::string status; // identical, ahead (the branch has newer commits), behind (the build has commits it lacks) or diverged
		uint32_t commitsBehind{}; // commits on the branch the build doesn't have (GitHub's ahead_by)
		uint32_t commitsAhead{}; // commits in the build the branch doesn't have (GitHub's behind_by)
		std::string url; // the compare page on github.com
	};
	std::optional<Comparison> ParseComparison(const std::string& body, std::string& error);

	// A GitHub error body's message ("Not Found", "API rate limit exceeded for ..."), or empty
	std::string ErrorMessage(const std::string& body);

	// Only owner/name of letters, digits, '-', '_' and '.'
	bool IsValidRepo(std::string_view repo);
	// A branch name safe to put in a compare URL (letters, digits, '-', '_', '.', '/')
	bool IsSafeBranch(std::string_view branch);

	// ---- What the check found ----

	enum class eState : uint8_t {
		UNKNOWN = 0, // not checked yet (or turned off)
		UP_TO_DATE = 1,
		UPDATE_AVAILABLE = 2,
		FAILED = 3, // nothing could be found out
	};

	struct Status {
		std::optional<Release> release; // the newest release, when known
		std::optional<Comparison> comparison; // development builds, when the build's commit is on GitHub
		std::string comparedBranch; // the branch the comparison is against
		std::string error; // what went wrong, if anything did (the rest may still be known)
		int64_t checkedAt{}; // unix time of the last check, 0 before the first
	};

	// Whether the newest release is newer than the build's version
	bool ReleaseIsNewer(const Build& build, const Status& status);
	eState State(const Build& build, const Status& status);

	// One line for the logs, e.g. "Update available: release v3.1.0 (this server is running release v3.0.0)"
	std::string Summary(const Build& build, const Status& status);
}
