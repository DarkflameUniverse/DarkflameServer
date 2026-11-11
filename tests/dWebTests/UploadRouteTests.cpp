#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>

#include <curl/curl.h>
#include <unistd.h>

#include "Game.h"
#include "Web.h"
#include "eHTTPMethod.h"

/**
 * Upload routes (Web::RegisterUploadRoute) over a real connection: the test starts the web server on a loopback
 * port of its own, and a second thread sends requests to it with libcurl while this one polls the server, as a
 * server's main loop does. Skipped when no port can be had.
 */
namespace {
	// ctest runs each test in a process of its own, several at once: each takes a port and a folder of its own
	uint32_t g_Port = 0;
	constexpr uint64_t MAX_BYTES = 8 * 1024 * 1024;

	struct Seen {
		std::atomic<int> middlewareCalls{};
		std::atomic<int> handlerCalls{};
		std::string bodyFile;
		uint64_t bodyBytes{};
		std::string body;
		std::string fieldHeader;
	};
	Seen g_Seen;
	std::filesystem::path g_Dir;
	bool g_Started = false;

	// Refuses a request that says so, before its body
	class DenyOnHeader : public IHTTPMiddleware {
	public:
		bool Process(HTTPContext& context, HTTPReply& reply) override {
			g_Seen.middlewareCalls++;
			if (context.GetHeader("X-Deny").empty()) return true;
			reply.status = eHTTPStatusCode::FORBIDDEN;
			reply.message = "{\"error\":\"denied\"}";
			return false;
		}
		std::string GetName() const override { return "DenyOnHeader"; }
	};

	struct Response {
		long status{};
		std::string body;
		CURLcode code{};
	};

	size_t Collect(char* data, size_t size, size_t count, void* out) {
		static_cast<std::string*>(out)->append(data, size * count);
		return size * count;
	}

	// Sends the request on another thread while this one runs the server
	Response Post(const std::string& body, const std::vector<std::string>& headers, bool chunked = false) {
		Response response;
		std::atomic<bool> done{};
		std::thread client([&] {
			CURL* curl = curl_easy_init();
			curl_slist* list = nullptr;
			for (const auto& header : headers) list = curl_slist_append(list, header.c_str());
			list = curl_slist_append(list, "Expect:"); // send the body at once
			if (chunked) list = curl_slist_append(list, "Transfer-Encoding: chunked");
			curl_easy_setopt(curl, CURLOPT_URL, ("http://127.0.0.1:" + std::to_string(g_Port) + "/upload/7").c_str());
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
			if (!chunked) curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Collect);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
			curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");
			response.code = curl_easy_perform(curl);
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
			curl_slist_free_all(list);
			curl_easy_cleanup(curl);
			done = true;
		});
		while (!done) Game::web.ReceiveRequests(5);
		client.join();
		return response;
	}

	class UploadRouteTests : public ::testing::Test {
	protected:
		static void SetUpTestSuite() {
			g_Dir = std::filesystem::temp_directory_path() / ("dlu-upload-route-tests-" + std::to_string(getpid()));
			std::filesystem::remove_all(g_Dir);
			std::filesystem::create_directories(g_Dir);
			for (uint32_t attempt = 0; attempt < 20 && !g_Started; ++attempt) {
				g_Port = 38000 + (static_cast<uint32_t>(getpid()) * 7 + attempt * 131) % 2000;
				g_Started = Game::web.Startup("127.0.0.1", g_Port);
			}
			if (!g_Started) return;
			Game::web.RegisterUploadRoute({ .path = "/upload/:id", .method = eHTTPMethod::POST,
				.middleware = { std::make_shared<DenyOnHeader>() },
				.maxBytes = [] { return MAX_BYTES; },
				.spoolFile = [](const HTTPContext&) { return (g_Dir / "body").string(); },
				.handle = [](HTTPReply& reply, const HTTPContext& context) {
					g_Seen.handlerCalls++;
					g_Seen.bodyFile = context.bodyFile;
					g_Seen.bodyBytes = context.bodyBytes;
					g_Seen.fieldHeader = context.GetHeader("X-Field");
					std::ifstream in(context.bodyFile, std::ios::binary);
					g_Seen.body.assign(std::istreambuf_iterator<char>(in), {});
					reply.status = eHTTPStatusCode::OK;
					reply.message = "{\"bytes\":" + std::to_string(context.bodyBytes) + ",\"inMemory\":" + std::to_string(context.body.size()) + "}";
				} });
		}
		static void TearDownTestSuite() {
			if (g_Started) Game::web.Shutdown();
			std::filesystem::remove_all(g_Dir);
		}
		void SetUp() override {
			if (!g_Started) GTEST_SKIP() << "No loopback port could be had";
			g_Seen.middlewareCalls = 0;
			g_Seen.handlerCalls = 0;
			g_Seen.body.clear();
		}
	};
}

// Larger than the web server's in-memory limit for a request: it arrives whole, in the file
TEST_F(UploadRouteTests, ALargeBodyArrivesInAFile) {
	std::string body(5 * 1024 * 1024, '\0');
	for (size_t i = 0; i < body.size(); ++i) body[i] = static_cast<char>((i * 31 + i / 977) & 0xFF);
	const auto response = Post(body, { "X-Field: yes", "Content-Type: application/octet-stream" });
	ASSERT_EQ(response.code, CURLE_OK);
	EXPECT_EQ(response.status, 200);
	EXPECT_EQ(response.body, "{\"bytes\":" + std::to_string(body.size()) + ",\"inMemory\":0}");
	EXPECT_EQ(g_Seen.handlerCalls, 1);
	EXPECT_EQ(g_Seen.middlewareCalls, 1); // once, however many reads the body took
	EXPECT_EQ(g_Seen.bodyBytes, body.size());
	EXPECT_EQ(g_Seen.fieldHeader, "yes");
	EXPECT_TRUE(g_Seen.body == body);
}

TEST_F(UploadRouteTests, AnEmptyBodyIsAnUpload) {
	const auto response = Post("", {});
	ASSERT_EQ(response.code, CURLE_OK);
	EXPECT_EQ(response.status, 200);
	EXPECT_EQ(g_Seen.handlerCalls, 1);
	EXPECT_EQ(g_Seen.bodyBytes, 0u);
}

// What the middleware refuses is answered before the body: the handler never runs and no file is kept
TEST_F(UploadRouteTests, RefusedBeforeTheBodyIsTaken) {
	std::filesystem::remove(g_Dir / "body");
	const auto response = Post(std::string(2 * 1024 * 1024, 'x'), { "X-Deny: 1" });
	EXPECT_EQ(response.status, 403);
	EXPECT_EQ(response.body, "{\"error\":\"denied\"}");
	EXPECT_EQ(g_Seen.handlerCalls, 0);
	EXPECT_FALSE(std::filesystem::exists(g_Dir / "body"));
}

TEST_F(UploadRouteTests, TooLargeIsRefusedByItsLength) {
	std::filesystem::remove(g_Dir / "body");
	const auto response = Post(std::string(MAX_BYTES + 1, 'x'), {});
	EXPECT_EQ(response.status, 413);
	EXPECT_EQ(g_Seen.handlerCalls, 0);
	EXPECT_FALSE(std::filesystem::exists(g_Dir / "body"));
}

TEST_F(UploadRouteTests, ALengthIsRequired) {
	const auto response = Post("hello", {}, true);
	EXPECT_EQ(response.status, 411);
	EXPECT_EQ(g_Seen.handlerCalls, 0);
}
