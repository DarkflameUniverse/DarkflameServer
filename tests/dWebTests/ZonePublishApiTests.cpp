#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "ApiKeyScope.h"
#include "MultipartFile.h"
#include "Permissions.h"
#include "ZonePublishAuth.h"
#include "ZonePublishRoutes.h"

using namespace ZonePublishAuth;

namespace {
	ApiKeys::Scope Key(std::set<std::string> permissions, bool readOnly = false, bool all = false) {
		ApiKeys::Scope scope;
		scope.keyId = 7;
		scope.name = "editor";
		scope.permissions = std::move(permissions);
		scope.readOnly = readOnly;
		scope.allPermissions = all;
		return scope;
	}

	// A request that passes; each test breaks one thing
	Request Good(const ApiKeys::Scope& key) {
		Request request;
		request.transportAllowed = true;
		request.authenticated = true;
		request.key = &key;
		request.gmLevel = 9;
		request.requiredLevel = 9;
		request.changes = true;
		return request;
	}
}

TEST(ZonePublishAuthTests, AValidKeyWithTheScopeAtTheLevelPasses) {
	const auto key = Key({ SCOPE });
	EXPECT_EQ(Check(Good(key)), eDenial::NONE);
}

TEST(ZonePublishAuthTests, NoKeyIsUnauthorized) {
	const auto key = Key({ SCOPE });
	auto request = Good(key);
	request.authenticated = false;
	request.key = nullptr;
	EXPECT_EQ(Check(request), eDenial::NO_KEY);
	EXPECT_EQ(Status(eDenial::NO_KEY), eHTTPStatusCode::UNAUTHORIZED);
}

// A revoked or expired key never authenticates (ApiKeyService asks InUse on every request), so it is "no key"
TEST(ZonePublishAuthTests, RevokedAndExpiredKeysAreNotInUse) {
	EXPECT_TRUE(ApiKeys::InUse(0, 0, 1000));
	EXPECT_FALSE(ApiKeys::InUse(900, 0, 1000));   // revoked
	EXPECT_FALSE(ApiKeys::InUse(0, 1000, 1000));  // expired
	EXPECT_TRUE(ApiKeys::InUse(0, 1001, 1000));
	const auto key = Key({ SCOPE });
	auto request = Good(key);
	request.authenticated = false; // what a revoked key leaves behind
	EXPECT_EQ(Check(request), eDenial::NO_KEY);
}

TEST(ZonePublishAuthTests, ASessionIsNotAKey) {
	const auto key = Key({ SCOPE });
	auto request = Good(key);
	request.key = nullptr;
	EXPECT_EQ(Check(request), eDenial::NOT_A_KEY);
	EXPECT_EQ(Status(eDenial::NOT_A_KEY), eHTTPStatusCode::UNAUTHORIZED);
}

TEST(ZonePublishAuthTests, WrongScopeIsForbidden) {
	const auto other = Key({ "world_reload", "client_files" });
	EXPECT_EQ(Check(Good(other)), eDenial::SCOPE);
	EXPECT_EQ(Status(eDenial::SCOPE), eHTTPStatusCode::FORBIDDEN);
	// "All of the owner's permissions" is not the scope either: a key has to be made for zone files
	const auto all = Key({}, false, true);
	EXPECT_EQ(Check(Good(all)), eDenial::SCOPE);
}

TEST(ZonePublishAuthTests, GmLevelTooLowIsForbidden) {
	const auto key = Key({ SCOPE });
	auto request = Good(key);
	request.gmLevel = 8;
	EXPECT_EQ(Check(request), eDenial::GM_LEVEL);
	EXPECT_EQ(Status(eDenial::GM_LEVEL), eHTTPStatusCode::FORBIDDEN);
	request.requiredLevel = 8;
	EXPECT_EQ(Check(request), eDenial::NONE);
}

TEST(ZonePublishAuthTests, ReadOnlyKeysOnlyRead) {
	const auto key = Key({ SCOPE }, true);
	auto request = Good(key);
	EXPECT_EQ(Check(request), eDenial::READ_ONLY);
	request.changes = false;
	EXPECT_EQ(Check(request), eDenial::NONE);
}

TEST(ZonePublishAuthTests, PlainHttpFromAnotherMachineIsRefusedFirst) {
	const auto key = Key({ SCOPE });
	auto request = Good(key);
	request.transportAllowed = false;
	EXPECT_EQ(Check(request), eDenial::PLAIN_HTTP);
	EXPECT_EQ(Status(eDenial::PLAIN_HTTP), eHTTPStatusCode::FORBIDDEN);
	EXPECT_STREQ(Code(eDenial::PLAIN_HTTP), "https_required");
}

