#include <gtest/gtest.h>

#include <chrono>
#include <memory>

#include "AccountRules.h"
#include "ApiKeyLimiter.h"
#include "ApiKeyScope.h"
#include "HTTPContext.h"
#include "HTTPReply.h"
#include "Permissions.h"
#include "RequireAuthMiddleware.h"

using AccountRules::eAccountAction;
using AccountRules::eManageDenial;

namespace {
	// With no config every permission is at its default level (accounts_ban 4, characters_view 1, self_items 9 ...)
	std::shared_ptr<ApiKeys::Scope> Scope(std::set<std::string> permissions, bool readOnly = false) {
		auto scope = std::make_shared<ApiKeys::Scope>();
		scope->keyId = 7;
		scope->name = "bot";
		scope->permissions = std::move(permissions);
		scope->readOnly = readOnly;
		return scope;
	}

	std::shared_ptr<ApiKeys::Scope> AllScope() {
		auto scope = Scope({});
		scope->allPermissions = true;
		return scope;
	}
}

TEST(ApiKeyScopeTests, PermissionListsRoundTrip) {
	bool all = false;
	std::set<std::string> permissions;
	ApiKeys::ParsePermissions("chat_view,players_view", all, permissions);
	EXPECT_FALSE(all);
	EXPECT_EQ(permissions, (std::set<std::string>{ "chat_view", "players_view" }));
	EXPECT_EQ(ApiKeys::JoinPermissions(false, permissions), "chat_view,players_view");
	ApiKeys::ParsePermissions("*", all, permissions);
	EXPECT_TRUE(all);
	EXPECT_TRUE(permissions.empty());
	EXPECT_EQ(ApiKeys::JoinPermissions(true, {}), "*");
	ApiKeys::ParsePermissions("", all, permissions);
	EXPECT_FALSE(all);
	EXPECT_TRUE(permissions.empty());
}

TEST(ApiKeyScopeTests, AddressAndPathRestrictions) {
	const auto ips = ApiKeys::SplitList(" 10.0.0., 192.168.1.5 ,,::1");
	ASSERT_EQ(ips.size(), 3u);
	EXPECT_TRUE(ApiKeys::AddressAllowed(ips, "10.0.0.44"));
	EXPECT_TRUE(ApiKeys::AddressAllowed(ips, "192.168.1.5"));
	EXPECT_TRUE(ApiKeys::AddressAllowed(ips, "::1"));
	EXPECT_FALSE(ApiKeys::AddressAllowed(ips, "192.168.1.50")); // exact entries don't match as prefixes
	EXPECT_FALSE(ApiKeys::AddressAllowed(ips, "10.0.1.1"));
	EXPECT_TRUE(ApiKeys::AddressAllowed({}, "8.8.8.8"));

	const auto paths = ApiKeys::SplitList("/api/chat,/api/players");
	EXPECT_TRUE(ApiKeys::PathAllowed(paths, "/api/chat/send"));
	EXPECT_FALSE(ApiKeys::PathAllowed(paths, "/api/accounts/2"));
	EXPECT_TRUE(ApiKeys::PathAllowed({}, "/api/accounts/2"));
}

TEST(ApiKeyPermissionTests, KeyIsOwnerPermissionsIntersectScope) {
	const auto key = Scope({ "accounts_ban", "characters_view" });
	// The owner has both: the key has exactly its scope
	EXPECT_TRUE(Permissions::Allowed(5, "accounts_ban", key.get()));
	EXPECT_TRUE(Permissions::Allowed(5, "characters_view", key.get()));
	EXPECT_FALSE(Permissions::Allowed(5, "accounts_mute", key.get())); // the owner may, the key may not
	EXPECT_TRUE(Permissions::Allowed(5, "accounts_mute", nullptr));    // a browser session may
	// The owner is demoted: the key loses what the owner lost, although its scope still names it
	EXPECT_FALSE(Permissions::Allowed(3, "accounts_ban", key.get()));
	EXPECT_TRUE(Permissions::Allowed(3, "characters_view", key.get()));
	// Demoted to a player: nothing staff-only is left
	EXPECT_FALSE(Permissions::Allowed(0, "characters_view", key.get()));
	// Unknown permissions never pass, scope or not
	EXPECT_FALSE(Permissions::Allowed(9, "no_such_permission", AllScope().get()));
}

