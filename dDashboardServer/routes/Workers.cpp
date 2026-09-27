#include "Workers.h"

#include <algorithm>
#include <exception>
#include <thread>

#include "RouteUtils.h"
#include "Web.h"

#include "Game.h"
#include "Logger.h"
#include "dConfig.h"

namespace {
	WorkerPool g_Pool;
}

namespace Workers {
	void Start() {
		auto threads = Game::config ? Game::config->GetValue<uint32_t>("scenery_workers", 0) : 0;
		if (threads == 0) threads = static_cast<uint32_t>(WorkerPool::DefaultThreads(std::thread::hardware_concurrency()));
		threads = std::clamp<uint32_t>(threads, 2, 16);
		// Work ahead of time (converting a zone's models) never takes more than half of the threads besides the fast lane
		g_Pool.Start(threads, std::max<size_t>(1, (threads - 1) / 2));
		LOG("Dashboard worker threads: %u", threads);
	}

	void Stop() {
		g_Pool.Stop();
	}

	WorkerPool& Pool() {
		return g_Pool;
	}

	void Reply(HTTPReply& reply, const HTTPContext& context, bool ready, std::function<void(HTTPReply&)> fill, WorkerPool::ePriority priority) {
		if (ready || !g_Pool.Running()) return fill(reply);
		const auto deferred = Web::Defer(reply, context);
		const auto path = context.path;
		g_Pool.Submit(priority, [deferred, path, fill = std::move(fill)] {
			if (deferred.Cancelled()) return;
			HTTPReply out;
			try {
				fill(out);
			} catch (const std::exception& ex) {
				LOG("Error handling GET %s: %s", path.c_str(), ex.what());
				out = HTTPReply{};
				RouteUtils::JsonError(out, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Internal server error");
			}
			deferred.Send(std::move(out));
		});
	}
}
