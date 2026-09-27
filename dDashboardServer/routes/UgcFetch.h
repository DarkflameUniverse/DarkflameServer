#ifndef UGCFETCH_H
#define UGCFETCH_H

#include <memory>
#include <string>
#include <string_view>

struct HTTPReply;

/**
 * The dashboard's own way to the UGC server (ugc_internal_url, normally the same machine): its status and the files it
 * made are fetched here and handed to the browser, which may not be able to reach the UGC server at all. Work out the
 * URL (and anything else from the config or database) on the web thread; the requests themselves only touch curl and
 * this file's cache, so they can run on worker threads (Workers::Reply).
 */
namespace UgcFetch {
	struct Fetched {
		long status{}; // HTTP status, 0 when the UGC server didn't answer
		std::string body;
		std::string error;
	};

	enum class eCache {
		FILE,   // files the UGC server made, kept 15 seconds
		STATUS, // its status, kept 2 seconds
	};

	// Web thread: where the dashboard reaches the UGC server, without a trailing slash
	std::string InternalUrl();

	// Any thread: a GET to the UGC server, not cached
	std::shared_ptr<const Fetched> Get(const std::string& url);

	// Any thread: Get through a short cache (answers that aren't a file being made are kept)
	std::shared_ptr<const Fetched> CachedGet(const std::string& url, eCache cache = eCache::FILE);

	// Any thread: a POST of JSON to one of the UGC server's /admin routes, with the key it checks
	std::shared_ptr<const Fetched> AdminPost(const std::string& url, const std::string& key, const std::string& body);

	// Answers with why a fetch didn't give a file. The URL is only named when shown (not to players, who don't need it)
	void ReplyError(HTTPReply& reply, const Fetched& fetched, std::string_view shownUrl = {});
}

#endif // !UGCFETCH_H
