#pragma once

#include <cstdint>
#include <filesystem>

struct MessageCaptureData;

/**
 * Packet captures and their replay on the dashboard (docs/CaptureReplay.md), next to the game message inspector
 * (Inspector.h) and under the same permission (dev_message_inspector).
 *
 * Staff arm a capture for an account (from its next login, or at once if it is online), one character, or
 * everything; every server records its part (PacketCapture.h) and sends batches through master. The dashboard keeps
 * each capture's session in message_capture_sessions (capture_kind 1) and its packets in a bundle file under
 * capture_dir (CaptureBundle.h), written once per batch, never per packet. Viewing decodes the saved bytes with the
 * server's packet structs (PacketDecoder.h); positions feed the World 3D replay; a capture exports as a portable
 * bundle for the capture tool's replay.
 */
namespace CaptureReplay {
	// After the database is up: captures that were running when the dashboard stopped are marked ended
	void Initialize();

	void RegisterRoutes();

	// Reads what decoding captured replica packets needs from the CDClient (main thread, before the workers start)
	void PreloadDecoding();

	// MESSAGE_CAPTURE_DATA with status PACKETS, via master
	void HandleData(const MessageCaptureData& data);

	// Main loop: writes batches, pushes to browsers, re-arms and ends captures on time
	void Update();

	// Writes what is waiting and closes the files
	void Shutdown();

	// Whether a packet capture is still recording
	bool IsRunning(uint64_t sessionId);

	// Where capture files are kept (capture_dir, relative to the server's folder), and one capture's file
	std::filesystem::path Folder();
	std::filesystem::path FileOf(uint64_t sessionId);
}