TEST(ZonePublishAuthTests, TransportIsHttpsOrLocal) {
	// No proxy: the dashboard speaks plain HTTP, so only this machine
	EXPECT_TRUE(TransportAllowed(false, "127.0.0.1", "", ""));
	EXPECT_TRUE(TransportAllowed(false, "::1", "", ""));
	EXPECT_FALSE(TransportAllowed(false, "192.168.1.20", "", ""));
	EXPECT_FALSE(TransportAllowed(false, "203.0.113.9", "", ""));
	// Headers anyone can send are not believed without a proxy
	EXPECT_FALSE(TransportAllowed(false, "203.0.113.9", "127.0.0.1", "https"));
	// Behind a proxy: what the proxy says about the request
	EXPECT_TRUE(TransportAllowed(true, "127.0.0.1", "203.0.113.9", "https"));
	EXPECT_FALSE(TransportAllowed(true, "127.0.0.1", "203.0.113.9", "http"));
	EXPECT_FALSE(TransportAllowed(true, "127.0.0.1", "203.0.113.9", ""));
	EXPECT_FALSE(TransportAllowed(true, "127.0.0.1", "203.0.113.9", "HTTPS, http"));
	// Not through the proxy: a caller on this machine, or nobody
	EXPECT_TRUE(TransportAllowed(true, "127.0.0.1", "", ""));
	EXPECT_FALSE(TransportAllowed(true, "10.0.0.5", "", ""));
	// Through a local proxy from this machine over plain HTTP
	EXPECT_TRUE(TransportAllowed(true, "127.0.0.1", "127.0.0.1", "http"));
	EXPECT_FALSE(TransportAllowed(true, "10.0.0.5", "127.0.0.1", "http"));
}

TEST(ZonePublishAuthTests, RefusalsAreLimitedPerAddress) {
	FailureLimiter limiter(3, 600);
	EXPECT_FALSE(limiter.Blocked("203.0.113.9", 1000));
	EXPECT_FALSE(limiter.RecordFailure("203.0.113.9", 1000));
	EXPECT_FALSE(limiter.RecordFailure("203.0.113.9", 1001));
	EXPECT_FALSE(limiter.Blocked("203.0.113.9", 1002));
	EXPECT_TRUE(limiter.RecordFailure("203.0.113.9", 1002)); // the one that blocks it
	EXPECT_TRUE(limiter.Blocked("203.0.113.9", 1003));
	EXPECT_FALSE(limiter.Blocked("198.51.100.1", 1003)); // another address is not
	// Refusals while blocked don't keep it blocked longer
	EXPECT_FALSE(limiter.RecordFailure("203.0.113.9", 1500));
	EXPECT_TRUE(limiter.Blocked("203.0.113.9", 1599));
	EXPECT_FALSE(limiter.Blocked("203.0.113.9", 1603));

	const auto key = Key({ SCOPE });
	auto request = Good(key);
	request.blocked = true;
	EXPECT_EQ(Check(request), eDenial::RATE_LIMITED);
	EXPECT_EQ(Status(eDenial::RATE_LIMITED), eHTTPStatusCode::TOO_MANY_REQUESTS);
}

TEST(ZonePublishAuthTests, TheScopeIsAPermissionKeysCanBeGiven) {
	ASSERT_NE(Permissions::Find(SCOPE), nullptr);
	EXPECT_EQ(Permissions::Find(SCOPE)->defaultLevel, 9);
	// Only an account that has it can make a key with it
	EXPECT_TRUE(Permissions::NotGrantable(9, { SCOPE }).empty());
	EXPECT_FALSE(Permissions::NotGrantable(8, { SCOPE }).empty());
}

TEST(ZonePublishProgressTests, AReloadIsDoneOnceMasterTookItAndNothingIsPending) {
	using namespace ZonePublishRoutes;
	const ReloadCounters before{ true, 2, 5, 10, 0, 0 };
	// Master has not handled the request yet
	EXPECT_FALSE(ProgressOf(before, before).done);
	// Taken: two instances, one still moving its players
	auto progress = ProgressOf(before, { true, 3, 7, 10, 0, 1 });
	EXPECT_FALSE(progress.done);
	EXPECT_EQ(progress.instances, 2u);
	progress = ProgressOf(before, { true, 3, 7, 14, 1, 0 });
	EXPECT_TRUE(progress.done);
	EXPECT_EQ(progress.playersMoved, 4u);
	EXPECT_EQ(progress.failed, 1u);
	// Every instance stopped: the zone is gone from master's status
	EXPECT_TRUE(ProgressOf(before, {}).done);
	// The zone went away and came back: its counts started over
	EXPECT_TRUE(ProgressOf(before, { true, 1, 1, 0, 0, 0 }).done);
}

