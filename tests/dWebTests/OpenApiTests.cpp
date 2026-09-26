#include <gtest/gtest.h>

#include "OpenApi.h"

TEST(OpenApiTests, ReadsQueryParameters) {
	const auto params = OpenApi::QueryParams("Server health over time. Query: ?range=24h|7d|30d. Players and worlds are the highest in each point");
	ASSERT_EQ(params.size(), 1u);
	EXPECT_EQ(params[0].name, "range");
	EXPECT_EQ(params[0].choices, (std::vector<std::string>{ "24h", "7d", "30d" }));

	const auto search = OpenApi::QueryParams("Search the logs. Query: ?q= (text, case-insensitive), &server= (e.g. WorldServer), &files= (newest per server, default 3, max 20). Runs in the background");
	ASSERT_EQ(search.size(), 3u);
	EXPECT_EQ(search[0].name, "q");
	EXPECT_EQ(search[0].description, "text, case-insensitive");
	EXPECT_EQ(search[2].name, "files");

	const auto chat = OpenApi::QueryParams("Chat messages. Query: after (the last id you have; for bridges polling), limit (max 500), blocked=1 (only filtered). Returns");
	ASSERT_EQ(chat.size(), 3u);
	EXPECT_EQ(chat[0].name, "after");
	EXPECT_EQ(chat[1].description, "max 500");
	EXPECT_EQ(chat[2].name, "blocked");
}

TEST(OpenApiTests, ReadsBodyParameters) {
	const auto params = OpenApi::BodyParams("Set a setting. Body: {file, name, value (null to clear the web value), webWins (beat the file)}. Servers reload");
	ASSERT_EQ(params.size(), 4u);
	EXPECT_EQ(params[2].name, "value");
	EXPECT_EQ(params[2].description, "null to clear the web value");

	const auto batch = OpenApi::BodyParams("Save several. Body: {changes: [{file, name, value, webWins}]}. Nothing is saved unless");
	ASSERT_EQ(batch.size(), 1u);
	EXPECT_EQ(batch[0].name, "changes");
	EXPECT_TRUE(batch[0].isList);

	EXPECT_EQ(OpenApi::BodyParams("Reject a name. Body (optional): {reason}, shown to the player").size(), 1u);
	EXPECT_TRUE(OpenApi::BodyParams("No body here").empty());
}

TEST(OpenApiTests, BuildsPathsWithParameters) {
	const auto doc = OpenApi::Build({
		{ "GET", "/api/accounts/:id", "Account details. Without accounts_view, only your own", 0, "" },
		{ "POST", "/api/leaderboards/:id/remove", "Remove one score. Body: {character_id, reason}", 5, "leaderboards_manage" },
		{ "POST", "/api/tables/chat_log", "The chat log (DataTables). Body adds {channel, zone}", 3, "chat_view" },
	}, "Test");
	ASSERT_TRUE(doc["paths"].contains("/api/accounts/{id}"));
	const auto& get = doc["paths"]["/api/accounts/{id}"]["get"];
	EXPECT_EQ(get["parameters"][0]["name"], "id");
	EXPECT_EQ(get["parameters"][0]["in"], "path");
	EXPECT_EQ(get["tags"][0], "accounts");
	EXPECT_EQ(get["summary"], "Account details");

	const auto& remove = doc["paths"]["/api/leaderboards/{id}/remove"]["post"];
	EXPECT_EQ(remove["x-permission"], "leaderboards_manage");
	EXPECT_TRUE(remove["requestBody"]["content"]["application/json"]["schema"]["properties"].contains("character_id"));

	const auto& table = doc["paths"]["/api/tables/chat_log"]["post"]["requestBody"]["content"]["application/json"];
	EXPECT_TRUE(table["schema"]["properties"].contains("length"));
	EXPECT_TRUE(table["schema"]["properties"].contains("channel"));
	EXPECT_EQ(table["example"]["length"], 25);
}
