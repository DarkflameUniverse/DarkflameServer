#include <gtest/gtest.h>

#include <map>

#include "LocaleText.h"

namespace {
	const std::string EMPTY;
	const std::map<std::string, std::string> PHRASES{ { "MissionEmail_1_subjectText", "Welcome!" }, { "MAIL_SYSTEM_NOTIFICATION", "LEGO Universe" } };

	std::string Expand(const std::string& text) {
		return LocaleText::Expand(text, [](const std::string& key) -> const std::string& {
			const auto it = PHRASES.find(key);
			return it == PHRASES.end() ? EMPTY : it->second;
		});
	}
}

TEST(LocaleTextTest, ReplacesKeys) {
	EXPECT_EQ(Expand("%[MissionEmail_1_subjectText]"), "Welcome!");
	EXPECT_EQ(Expand("From %[MAIL_SYSTEM_NOTIFICATION]: %[MissionEmail_1_subjectText] ok"), "From LEGO Universe: Welcome! ok");
}

TEST(LocaleTextTest, LeavesOtherTextAlone) {
	EXPECT_EQ(Expand("Items returned to you"), "Items returned to you");
	EXPECT_EQ(Expand("%[Unknown_key] and 100%"), "%[Unknown_key] and 100%");
	EXPECT_EQ(Expand("%[not closed"), "%[not closed");
	EXPECT_EQ(Expand("[x] %[]"), "[x] %[]");
	EXPECT_EQ(Expand(""), "");
}