TEST(ApiKeyPermissionTests, AllPermissionsKeyFollowsOwner) {
	const auto key = AllScope();
	EXPECT_TRUE(Permissions::Allowed(4, "accounts_ban", key.get()));
	EXPECT_FALSE(Permissions::Allowed(3, "accounts_ban", key.get()));
	const auto can = Permissions::ForLevel(4, key.get());
	EXPECT_EQ(can, Permissions::ForLevel(4));
}

TEST(ApiKeyPermissionTests, ForLevelIsMasked) {
	const auto key = Scope({ "accounts_view" });
	const auto can = Permissions::ForLevel(9, key.get());
	EXPECT_TRUE(can["accounts_view"].get<bool>());
	EXPECT_FALSE(can["accounts_ban"].get<bool>());
	EXPECT_FALSE(can["own_characters"].get<bool>());
}

TEST(ApiKeyPermissionTests, CharacterViewNeedsScope) {
	// Own characters with own_characters in scope, others' with characters_view
	EXPECT_TRUE(Permissions::CanViewCharacter(0, 5, 5, Scope({ "own_characters" }).get()));
	EXPECT_FALSE(Permissions::CanViewCharacter(0, 5, 5, Scope({ "leaderboards_view" }).get()));
	EXPECT_FALSE(Permissions::CanViewCharacter(9, 5, 6, Scope({ "own_characters" }).get()));
	EXPECT_TRUE(Permissions::CanViewCharacter(9, 5, 6, Scope({ "characters_view" }).get()));
	EXPECT_FALSE(Permissions::CanViewCharacter(0, 5, 6, Scope({ "characters_view" }).get())); // the owner can't
}

TEST(ApiKeyPermissionTests, CreatorCantGrantWhatTheyLack) {
	EXPECT_TRUE(Permissions::NotGrantable(5, { "accounts_ban", "chat_view" }).empty());
	EXPECT_EQ(Permissions::NotGrantable(3, { "accounts_ban", "characters_view" }), (std::set<std::string>{ "accounts_ban" }));
	EXPECT_EQ(Permissions::NotGrantable(9, { "made_up" }), (std::set<std::string>{ "made_up" }));
	EXPECT_EQ(Permissions::NotGrantable(0, { "own_characters", "characters_view" }), (std::set<std::string>{ "characters_view" }));
}

TEST(ApiKeyPermissionTests, SelfAndRankRulesNarrowToo) {
	constexpr uint32_t OWNER = 5, OTHER = 6;
	// GM 9 needs neither self_* nor manage_equal_rank; a key of theirs needs them in its scope
	EXPECT_EQ(AccountRules::ManageDenialNow(9, OWNER, 9, OWNER, eAccountAction::ITEMS, nullptr), eManageDenial::NONE);
	EXPECT_EQ(AccountRules::ManageDenialNow(9, OWNER, 9, OWNER, eAccountAction::ITEMS, Scope({ "characters_edit" }).get()), eManageDenial::SELF);
	EXPECT_EQ(AccountRules::ManageDenialNow(9, OWNER, 9, OWNER, eAccountAction::ITEMS, Scope({ "self_items" }).get()), eManageDenial::NONE);
	EXPECT_EQ(AccountRules::ManageDenialNow(9, OWNER, 9, OTHER, eAccountAction::MODERATION, Scope({}).get()), eManageDenial::EQUAL_RANK);
	EXPECT_EQ(AccountRules::ManageDenialNow(9, OWNER, 9, OTHER, eAccountAction::MODERATION, Scope({ "manage_equal_rank" }).get()), eManageDenial::NONE);
	EXPECT_EQ(AccountRules::ManageDenialNow(9, OWNER, 3, OTHER, eAccountAction::MODERATION, Scope({}).get()), eManageDenial::NONE);
	// The owner's own rules always come first: no scope lets a key act above its owner
	EXPECT_EQ(AccountRules::ManageDenialNow(4, OWNER, 5, OTHER, eAccountAction::TOOLS, AllScope().get()), eManageDenial::HIGHER_RANK);
	// self_tools defaults to GM 1, so a GM 4 may kick themselves; their key only with self_tools in scope
	EXPECT_EQ(AccountRules::ManageDenialNow(4, OWNER, 4, OWNER, eAccountAction::TOOLS, nullptr), eManageDenial::NONE);
	EXPECT_EQ(AccountRules::ManageDenialNow(4, OWNER, 4, OWNER, eAccountAction::TOOLS, Scope({ "accounts_kick" }).get()), eManageDenial::SELF);
	// self_items defaults to GM 9: in a GM 4's scope it still doesn't let them
	EXPECT_EQ(AccountRules::ManageDenialNow(4, OWNER, 4, OWNER, eAccountAction::ITEMS, Scope({ "self_items" }).get()), eManageDenial::SELF);
	// An owner demoted below the target: the owner's rule refuses, whatever the scope
	EXPECT_EQ(AccountRules::ManageDenialNow(2, OWNER, 3, OTHER, eAccountAction::TOOLS, AllScope().get()), eManageDenial::HIGHER_RANK);
}

