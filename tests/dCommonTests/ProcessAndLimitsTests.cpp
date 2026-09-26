#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "GeneralUtils.h"
#include "Process.h"
#include "SpareBackoff.h"

TEST(LikeEscapeTests, WildcardsAreLiteral) {
	EXPECT_EQ(GeneralUtils::LikeEscape("50%_off\\"), "50\\%\\_off\\\\");
	EXPECT_EQ(GeneralUtils::LikeEscape("50%_off!", '!'), "50!%!_off!!");
	EXPECT_EQ(GeneralUtils::LikeEscape("plain", '!'), "plain");
}

TEST(ProcessTests, ShellMetacharactersAreDetected) {
	EXPECT_FALSE(Process::HasShellMetacharacters("/home/me/LEGO Universe (unpacked)/res"));
	EXPECT_FALSE(Process::HasShellMetacharacters("C:\\Games\\LU\\client"));
	EXPECT_TRUE(Process::HasShellMetacharacters("/tmp/$(touch pwned)"));
	EXPECT_TRUE(Process::HasShellMetacharacters("/tmp/`id`"));
	EXPECT_TRUE(Process::HasShellMetacharacters("a\"b"));
	EXPECT_TRUE(Process::HasShellMetacharacters("a; rm -rf /"));
	EXPECT_TRUE(Process::HasShellMetacharacters("a\nb"));
}

#ifndef _WIN32
TEST(ProcessTests, RunsWithoutAShell) {
	const auto dir = std::filesystem::temp_directory_path() / "dlu_process_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	const auto marker = dir / "pwned";
	const auto out = dir / "out.txt";
	// $(...) is handed to echo as text, not run
	EXPECT_EQ(Process::Run({ "echo", "$(touch " + marker.string() + ")" }, out.string()), 0);
	EXPECT_FALSE(std::filesystem::exists(marker));
	std::ifstream in(out);
	std::string line;
	std::getline(in, line);
	EXPECT_EQ(line, "$(touch " + marker.string() + ")");
	EXPECT_NE(Process::Run({ "false" }), 0);
	EXPECT_NE(Process::Run({ "/no/such/program" }), 0);
	EXPECT_EQ(Process::Run({}), -1);
	std::filesystem::remove_all(dir);
}
#endif

TEST(SpareBackoffTests, CrashingSparesWaitLongerEachTime) {
	SpareBackoff backoff;
	EXPECT_TRUE(backoff.CanStart(0));
	backoff.Started(1);
	EXPECT_FALSE(backoff.CanStart(1)); // still starting
	backoff.Lost(10);
	EXPECT_FALSE(backoff.CanStart(15));
	EXPECT_TRUE(backoff.CanStart(10 + SpareBackoff::BASE_DELAY));
	backoff.Started(2);
	backoff.Lost(100);
	EXPECT_FALSE(backoff.CanStart(100 + SpareBackoff::BASE_DELAY));
	EXPECT_TRUE(backoff.CanStart(100 + 2 * SpareBackoff::BASE_DELAY));
	EXPECT_EQ(SpareBackoff::Delay(50), SpareBackoff::MAX_DELAY);
}

TEST(SpareBackoffTests, OneThatDiesSoonAfterStartingStillCounts) {
	SpareBackoff backoff;
	backoff.Started(1);
	backoff.Running(true, 100);
	EXPECT_TRUE(backoff.CanStart(101)); // ready: another may start if needed
	backoff.Lost(160);                   // but it crashed within STABLE
	EXPECT_EQ(backoff.Failures(), 1u);

	backoff.Started(2);
	backoff.Running(true, 1000);
	backoff.Running(true, 1000 + SpareBackoff::STABLE);
	EXPECT_EQ(backoff.Failures(), 0u); // ran long enough: the zone works
	EXPECT_EQ(backoff.Watched(), 0u);
}
