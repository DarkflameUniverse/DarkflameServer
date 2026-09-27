#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "DeferredReply.h"
#include "Web.h"
#include "eHTTPMethod.h"

namespace {
	HTTPReply Text(const std::string& body) {
		HTTPReply reply;
		reply.status = eHTTPStatusCode::OK;
		reply.contentType = eContentType::TEXT_PLAIN;
		reply.message = body;
		return reply;
	}
}

TEST(DeferredQueueTests, AnswerReachesItsConnection) {
	DeferredQueue queue;
	const DeferredReply deferred(queue.Begin(7));
	queue.SetReplyOptions(7, { "Set-Cookie: a=b" }, true);
	EXPECT_TRUE(queue.Drain().empty());
	EXPECT_EQ(queue.Pending(), 1u);

	auto reply = Text("done");
	reply.headers.push_back("Cache-Control: no-store");
	deferred.Send(reply);
	const auto finished = queue.Drain();
	ASSERT_EQ(finished.size(), 1u);
	EXPECT_EQ(finished[0].connection, 7u);
	EXPECT_EQ(finished[0].reply.message, "done");
	EXPECT_TRUE(finished[0].close);
	// Middleware's headers first, then the handler's own
	ASSERT_EQ(finished[0].reply.headers.size(), 2u);
	EXPECT_EQ(finished[0].reply.headers[0], "Set-Cookie: a=b");
	EXPECT_EQ(finished[0].reply.headers[1], "Cache-Control: no-store");
	EXPECT_EQ(queue.Pending(), 0u);
}

TEST(DeferredQueueTests, OnlyTheFirstAnswerCounts) {
	DeferredQueue queue;
	const DeferredReply deferred(queue.Begin(1));
	deferred.Send(Text("first"));
	deferred.Send(Text("second"));
	const auto finished = queue.Drain();
	ASSERT_EQ(finished.size(), 1u);
	EXPECT_EQ(finished[0].reply.message, "first");
	EXPECT_TRUE(queue.Drain().empty());
}

TEST(DeferredQueueTests, ClosedConnectionCancelsAndDropsTheAnswer) {
	DeferredQueue queue;
	const DeferredReply deferred(queue.Begin(3));
	EXPECT_FALSE(deferred.Cancelled());
	queue.Close(3);
	EXPECT_TRUE(deferred.Cancelled());
	deferred.Send(Text("late"));
	EXPECT_TRUE(queue.Drain().empty());
	EXPECT_EQ(queue.Pending(), 0u);
}

TEST(DeferredQueueTests, LateAnswerNeverReachesALaterRequest) {
	DeferredQueue queue;
	const DeferredReply first(queue.Begin(5));
	// The same connection starts another deferred request (the first was given up on)
	const DeferredReply second(queue.Begin(5));
	EXPECT_TRUE(first.Cancelled());
	// Pushed straight to the queue, as a worker that checked before the cancel would
	queue.Push(DeferredState{ .connection = 5, .id = 1 }, Text("stale"));
	second.Send(Text("fresh"));
	const auto finished = queue.Drain();
	ASSERT_EQ(finished.size(), 1u);
	EXPECT_EQ(finished[0].reply.message, "fresh");
}

TEST(DeferredQueueTests, CancelAllCancelsEverything) {
	DeferredQueue queue;
	const DeferredReply a(queue.Begin(1)), b(queue.Begin(2));
	queue.CancelAll();
	EXPECT_TRUE(a.Cancelled());
	EXPECT_TRUE(b.Cancelled());
	EXPECT_EQ(queue.Pending(), 0u);
}

TEST(DeferredQueueTests, AnswersFromManyThreads) {
	DeferredQueue queue;
	constexpr unsigned long COUNT = 64;
	std::vector<DeferredReply> replies;
	for (unsigned long i = 1; i <= COUNT; i++) replies.emplace_back(queue.Begin(i));
	std::vector<std::thread> threads;
	for (unsigned long i = 0; i < COUNT; i++) threads.emplace_back([&replies, i] { replies[i].Send(Text(std::to_string(i + 1))); });
	for (auto& thread : threads) thread.join();
	const auto finished = queue.Drain();
	ASSERT_EQ(finished.size(), COUNT);
	for (const auto& f : finished) EXPECT_EQ(f.reply.message, std::to_string(f.connection));
}