class ApiKeyMiddlewareTest : public ::testing::Test {
protected:
	HTTPContext context;
	HTTPReply reply;

	void WithKey(uint8_t ownerLevel, std::shared_ptr<ApiKeys::Scope> scope, const std::string& method = "GET") {
		context.method = method;
		context.path = "/api/something";
		context.isAuthenticated = true;
		context.authenticatedUser = "owner";
		context.accountId = 5;
		context.gmLevel = ownerLevel;
		context.userData["auth_source"] = "header";
		context.apiKey = std::move(scope);
	}

	static RequireAuthMiddleware PermRoute(const std::string& key) {
		return RequireAuthMiddleware(std::function<uint8_t()>([key] { return Permissions::Level(key); }), key);
	}
};

TEST_F(ApiKeyMiddlewareTest, PermissionRouteNeedsScope) {
	WithKey(9, Scope({ "chat_view" }));
	EXPECT_TRUE(PermRoute("chat_view").Process(context, reply));
	EXPECT_FALSE(PermRoute("accounts_ban").Process(context, reply));
	EXPECT_EQ(reply.status, eHTTPStatusCode::FORBIDDEN);
	EXPECT_NE(reply.message.find("accounts_ban"), std::string::npos);
}

TEST_F(ApiKeyMiddlewareTest, DemotedOwnerNarrowsKey) {
	WithKey(4, Scope({ "accounts_ban" }));
	EXPECT_TRUE(PermRoute("accounts_ban").Process(context, reply));
	context.gmLevel = 3; // looked up again on the next request
	EXPECT_FALSE(PermRoute("accounts_ban").Process(context, reply));
	EXPECT_EQ(reply.status, eHTTPStatusCode::FORBIDDEN);
}

TEST_F(ApiKeyMiddlewareTest, ReadOnlyKeysOnlyRead) {
	WithKey(9, Scope({ "chat_view" }, true), "POST");
	EXPECT_FALSE(PermRoute("chat_view").Process(context, reply));
	EXPECT_EQ(reply.status, eHTTPStatusCode::FORBIDDEN);
	// A POST that only reads (a DataTables query) is fine
	auto reads = PermRoute("chat_view");
	reads.SetReadsOnly();
	reply = {};
	EXPECT_TRUE(reads.Process(context, reply));
	context.method = "GET";
	EXPECT_TRUE(PermRoute("chat_view").Process(context, reply));
}

TEST_F(ApiKeyMiddlewareTest, LevelOnlyRoutesNeedAllPermissions) {
	WithKey(9, Scope({ "chat_view" }));
	EXPECT_TRUE(RequireAuthMiddleware(0).Process(context, reply));  // level 0: the handler checks its own permissions
	EXPECT_FALSE(RequireAuthMiddleware(1).Process(context, reply));
	WithKey(9, AllScope());
	EXPECT_TRUE(RequireAuthMiddleware(1).Process(context, reply));
	WithKey(0, AllScope());
	EXPECT_FALSE(RequireAuthMiddleware(1).Process(context, reply)); // never more than the owner
}

