#ifndef CDCLIENTRELOADER_H
#define CDCLIENTRELOADER_H

#include <filesystem>
#include <functional>
#include <string>

#include "dCommonVars.h"

struct CDClientReload;

/**
 * Master's side of the CDClient reload (docs/CDClientFdb.md).
 *
 * Master keeps a content-addressed copy of the client's cdclient.fdb in resServer and the CDServer.sqlite made from
 * it, and every server opens those instead of the client's file. It watches the client's file (size and mtime every
 * cdclient_watch_seconds) and, when it changes or a GM or the dashboard asks, makes a new copy and CDServer.sqlite on a
 * worker thread, switches itself over and tells every server (CDCLIENT_RELOAD). The last two copies are kept.
 */
namespace CDClientReloader {
	/**
	 * At startup, once resServer/CDServer.sqlite exists and before CDClientDatabase connects: makes sure the current
	 * copy of the client's fdb is there (and its CDServer.sqlite, if the fdb changed while master was down) and writes
	 * the pointer file. Main thread.
	 *
	 * @return the CDServer.sqlite to connect to and the fdb copy to map (empty when the client has no loose cdclient.fdb)
	 */
	struct Files {
		std::filesystem::path sqlite;
		std::filesystem::path fdb;
	};
	Files Init(const std::filesystem::path& clientFdb, const std::filesystem::path& resServer, const std::filesystem::path& migrationsDir);

	// Tells every server to switch (CDCLIENT_RELOAD)
	void SetBroadcast(std::function<void(const CDClientReload&)> broadcast);

	// Checks the client's fdb now; who is for the log
	void Request(const std::string& who);

	// Every frame: polls the file and finishes a reload whose worker is done. Main thread.
	void Update();

	// Waits for a running worker (at shutdown)
	void Shutdown();
};

#endif // CDCLIENTRELOADER_H