// The real web server: a deferred request doesn't hold up others, its answer arrives later on the same
// connection (which then takes its next request), and a client that leaves first is handled safely.
namespace {
	struct Client {
		std::vector<std::string> bodies;
		bool closed{};
	};

	void ClientEvents(mg_connection* connection, int event, void* data) {
		auto* client = static_cast<Client*>(connection->fn_data);
		if (event == MG_EV_HTTP_MSG) {
			const auto* message = static_cast<mg_http_message*>(data);
			client->bodies.emplace_back(message->body.buf, message->body.len);
		} else if (event == MG_EV_CLOSE) {
			client->closed = true;
		}
	}

	void Get(mg_connection* connection, const std::string& path) {
		mg_printf(connection, "GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n", path.c_str());
	}
}

TEST(DeferredWebTests, DeferredRequestsDontHoldUpOthers) {
	constexpr uint32_t PORT = 38631;
	ASSERT_TRUE(Game::web.Startup("127.0.0.1", PORT));

	std::atomic<bool> release{};
	std::vector<std::thread> workers;
	std::vector<DeferredReply> handedOff;
	Game::web.RegisterHTTPRoute({ .path = "/slow", .method = eHTTPMethod::GET, .middleware = {}, .handle = [&](HTTPReply& reply, const HTTPContext& context) {
		auto deferred = Web::Defer(reply, context);
		handedOff.push_back(deferred);
		workers.emplace_back([deferred, &release] {
			while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(1));
			deferred.Send(Text("slow"));
		});
	} });
	Game::web.RegisterHTTPRoute({ .path = "/fast", .method = eHTTPMethod::GET, .middleware = {}, .handle = [](HTTPReply& reply, const HTTPContext&) {
		reply = Text("fast");
	} });

	mg_mgr clients;
	mg_mgr_init(&clients);
	const auto poll = [&](const std::function<bool()>& done) {
		const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!done() && std::chrono::steady_clock::now() < until) {
			Game::web.ReceiveRequests(1);
			mg_mgr_poll(&clients, 1);
		}
		return done();
	};
	const std::string url = "http://127.0.0.1:" + std::to_string(PORT);

	Client slow, fast, leaving;
	auto* slowConnection = mg_http_connect(&clients, url.c_str(), ClientEvents, &slow);
	auto* fastConnection = mg_http_connect(&clients, url.c_str(), ClientEvents, &fast);
	auto* leavingConnection = mg_http_connect(&clients, url.c_str(), ClientEvents, &leaving);
	ASSERT_NE(slowConnection, nullptr);
	ASSERT_NE(fastConnection, nullptr);
	ASSERT_NE(leavingConnection, nullptr);
	Get(slowConnection, "/slow");
	Get(leavingConnection, "/slow");
	ASSERT_TRUE(poll([&] { return handedOff.size() == 2; }));

	// Answered while the slow ones still wait
	Get(fastConnection, "/fast");
	ASSERT_TRUE(poll([&] { return !fast.bodies.empty(); }));
	EXPECT_EQ(fast.bodies[0], "fast");
	EXPECT_TRUE(slow.bodies.empty());
	EXPECT_EQ(Game::web.PendingDeferred(), 2u);

	// One client leaves before its answer: the work sees it's cancelled, and the answer is dropped
	leavingConnection->is_closing = 1;
	ASSERT_TRUE(poll([&] { return handedOff[0].Cancelled() || handedOff[1].Cancelled(); }));
	EXPECT_EQ(Game::web.PendingDeferred(), 1u);

	release = true;
	ASSERT_TRUE(poll([&] { return !slow.bodies.empty(); }));
	EXPECT_EQ(slow.bodies[0], "slow");
	EXPECT_EQ(Game::web.PendingDeferred(), 0u);
	EXPECT_TRUE(leaving.bodies.empty());

	// The connection takes its next request after the deferred answer
	Get(slowConnection, "/fast");
	ASSERT_TRUE(poll([&] { return slow.bodies.size() == 2; }));
	EXPECT_EQ(slow.bodies[1], "fast");

	for (auto& worker : workers) worker.join();
	mg_mgr_free(&clients);
}
