#include <gtest/gtest.h>

#include <cstring>

#include "StrikeSteps.h"
#include "ChatFilterWords.h"

using StrikeSteps::Threshold;

namespace {
	const std::vector<Threshold> THRESHOLDS{ { eStrikeStep::WARN, 2, 0 }, { eStrikeStep::MUTE, 3, 3 }, { eStrikeStep::BAN, 5, 7 } };
}

TEST(StrikeStepsTest, NothingBelowTheFirstThreshold) {
	EXPECT_FALSE(StrikeSteps::Choose(THRESHOLDS, 0, {}));
	EXPECT_FALSE(StrikeSteps::Choose(THRESHOLDS, 1, {}));
}

TEST(StrikeStepsTest, HighestReachedThresholdApplies) {
	EXPECT_EQ(StrikeSteps::Choose(THRESHOLDS, 2, {})->step, eStrikeStep::WARN);
	EXPECT_EQ(StrikeSteps::Choose(THRESHOLDS, 3, {})->step, eStrikeStep::MUTE);
	EXPECT_EQ(StrikeSteps::Choose(THRESHOLDS, 3, {})->days, 3);
	EXPECT_EQ(StrikeSteps::Choose(THRESHOLDS, 5, {})->step, eStrikeStep::BAN);
	// Past the last threshold (e.g. thresholds set after the strikes were given) the last one still applies once
	EXPECT_EQ(StrikeSteps::Choose(THRESHOLDS, 9, {})->step, eStrikeStep::BAN);
}

TEST(StrikeStepsTest, NotAppliedTwiceForTheSameCount) {
	EXPECT_FALSE(StrikeSteps::Choose(THRESHOLDS, 3, { { eStrikeStep::MUTE, 3 } }));
	// A fourth strike after the mute at three does nothing more
	EXPECT_FALSE(StrikeSteps::Choose(THRESHOLDS, 4, { { eStrikeStep::WARN, 2 }, { eStrikeStep::MUTE, 3 } }));
	// Reaching the ban threshold still bans
	EXPECT_EQ(StrikeSteps::Choose(THRESHOLDS, 5, { { eStrikeStep::MUTE, 3 } })->step, eStrikeStep::BAN);
}

TEST(StrikeStepsTest, OffStepsAndTies) {
	EXPECT_FALSE(StrikeSteps::Choose({ { eStrikeStep::WARN, 0, 0 }, { eStrikeStep::BAN, 0, 0 } }, 10, {}));
	// The same number for two steps: the more severe one
	EXPECT_EQ(StrikeSteps::Choose({ { eStrikeStep::WARN, 3, 0 }, { eStrikeStep::MUTE, 3, 1 } }, 3, {})->step, eStrikeStep::MUTE);
}

TEST(ChatFilterWordsTest, FilterWord) {
	EXPECT_EQ(ModerationTools::FilterWord("  Hello! "), "hello");
	EXPECT_EQ(ModerationTools::FilterWord("W.o,r;d?"), "word");
	EXPECT_FALSE(ModerationTools::FilterWord(""));
	EXPECT_FALSE(ModerationTools::FilterWord("two words"));
	EXPECT_FALSE(ModerationTools::FilterWord("!!!"));
	EXPECT_FALSE(ModerationTools::FilterWord(std::string(65, 'a')));
}

TEST(ChatFilterWordsTest, HasFilterWord) {
	EXPECT_TRUE(ModerationTools::HasFilterWord("You are a Bad, BAD person!", "bad"));
	EXPECT_TRUE(ModerationTools::HasFilterWord("bad", "bad"));
	EXPECT_FALSE(ModerationTools::HasFilterWord("badger badminton", "bad"));
	EXPECT_FALSE(ModerationTools::HasFilterWord("", "bad"));
}

TEST(ChatFilterWordsTest, FileWords) {
	const auto words = ModerationTools::FileWords("Hello\r\nworld\n\nhello\nZebra\r\n");
	ASSERT_EQ(words, (std::vector<std::string>{ "hello", "world", "zebra" }));
	ASSERT_TRUE(ModerationTools::FileWords("").empty());
}

TEST(ChatFilterWordsTest, DcfHashes) {
	const std::vector<size_t> hashes{ ModerationTools::WordHash("badword"), 42 };
	std::string bytes(sizeof(dChatFilterDCF::fileHeader) + sizeof(size_t) * (hashes.size() + 1), '\0');
	const dChatFilterDCF::fileHeader header{ dChatFilterDCF::header, dChatFilterDCF::formatVersion };
	const size_t count = hashes.size();
	std::memcpy(bytes.data(), &header, sizeof(header));
	std::memcpy(bytes.data() + sizeof(header), &count, sizeof(count));
	std::memcpy(bytes.data() + sizeof(header) + sizeof(count), hashes.data(), sizeof(size_t) * count);
	ASSERT_EQ(ModerationTools::DcfHashes(bytes), hashes);

	// Wrong header, other version, or fewer hashes than it says
	auto wrong = bytes;
	wrong[0] = 'X';
	ASSERT_FALSE(ModerationTools::DcfHashes(wrong).has_value());
	auto version = bytes;
	version[sizeof(uint32_t)] = 9;
	ASSERT_FALSE(ModerationTools::DcfHashes(version).has_value());
	ASSERT_FALSE(ModerationTools::DcfHashes(bytes.substr(0, bytes.size() - sizeof(size_t) * 2)).has_value());
	ASSERT_FALSE(ModerationTools::DcfHashes("DCFB").has_value());
}

namespace {
	ModerationTools::WordSources Sources(bool blockFileLoaded = true) {
		ModerationTools::WordSources sources;
		sources.dashboard = [](const std::string& w) -> std::optional<bool> {
			if (w == "darn") return false;
			if (w == "brickbuild") return true;
			return std::nullopt;
		};
		sources.allowFile = [](const std::string& w) { return w == "hello" || w == "darn"; };
		sources.characterName = [](const std::string& w) { return w == "bob"; };
		sources.blockFile = [](const std::string& w) { return w == "rude"; };
		sources.blockFileLoaded = blockFileLoaded;
		return sources;
	}

	std::vector<std::string> Reasons(const std::vector<ModerationTools::WordVerdict>& verdicts) {
		std::vector<std::string> reasons;
		for (const auto& v : verdicts) reasons.push_back((v.stopped ? "x " : "ok ") + v.reason);
		return reasons;
	}
}

TEST(ChatFilterWordsTest, ExplainNormalChat) {
	const auto verdicts = ModerationTools::ExplainMessage("Hello, Bob! brickbuild darn zzz", true, Sources());
	ASSERT_EQ(verdicts.size(), 5u);
	EXPECT_EQ(verdicts[0].text, "Hello,");
	EXPECT_EQ(verdicts[0].word, "hello");
	// A blocked word is stopped even though the file allows it
	EXPECT_EQ(Reasons(verdicts), (std::vector<std::string>{ "ok allow_file", "ok character_name", "ok allowed_here", "x blocked_here", "x not_allowed" }));
	EXPECT_TRUE(ModerationTools::ExplainMessage("", true, Sources()).empty());
}

TEST(ChatFilterWordsTest, ExplainFreeChat) {
	EXPECT_EQ(Reasons(ModerationTools::ExplainMessage("rude zzz darn", false, Sources())),
		(std::vector<std::string>{ "x block_file", "ok not_in_block_file", "x blocked_here" }));
	// Without blocklist.dcf free chat stops every word
	EXPECT_EQ(Reasons(ModerationTools::ExplainMessage("zzz darn", false, Sources(false))),
		(std::vector<std::string>{ "x no_block_file", "x no_block_file" }));
}
