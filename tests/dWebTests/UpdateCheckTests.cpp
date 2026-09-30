#include <filesystem>
#include <fstream>
#include <sstream>

#include <gtest/gtest.h>

#include "UpdateCheck.h"

using namespace UpdateCheck;
using BuildInfo::eBuildKind;

namespace {
	// Hand-written samples in the shape of GitHub's REST answers
	std::string Sample(const std::string& name) {
		std::ifstream in(std::filesystem::path(DLU_SOURCE_DIR) / "tests" / "dWebTests" / "UpdateCheckSamples" / name);
		std::stringstream text;
		text << in.rdbuf();
		EXPECT_FALSE(text.str().empty()) << name;
		return text.str();
	}

	Build Make(eBuildKind kind, bool dirty = false, std::string commit = "1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d") {
		return { .kind = kind, .version = { 3, 0, 0 }, .commit = std::move(commit), .branch = "main", .dirty = dirty };
	}

	Release ReleaseOf(const std::string& tag) {
		return { .tag = tag, .url = "https://github.com/example/server/releases/tag/" + tag, .version = ParseVersion(tag) };
	}
}

TEST(UpdateCheckTests, VersionsParse) {
	EXPECT_EQ(ParseVersion("v3.0.1"), (Version{ 3, 0, 1 }));
	EXPECT_EQ(ParseVersion("3.0.1"), (Version{ 3, 0, 1 }));
	EXPECT_EQ(ParseVersion("V3.1"), (Version{ 3, 1, 0 }));
	EXPECT_EQ(ParseVersion("v10"), (Version{ 10, 0, 0 }));
	EXPECT_EQ(ParseVersion("v3.0.1-rc1"), (Version{ 3, 0, 1 }));
	EXPECT_EQ(ParseVersion("v3.0.1+g1a2b"), (Version{ 3, 0, 1 }));
	EXPECT_EQ(ParseVersion("v3."), (Version{ 3, 0, 0 }));
	EXPECT_FALSE(ParseVersion("nightly"));
	EXPECT_FALSE(ParseVersion(""));
	EXPECT_FALSE(ParseVersion("v"));
	EXPECT_FALSE(ParseVersion("v99999999999"));
	EXPECT_EQ((Version{ 3, 0, 1 }).ToString(), "3.0.1");
}

TEST(UpdateCheckTests, VersionsCompareNumerically) {
	EXPECT_LT(*ParseVersion("v3.0.9"), *ParseVersion("v3.0.10"));
	EXPECT_LT(*ParseVersion("v3.9.0"), *ParseVersion("v3.10.0"));
	EXPECT_LT(*ParseVersion("v2.99.99"), *ParseVersion("v3.0.0"));
	EXPECT_EQ(*ParseVersion("v3.1"), *ParseVersion("3.1.0"));
	EXPECT_GT(*ParseVersion("v3.0.1"), *ParseVersion("v3.0.0"));
}

TEST(UpdateCheckTests, ReleaseParses) {
	std::string error;
	const auto release = ParseRelease(Sample("release_latest.json"), error);
	ASSERT_TRUE(release) << error;
	EXPECT_EQ(release->tag, "v3.1.0");
	EXPECT_EQ(release->name, "Version 3.1.0");
	EXPECT_EQ(release->url, "https://github.com/example/server/releases/tag/v3.1.0");
	EXPECT_EQ(release->publishedAt, "2026-08-02T12:30:00Z");
	EXPECT_EQ(release->version, (Version{ 3, 1, 0 }));

	// A tag that isn't a version is still a release, just never "newer"
	const auto odd = ParseRelease(Sample("release_odd_tag.json"), error);
	ASSERT_TRUE(odd);
	EXPECT_EQ(odd->tag, "nightly");
	EXPECT_FALSE(odd->version);
}

