#pragma once

struct MessageCaptureData;

/**
 * The game message inspector (developer tool): staff capture the game messages one online player sends and
 * receives, watch them live and look at them again later. The world holding the player captures and sends batches
 * through master (MessageCapture.h); the dashboard numbers them, pushes them to the message_capture socket topic and
 * saves every capture with its messages' raw bytes in the database (IMessageCaptures) while it runs, so a capture
 * outlives the dashboard. Captures follow the player across zone changes and always end at their time limit. The
 * Message capture pruning task deletes saved captures past inspector_session_days or over inspector_max_mb.
 */
namespace Inspector {
	// After the database is up, before Scheduler::Initialize: registers the pruning task and picks up the captures that
	// were running when the dashboard stopped (still within their time: carried on; otherwise marked ended)
	void Initialize();

	void RegisterRoutes();

	// MESSAGE_CAPTURE_DATA from a world, via master
	void HandleData(const MessageCaptureData& data);

	// Main loop: push new messages to browsers, save them, find the player again after a zone change, end captures on time
	void Update();

	// Save what hasn't been yet (running captures stay unfinished, to be carried on at the next start)
	void Shutdown();
}
