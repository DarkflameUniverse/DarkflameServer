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
	// Phrases: words normalized and joined by one space
	EXPECT_EQ(ModerationTools::FilterWord(" Two   Words! "), "two words");
	EXPECT_TRUE(ModerationTools::IsPhrase("two words"));
	EXPECT_FALSE(ModerationTools::IsPhrase("word"));
	EXPECT_FALSE(ModerationTools::FilterWord("!!!"));
	EXPECT_FALSE(ModerationTools::FilterWord(std::string(65, 'a')));
}

TEST(ChatFilterWordsTest, HasFilterWord) {
	EXPECT_TRUE(ModerationTools::HasFilterWord("You are a Bad, BAD person!", "bad"));
	EXPECT_TRUE(ModerationTools::HasFilterWord("bad", "bad"));
	EXPECT_FALSE(ModerationTools::HasFilterWord("badger badminton", "bad"));
	EXPECT_FALSE(ModerationTools::HasFilterWord("", "bad"));
	// Phrases: the words in a row, whatever the spaces and punctuation between them
	EXPECT_TRUE(ModerationTools::HasFilterWord("well, Bad  Phrase!", "bad phrase"));
	EXPECT_TRUE(ModerationTools::HasFilterWord("bad ... phrase", "bad phrase"));
	EXPECT_FALSE(ModerationTools::HasFilterWord("bad other phrase", "bad phrase"));
	EXPECT_FALSE(ModerationTools::HasFilterWord("phrase bad", "bad phrase"));
}

TEST(ChatFilterWordsTest, FileWords) {
	const auto words = ModerationTools::FileWords("Hello\r\nworld\n\nhello\nZebra\r\n");
	ASSERT_EQ(words, (std::vector<std::string>{ "hello", "world", "zebra" }));
	ASSERT_TRUE(ModerationTools::FileWords("").empty());
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

TEST(ChatFilterWordsTest, ExplainPhrases) {
	auto sources = Sources();
	sources.dashboard = [](const std::string& w) -> std::optional<bool> {
		if (w == "no way") return false;
		return std::nullopt;
	};
	sources.allowFile = [](const std::string& w) { return w == "hello" || w == "no" || w == "way" || w == "rude"; };
	sources.blockFile = [](const std::string& w) { return w == "very rude"; };
	sources.maxWords = 2;
	// A phrase blocked here stops each of its words (and the empty piece between two spaces inside it), in normal chat too
	auto verdicts = ModerationTools::ExplainMessage("hello No  way!", true, sources);
	EXPECT_EQ(Reasons(verdicts), (std::vector<std::string>{ "ok allow_file", "x blocked_here", "x blocked_here", "x blocked_here" }));
	EXPECT_EQ(verdicts[1].phrase, "no way");
	EXPECT_EQ(verdicts[3].text, "way!");
	// The block file's phrases in free chat and in normal chat, even when each word is allowed
	EXPECT_EQ(Reasons(ModerationTools::ExplainMessage("very rude", false, sources)), (std::vector<std::string>{ "x block_file", "x block_file" }));
	sources.allowFile = [](const std::string& w) { return w == "very" || w == "rude"; };
	EXPECT_EQ(Reasons(ModerationTools::ExplainMessage("very rude", true, sources)), (std::vector<std::string>{ "x block_file", "x block_file" }));
	EXPECT_EQ(Reasons(ModerationTools::ExplainMessage("rude very", false, sources)), (std::vector<std::string>{ "ok not_in_block_file", "ok not_in_block_file" }));
}

TEST(ChatFilterWordsTest, ExplainAllowedPhrases) {
	auto sources = Sources();
	sources.dashboard = [](const std::string& w) -> std::optional<bool> {
		if (w == "nexus tower") return true;
		return std::nullopt;
	};
	sources.maxAllowedWords = 2;
	// Neither word is allowed alone; together they are
	auto verdicts = ModerationTools::ExplainMessage("hello Nexus  Tower", true, sources);
	EXPECT_EQ(Reasons(verdicts), (std::vector<std::string>{ "ok allow_file", "ok allowed_here", "ok allowed_here", "ok allowed_here" }));
	EXPECT_EQ(verdicts[1].phrase, "nexus tower");
	EXPECT_EQ(Reasons(ModerationTools::ExplainMessage("tower", true, sources)), (std::vector<std::string>{ "x not_allowed" }));
	// Free chat doesn't use allowed phrases
	EXPECT_EQ(Reasons(ModerationTools::ExplainMessage("nexus tower", false, sources)), (std::vector<std::string>{ "ok not_in_block_file", "ok not_in_block_file" }));
}