TEST(UpdateCheckTests, ErrorsAndGarbageDontParse) {
	std::string error;
	EXPECT_FALSE(ParseRelease(Sample("not_found.json"), error));
	EXPECT_EQ(error, "GitHub: Not Found");
	EXPECT_EQ(ErrorMessage(Sample("not_found.json")), "Not Found");
	EXPECT_FALSE(ParseRelease("<html>rate limited</html>", error));
	EXPECT_FALSE(ParseRelease("[]", error));
	EXPECT_FALSE(ParseRelease(R"({"tag_name": 5})", error));
	EXPECT_FALSE(ParseComparison(Sample("not_found.json"), error)); // it has "status": "404"
	EXPECT_EQ(error, "GitHub: Not Found");
	EXPECT_FALSE(ParseComparison("", error));
	EXPECT_EQ(ErrorMessage("not json"), "");
}

TEST(UpdateCheckTests, ComparisonsParse) {
	std::string error;
	const auto behind = ParseComparison(Sample("compare_behind.json"), error);
	ASSERT_TRUE(behind) << error;
	EXPECT_EQ(behind->status, "ahead");
	EXPECT_EQ(behind->commitsBehind, 12u); // the branch is ahead of the build by 12
	EXPECT_EQ(behind->commitsAhead, 0u);
	EXPECT_EQ(behind->url, "https://github.com/example/server/compare/1a2b3c4d5e6f...main");

	const auto diverged = ParseComparison(Sample("compare_diverged.json"), error);
	ASSERT_TRUE(diverged);
	EXPECT_EQ(diverged->commitsBehind, 1u);
	EXPECT_EQ(diverged->commitsAhead, 3u);

	// Negative or missing counts read as 0
	const auto odd = ParseComparison(R"({"status": "identical", "ahead_by": -4})", error);
	ASSERT_TRUE(odd);
	EXPECT_EQ(odd->commitsBehind, 0u);
}

TEST(UpdateCheckTests, BuildWording) {
	EXPECT_EQ(DescribeBuild(Make(eBuildKind::RELEASE)), "release v3.0.0");
	EXPECT_EQ(DescribeBuild(Make(eBuildKind::LOCAL)), "a local development build of commit 1a2b3c4d");
	EXPECT_EQ(DescribeBuild(Make(eBuildKind::LOCAL, true)), "a local development build of commit 1a2b3c4d with uncommitted changes");
	EXPECT_EQ(DescribeBuild(Make(eBuildKind::LOCAL, false, "")), "a local development build (no git commit recorded)");
	EXPECT_EQ(DescribeBuild(Make(eBuildKind::CI)), "a CI build of commit 1a2b3c4d");
	EXPECT_EQ(DescribeBuild(Make(eBuildKind::UNKNOWN)), "an unidentified build of commit 1a2b3c4d");

	EXPECT_EQ(BuildLabel(Make(eBuildKind::RELEASE)), "v3.0.0");
	EXPECT_EQ(BuildLabel(Make(eBuildKind::LOCAL, true)), "Local dev build 1a2b3c4d*");
	EXPECT_EQ(BuildLabel(Make(eBuildKind::LOCAL, false, "")), "Local dev build");
	EXPECT_EQ(BuildLabel(Make(eBuildKind::CI)), "CI build 1a2b3c4d");
}

TEST(UpdateCheckTests, ReleaseBuildState) {
	const auto build = Make(eBuildKind::RELEASE);
	Status status;
	EXPECT_EQ(State(build, status), eState::UNKNOWN);
	EXPECT_EQ(Summary(build, status), "Not checked for updates yet");

	status.checkedAt = 1;
	status.error = "Couldn't reach GitHub for the latest release: Timeout was reached";
	EXPECT_EQ(State(build, status), eState::FAILED);
	EXPECT_EQ(Summary(build, status), "Could not check for updates: Couldn't reach GitHub for the latest release: Timeout was reached");

	status.error.clear();
	status.release = ReleaseOf("v3.0.0");
	EXPECT_EQ(State(build, status), eState::UP_TO_DATE);
	EXPECT_EQ(Summary(build, status), "Up to date: the newest release is v3.0.0; this server is running release v3.0.0");

	status.release = ReleaseOf("v3.1.0");
	EXPECT_TRUE(ReleaseIsNewer(build, status));
	EXPECT_EQ(State(build, status), eState::UPDATE_AVAILABLE);
	EXPECT_EQ(Summary(build, status), "Update available: release v3.1.0 (this server is running release v3.0.0): https://github.com/example/server/releases/tag/v3.1.0");

	// Release builds don't care about commits on a branch
	status.release = ReleaseOf("v3.0.0");
	status.comparison = Comparison{ .status = "ahead", .commitsBehind = 5 };
	EXPECT_EQ(State(build, status), eState::UP_TO_DATE);

	// An older or unversioned newest release isn't an update
	status.comparison.reset();
	status.release = ReleaseOf("v2.9.0");
	EXPECT_EQ(State(build, status), eState::UP_TO_DATE);
	status.release = ReleaseOf("nightly");
	EXPECT_EQ(State(build, status), eState::UP_TO_DATE);
}