TEST_F(ApiKeyMiddlewareTest, DeniedHookIsTold) {
	std::string told;
	RequireAuthMiddleware::SetApiKeyDeniedHook([&](const HTTPContext&, const std::string& reason) { told = reason; });
	WithKey(9, Scope({}));
	EXPECT_FALSE(PermRoute("chat_view").Process(context, reply));
	EXPECT_NE(told.find("chat_view"), std::string::npos);
	RequireAuthMiddleware::SetApiKeyDeniedHook(nullptr);
}

TEST(ApiKeyLimiterTests, BurstThenSteadyRate) {
	ApiKeyLimiter limiter;
	const auto start = ApiKeyLimiter::Clock::now();
	for (int i = 0; i < 60; ++i) EXPECT_TRUE(limiter.Check(1, 60, 0, 100, 1000, 0, 0, start).allowed) << i;
	const auto refused = limiter.Check(1, 60, 0, 100, 1000, 0, 0, start);
	EXPECT_FALSE(refused.allowed);
	EXPECT_FALSE(refused.quotaExceeded);
	EXPECT_EQ(refused.limit, 60u);
	EXPECT_EQ(refused.remaining, 0u);
	EXPECT_GE(refused.retryAfterSeconds, 1u);
	// One a second comes back at 60 a minute
	EXPECT_TRUE(limiter.Check(1, 60, 0, 100, 1000, 0, 0, start + std::chrono::milliseconds(1001)).allowed);
	EXPECT_FALSE(limiter.Check(1, 60, 0, 100, 1000, 0, 0, start + std::chrono::milliseconds(1002)).allowed);
	// Keys are separate
	EXPECT_TRUE(limiter.Check(2, 60, 0, 100, 1000, 0, 0, start).allowed);
	// A full minute later the whole allowance is back, no more
	const auto later = start + std::chrono::minutes(5);
	for (int i = 0; i < 60; ++i) EXPECT_TRUE(limiter.Check(1, 60, 0, 100, 1000, 0, 0, later).allowed);
	EXPECT_FALSE(limiter.Check(1, 60, 0, 100, 1000, 0, 0, later).allowed);
}

TEST(ApiKeyLimiterTests, DailyQuota) {
	ApiKeyLimiter limiter;
	const auto now = ApiKeyLimiter::Clock::now();
	// 3 were already used today before a restart
	auto decision = limiter.Check(1, 1000, 5, 100, 3600, 100, 3, now);
	EXPECT_TRUE(decision.allowed);
	EXPECT_EQ(decision.dayCount, 4u);
	EXPECT_EQ(decision.quotaRemaining, 1u);
	EXPECT_TRUE(limiter.Check(1, 1000, 5, 100, 3600, 100, 3, now).allowed);
	decision = limiter.Check(1, 1000, 5, 100, 3600, 100, 3, now);
	EXPECT_FALSE(decision.allowed);
	EXPECT_TRUE(decision.quotaExceeded);
	EXPECT_EQ(decision.retryAfterSeconds, 3600u);
	// The next UTC day starts over
	decision = limiter.Check(1, 1000, 5, 101, 86400, 100, 3, now);
	EXPECT_TRUE(decision.allowed);
	EXPECT_EQ(decision.dayCount, 1u);
	// A stored count from an earlier day doesn't count
	EXPECT_EQ(limiter.Check(2, 1000, 5, 101, 86400, 100, 5, now).dayCount, 1u);
}

TEST(ApiKeyLimiterTests, ForgetAndPrune) {
	ApiKeyLimiter limiter;
	const auto now = ApiKeyLimiter::Clock::now();
	EXPECT_TRUE(limiter.Check(1, 1, 0, 100, 1000, 0, 0, now).allowed);
	EXPECT_FALSE(limiter.Check(1, 1, 0, 100, 1000, 0, 0, now).allowed);
	limiter.Forget(1);
	EXPECT_TRUE(limiter.Check(1, 1, 0, 100, 1000, 0, 0, now).allowed);
	limiter.Check(2, 1, 0, 100, 1000, 0, 0, now + std::chrono::hours(1));
	limiter.Prune(now + std::chrono::hours(1), std::chrono::minutes(30));
	EXPECT_EQ(limiter.Size(), 1u);
}
