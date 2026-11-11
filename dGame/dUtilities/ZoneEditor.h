#pragma once

#include <cstdint>
#include <string>

#include "RakNetTypes.h"

/**
 * The game client's level editor, on a world server (docs/ZonePublish.md): the client says when its editor is on,
 * locks the zone, sends the zone's files, asks for them and asks for a reload. Everything goes through what the
 * dashboard's zone file API uses: the same checks (ZonePublish), override folder (ZoneOverrideStore), locks and
 * editors (ZoneEditLock, IZoneEditing), audit log entries and world reload. Only how the files travel is different
 * (ZoneFilePack).
 *
 * Packets are read and answered on the main thread. Unpacking, checking, writing and packing files run on one worker
 * thread, which touches no connection, database or setting and hands its result back to the main thread (Update).
 */
namespace ZoneEditor {
	// The client's editor was started or ended (it sends nothing else about it)
	void SetEditing(const SystemAddress& sysAddr, bool editing);
	bool IsEditing(const SystemAddress& sysAddr);

	/**
	 * Whether a client may be told to load a zone. Never while its editor is on: the client drops its connection
	 * when it is told to load a zone then.
	 */
	bool MaySendZoneLoad(const SystemAddress& sysAddr);

	// The connection is gone: nothing is kept about it
	void Forget(const SystemAddress& sysAddr);

	// The editor's requests (fileType and levelFileId as the client sent them)
	void HandleLock(const SystemAddress& sysAddr, bool lock);
	void HandlePushRequest(const SystemAddress& sysAddr, uint8_t fileType, int32_t levelFileId);
	void HandlePush(const SystemAddress& sysAddr, uint8_t fileType, int32_t levelFileId, std::string packedFiles);
	void HandlePull(const SystemAddress& sysAddr, uint8_t fileType, int32_t levelFileId);
	void HandleReload(const SystemAddress& sysAddr);

	// Every frame: finish what the worker did
	void Update();

	// Stops the worker (files being written are finished first)
	void Shutdown();
}