TEST(UpdateCheckTests, DevelopmentBuildState) {
	const auto build = Make(eBuildKind::LOCAL, true);
	Status status;
	status.checkedAt = 1;
	status.release = ReleaseOf("v3.0.0");
	status.comparedBranch = "main";
	status.comparison = Comparison{ .status = "ahead", .commitsBehind = 12, .url = "https://github.com/example/server/compare/1a2b3c4d...main" };
	EXPECT_EQ(State(build, status), eState::UPDATE_AVAILABLE);
	EXPECT_EQ(Summary(build, status), "Update available: 12 newer commits on main (this server is running a local development build of commit 1a2b3c4d "
		"with uncommitted changes): https://github.com/example/server/compare/1a2b3c4d...main");

	status.comparison->commitsBehind = 1;
	status.release = ReleaseOf("v3.1.0");
	EXPECT_EQ(Summary(build, status), "Update available: release v3.1.0, and 1 newer commit on main (this server is running a local development build "
		"of commit 1a2b3c4d with uncommitted changes): https://github.com/example/server/compare/1a2b3c4d...main");

	status.release = ReleaseOf("v3.0.0");
	status.comparison = Comparison{ .status = "behind", .commitsAhead = 3 };
	EXPECT_EQ(State(build, status), eState::UP_TO_DATE);
	EXPECT_EQ(Summary(build, status), "Up to date with main (and 3 commits not on it); this server is running a local development build of commit 1a2b3c4d with uncommitted changes");

	// The commit isn't on GitHub: only the release is known
	status.comparison.reset();
	status.error = "Commit 1a2b3c4d isn't on GitHub";
	EXPECT_EQ(State(build, status), eState::UP_TO_DATE);
	EXPECT_EQ(Summary(Make(eBuildKind::CI), status), "Up to date: the newest release is v3.0.0; this server is running a CI build of commit 1a2b3c4d");
}

TEST(UpdateCheckTests, RepoAndBranchNamesAreChecked) {
	EXPECT_TRUE(IsValidRepo("DarkflameUniverse/DarkflameServer"));
	EXPECT_TRUE(IsValidRepo("some-one/my.fork_2"));
	EXPECT_FALSE(IsValidRepo("DarkflameServer"));
	EXPECT_FALSE(IsValidRepo("/DarkflameServer"));
	EXPECT_FALSE(IsValidRepo("a/"));
	EXPECT_FALSE(IsValidRepo("a/b/c"));
	EXPECT_FALSE(IsValidRepo("a/.."));
	EXPECT_FALSE(IsValidRepo("a/b?x=1"));
	EXPECT_FALSE(IsValidRepo("https://github.com/a/b"));

	EXPECT_TRUE(IsSafeBranch("main"));
	EXPECT_TRUE(IsSafeBranch("dev/someone/experimental"));
	EXPECT_FALSE(IsSafeBranch(""));
	EXPECT_FALSE(IsSafeBranch("a..b"));
	EXPECT_FALSE(IsSafeBranch("a b"));
	EXPECT_FALSE(IsSafeBranch("a?b"));
	EXPECT_FALSE(IsSafeBranch("/main"));
}
