#pragma once

#include <functional>

#include "WorkerPool.h"

struct HTTPReply;
struct HTTPContext;

/**
 * The dashboard's worker threads (one WorkerPool, scenery_workers threads) for slow work routes would otherwise do
 * on the web server's one thread: converting the client's models, building a zone's terrain and scene data the
 * first time it is viewed, converting textures with ImageMagick. What they run must be thread safe: the zone data
 * builders keep their results in OnceCaches and read the client's files through ClientAssets.
 */
namespace Workers {
	// Start the threads (scenery_workers: 0 picks WorkerPool::DefaultThreads)
	void Start();

	// Stop them; queued work is dropped (answer deferred requests before Web::Shutdown)
	void Stop();

	WorkerPool& Pool();

	/**
	 * Answer a request with what `fill` writes to the reply: right away when `ready` (what it needs is built, so it's
	 * quick), else from a worker thread (Web::Defer). `fill` runs on either thread, so it must be thread safe and must
	 * not keep a reference to the request.
	 */
	void Reply(HTTPReply& reply, const HTTPContext& context, bool ready, std::function<void(HTTPReply&)> fill,
		WorkerPool::ePriority priority = WorkerPool::ePriority::NORMAL);
}