namespace {
	struct MultipartFixture : ::testing::Test {
		std::filesystem::path dir;
		void SetUp() override {
			dir = std::filesystem::temp_directory_path() / ("dlu-multipart-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
			std::filesystem::remove_all(dir);
			std::filesystem::create_directories(dir);
		}
		void TearDown() override { std::filesystem::remove_all(dir); }
		MultipartFile::eError Split(const std::string& body, std::vector<MultipartFile::Part>& parts, MultipartFile::Limits limits = {}) {
			std::ofstream(dir / "body", std::ios::binary) << body;
			return MultipartFile::Split(dir / "body", "XYZ", dir, limits, parts);
		}
		static std::string Slurp(const std::filesystem::path& path) {
			std::ifstream in(path, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(in), {});
		}
		static std::string FilePart(const std::string& filename, const std::string& data) {
			return "--XYZ\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\nContent-Type: application/octet-stream\r\n\r\n" + data + "\r\n";
		}
		static std::string Field(const std::string& name, const std::string& value) {
			return "--XYZ\r\nContent-Disposition: form-data; name=\"" + name + "\"\r\n\r\n" + value + "\r\n";
		}
	};
	using MultipartFileTests = MultipartFixture;
}

TEST(MultipartBoundaryTests, ReadsTheBoundary) {
	EXPECT_EQ(MultipartFile::Boundary("multipart/form-data; boundary=----abc123"), "----abc123");
	EXPECT_EQ(MultipartFile::Boundary("Multipart/Form-Data; charset=x; boundary=\"a b\""), "a b");
	EXPECT_FALSE(MultipartFile::Boundary("application/json"));
	EXPECT_FALSE(MultipartFile::Boundary("multipart/form-data"));
	EXPECT_FALSE(MultipartFile::Boundary("multipart/form-data; boundary=" + std::string(71, 'a')));
}

TEST_F(MultipartFileTests, SplitsFilesAndFields) {
	// A file bigger than the reader's buffer, with bytes that look like the start of a boundary inside it
	std::string big(700 * 1024, 'x');
	big.replace(1000, 6, "\r\n--XY");
	big.replace(262139, 7, "\r\n--XY!"); // across the edge of what is read at once
	const std::string binary("a\0b\r\n--\r\n", 9);
	std::vector<MultipartFile::Part> parts;
	const auto error = Split(FilePart("zone/a.luz", big) + Field("reload", "false") + FilePart("zone/a.lvl", binary) + Field("note", "") + "--XYZ--\r\n", parts);
	ASSERT_EQ(error, MultipartFile::eError::NONE);
	ASSERT_EQ(parts.size(), 4u);
	EXPECT_TRUE(parts[0].isFile);
	EXPECT_EQ(parts[0].filename, "zone/a.luz");
	EXPECT_EQ(parts[0].bytes, big.size());
	EXPECT_EQ(Slurp(parts[0].file), big);
	EXPECT_FALSE(parts[1].isFile);
	EXPECT_EQ(parts[1].name, "reload");
	EXPECT_EQ(parts[1].value, "false");
	EXPECT_EQ(Slurp(parts[2].file), binary);
	EXPECT_EQ(parts[3].value, "");
}

// The name a sender gives a file is only text handed back: it is never where the bytes are written
TEST_F(MultipartFileTests, FileNamesAreNeverUsedAsPaths) {
	std::vector<MultipartFile::Part> parts;
	ASSERT_EQ(Split(FilePart("../../escape", "x") + FilePart("/etc/passwd", "y") + FilePart("a\\b", "z") + "--XYZ--", parts), MultipartFile::eError::NONE);
	ASSERT_EQ(parts.size(), 3u);
	EXPECT_EQ(parts[0].filename, "../../escape");
	EXPECT_EQ(parts[2].filename, "a\\b");
	for (const auto& part : parts) EXPECT_EQ(part.file.parent_path(), dir);
	EXPECT_FALSE(std::filesystem::exists(dir.parent_path().parent_path() / "escape"));
}

TEST_F(MultipartFileTests, LimitsAndDamage) {
	std::vector<MultipartFile::Part> parts;
	MultipartFile::Limits limits;
	limits.maxFileBytes = 10;
	ASSERT_EQ(Split(FilePart("a", std::string(500, 'x')) + FilePart("b", "ok") + "--XYZ--", parts, limits), MultipartFile::eError::NONE);
	EXPECT_TRUE(parts[0].tooLarge);
	EXPECT_EQ(parts[0].bytes, 500u);
	EXPECT_LE(std::filesystem::file_size(parts[0].file), 10u);
	EXPECT_FALSE(parts[1].tooLarge);

	parts.clear();
	limits = {};
	limits.maxParts = 2;
	EXPECT_EQ(Split(FilePart("a", "1") + FilePart("b", "2") + FilePart("c", "3") + "--XYZ--", parts, limits), MultipartFile::eError::TOO_MANY_PARTS);

	parts.clear();
	limits = {};
	limits.maxFieldBytes = 4;
	EXPECT_EQ(Split(Field("note", "longer than four") + "--XYZ--", parts, limits), MultipartFile::eError::FIELD_TOO_LARGE);

	parts.clear();
	EXPECT_EQ(Split(FilePart("a", "cut off"), parts), MultipartFile::eError::MALFORMED); // no closing boundary
	parts.clear();
	EXPECT_EQ(Split("not multipart at all", parts), MultipartFile::eError::MALFORMED);
	parts.clear();
	EXPECT_EQ(Split("--XYZ\r\nContent-Type: text/plain\r\n\r\nno disposition\r\n--XYZ--", parts), MultipartFile::eError::MALFORMED);
	parts.clear();
	EXPECT_EQ(Split("", parts), MultipartFile::eError::MALFORMED);
}
