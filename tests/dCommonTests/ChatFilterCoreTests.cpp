#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ChatFilterCore.h"

using namespace ChatFilterWords;
using Spans = std::set<std::pair<uint8_t, uint8_t>>;

// The hash is a compile-time constant: the same value on every compiler, standard library and platform
static_assert(Hash("") == 0xcbf29ce484222325ULL);
static_assert(Hash("a") == 0xaf63dc4c8601ec8cULL);
static_assert(Hash("foobar") == 0x85944171f73967e8ULL);

TEST(ChatFilterCoreTest, HashIsFnv1a64) {
	// The standard FNV-1a 64 test vectors
	EXPECT_EQ(Hash(""), 0xcbf29ce484222325ULL);
	EXPECT_EQ(Hash("a"), 0xaf63dc4c8601ec8cULL);
	EXPECT_EQ(Hash("foobar"), 0x85944171f73967e8ULL);
	// Words of the client's chatplus_en_us.txt, as the filter hashes them (lower case)
	EXPECT_EQ(Hash("hello"), 0xa430d84680aabd0bULL);
	EXPECT_EQ(Hash("brick"), 0xf9236d1e24832c9aULL);
	EXPECT_EQ(Hash(AsciiLower("Brick")), Hash("brick"));
	// A phrase is its words joined by one space
	EXPECT_EQ(Hash("bad phrase"), 0x72e9fce49c1b0875ULL);
	// Bytes, not chars: a high byte hashes as 0x80-0xFF whether char is signed or not
	EXPECT_EQ(Hash("\xC3\xA9"), 0x0ac21707b7181e01ULL);
}

TEST(ChatFilterCoreTest, NormalizeEntry) {
	EXPECT_EQ(NormalizeWord("Hello!?"), "hello");
	EXPECT_EQ(NormalizeEntry("  Bad \t PHRASE!  "), "bad phrase");
	EXPECT_EQ(NormalizeEntry("word , here"), "word here");
	EXPECT_EQ(NormalizeEntry("..."), "");
	EXPECT_EQ(WordCount("bad phrase here"), 3u);
	EXPECT_EQ(WordCount("word"), 1u);
	EXPECT_EQ(WordCount(""), 0u);
	// ASCII only: other bytes are left alone whatever the locale
	EXPECT_EQ(AsciiLower("\xC3\x89Z"), "\xC3\x89z");
}

TEST(ChatFilterCoreTest, DcfBytesAreFixed) {
	WordList list;
	list.AddEntry("a");
	list.AddEntry("bad phrase");
	const auto bytes = dChatFilterDCF::Serialize(list);
	// magic DCFB, version 3, 2 words at most, 2 hashes, then the hashes sorted, all little-endian
	const std::string expected(
		"DCFB" "\x03\x00\x00\x00" "\x02\x00\x00\x00" "\x02\x00\x00\x00\x00\x00\x00\x00"
		"\x75\x08\x1b\x9c\xe4\xfc\xe9\x72" "\x8c\xec\x01\x86\x4c\xdc\x63\xaf", 4 + 4 + 4 + 8 + 16);
	EXPECT_EQ(bytes, expected);

	const auto parsed = dChatFilterDCF::Parse(bytes);
	ASSERT_EQ(parsed.status, dChatFilterDCF::eStatus::OK);
	EXPECT_EQ(parsed.list.maxWords, 2u);
	EXPECT_EQ(parsed.list.hashes, list.hashes);
}

TEST(ChatFilterCoreTest, DcfRejectsOldAndBadFiles) {
	// Version 2 (std::hash, platform dependent) is refused, not guessed at
	const std::string old("DCFB" "\x02\x00\x00\x00" "\x01\x00\x00\x00\x00\x00\x00\x00" "\x11\x22\x33\x44\x55\x66\x77\x88", 24);
	EXPECT_EQ(dChatFilterDCF::Parse(old).status, dChatFilterDCF::eStatus::OLD_FORMAT);
	EXPECT_EQ(dChatFilterDCF::Parse("XCFB\x03\x00\x00\x00").status, dChatFilterDCF::eStatus::NOT_DCF);
	EXPECT_EQ(dChatFilterDCF::Parse(std::string("DCFB\x09\x00\x00\x00", 8)).status, dChatFilterDCF::eStatus::UNKNOWN);
	WordList list;
	list.AddEntry("word");
	auto bytes = dChatFilterDCF::Serialize(list);
	bytes.pop_back();
	EXPECT_EQ(dChatFilterDCF::Parse(bytes).status, dChatFilterDCF::eStatus::TRUNCATED);
	EXPECT_EQ(dChatFilterDCF::ReadFile("no such blocklist.dcf").status, dChatFilterDCF::eStatus::MISSING);
}

TEST(ChatFilterCoreTest, BlockListFromText) {
	const auto list = dChatFilterDCF::BlockListFromText("Badword\r\n\r\n  Very   BAD phrase!\nbadword\n...\n");
	EXPECT_EQ(list.Size(), 2u);
	EXPECT_TRUE(list.Contains("badword"));
	EXPECT_TRUE(list.Contains("very bad phrase"));
	EXPECT_EQ(list.maxWords, 3u);
	// Written and read back, the same list
	const auto parsed = dChatFilterDCF::Parse(dChatFilterDCF::Serialize(list));
	ASSERT_EQ(parsed.status, dChatFilterDCF::eStatus::OK);
	EXPECT_EQ(parsed.list.hashes, list.hashes);
	EXPECT_EQ(parsed.list.maxWords, 3u);
}

namespace {
	Lists FreeChatLists(std::string_view blockText) {
		Lists lists;
		// Through the file format, as the servers load it
		lists.denied = dChatFilterDCF::Parse(dChatFilterDCF::Serialize(dChatFilterDCF::BlockListFromText(blockText))).list;
		lists.approved = dChatFilterDCF::AllowListFromText("hello\nthere\nfriend\nbad\nphrase\n");
		return lists;
	}
}

TEST(ChatFilterCoreTest, BlockedWordFromFileIsStopped) {
	const auto lists = FreeChatLists("badword\n");
	EXPECT_EQ(CheckMessage("hello badword there", false, lists), (Spans{ { 6, 7 } }));
	EXPECT_EQ(CheckMessage("hello BadWord!", false, lists), (Spans{ { 6, 8 } }));
	EXPECT_TRUE(CheckMessage("hello there", false, lists).empty());
	// Whitelist chat doesn't use the block list: the word is simply not allowed there
	EXPECT_EQ(CheckMessage("hello badword", true, lists), (Spans{ { 6, 7 } }));
	// No block list: free chat stops everything
	EXPECT_EQ(CheckMessage("hello", false, Lists{}), (Spans{ { 0, 5 } }));
}

TEST(ChatFilterCoreTest, PhrasesAtStartMiddleEnd) {
	const auto lists = FreeChatLists("bad phrase\n");
	EXPECT_EQ(CheckMessage("bad phrase hello", false, lists), (Spans{ { 0, 10 } }));
	EXPECT_EQ(CheckMessage("hello bad phrase there", false, lists), (Spans{ { 6, 10 } }));
	EXPECT_EQ(CheckMessage("hello bad phrase", false, lists), (Spans{ { 6, 10 } }));
	// The words alone are fine
	EXPECT_TRUE(CheckMessage("bad hello phrase", false, lists).empty());
	EXPECT_TRUE(CheckMessage("phrase bad", false, lists).empty());
}

TEST(ChatFilterCoreTest, PhrasesWithExtraSpacesAndPunctuation) {
	const auto lists = FreeChatLists("bad phrase\n");
	// Two spaces: the span covers both words and the gap
	EXPECT_EQ(CheckMessage("hi  Bad  Phrase!", false, lists), (Spans{ { 4, 12 } }));
	// A piece that is only punctuation is skipped
	EXPECT_EQ(CheckMessage("bad ... phrase", false, lists), (Spans{ { 0, 14 } }));
	EXPECT_EQ(CheckMessage("bad, phrase.", false, lists), (Spans{ { 0, 12 } }));
}

TEST(ChatFilterCoreTest, PhrasesOverlapWithWords) {
	const auto lists = FreeChatLists("bad phrase\nphrase here\nbadword\nfriend\n");
	// Two phrases sharing a word become one span
	EXPECT_EQ(CheckMessage("a bad phrase here b", false, lists), (Spans{ { 2, 15 } }));
	// A blocked word inside a blocked phrase: one span for the phrase
	EXPECT_EQ(CheckMessage("bad phrase", false, FreeChatLists("bad phrase\nphrase\n")), (Spans{ { 0, 10 } }));
	// A blocked word right after a phrase: its own span
	EXPECT_EQ(CheckMessage("bad phrase badword", false, lists), (Spans{ { 0, 10 }, { 11, 7 } }));
	// Longest match wins where it starts
	EXPECT_EQ(CheckMessage("friend bad phrase", false, FreeChatLists("friend\nfriend bad\nbad phrase\n")), (Spans{ { 0, 17 } }));
}

TEST(ChatFilterCoreTest, DashboardPhrasesInWhitelistChat) {
	auto lists = FreeChatLists("");
	lists.customBlocked.AddEntry(NormalizeEntry("Bad Phrase"));
	// Blocked on the dashboard: stopped in whitelist chat too, even though each word is allowed
	EXPECT_EQ(CheckMessage("hello bad phrase", true, lists), (Spans{ { 6, 10 } }));
	EXPECT_TRUE(CheckMessage("hello bad there phrase", true, lists).empty());
	// Whitelist chat checks one word at a time, as the client does
	EXPECT_EQ(CheckMessage("hello stranger", true, lists), (Spans{ { 6, 8 } }));
	lists.customAllowed.AddEntry("stranger");
	EXPECT_TRUE(CheckMessage("hello stranger", true, lists).empty());
}

TEST(ChatFilterCoreTest, ShippedBlockListIsPortable) {
	const auto parsed = dChatFilterDCF::ReadFile(std::string(DLU_SOURCE_DIR) + "/resources/blocklist.dcf");
	ASSERT_EQ(parsed.status, dChatFilterDCF::eStatus::OK);
	Lists lists;
	lists.denied = parsed.list;
	EXPECT_EQ(CheckMessage("what crap", false, lists), (Spans{ { 5, 4 } }));
	EXPECT_TRUE(CheckMessage("hello there", false, lists).empty());
}
