#include "UgcFetch.h"

#include <chrono>
#include <mutex>

#include <curl/curl.h>

#include "dConfig.h"
#include "Game.h"
#include "RouteUtils.h"
#include "TtlCache.h"

namespace {
	std::mutex g_CacheMutex;
	TtlCache<std::string, std::shared_ptr<const UgcFetch::Fetched>> g_FileCache(std::chrono::seconds(15), 32 * 1024 * 1024);
	TtlCache<std::string, std::shared_ptr<const UgcFetch::Fetched>> g_StatusCache(std::chrono::seconds(2), 1024 * 1024);

	constexpr size_t MAX_FETCH_BYTES = 128 * 1024 * 1024;

	size_t Collect(char* data, size_t size, size_t count, void* userData) {
		auto* out = static_cast<std::string*>(userData);
		if (out->size() + size * count > MAX_FETCH_BYTES) return 0;
		out->append(data, size * count);
		return size * count;
	}

	// Sends a request set up by setup (a GET unless it says otherwise)
	template <typename Setup>
	std::shared_ptr<const UgcFetch::Fetched> Perform(const std::string& url, long timeout, Setup&& setup) {
		auto out = std::make_shared<UgcFetch::Fetched>();
		CURL* curl = curl_easy_init();
		if (!curl) {
			out->error = "could not start a request";
			return out;
		}
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "DarkflameServer-Dashboard");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Collect);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out->body);
		setup(curl);
		const auto code = curl_easy_perform(curl);
		if (code == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out->status);
		else out->error = curl_easy_strerror(code);
		curl_easy_cleanup(curl);
		return out;
	}
}

namespace UgcFetch {
	std::string InternalUrl() {
		auto url = Game::config->GetValue("ugc_internal_url");
		if (url.empty()) url = "http://127.0.0.1:2008";
		while (url.ends_with('/')) url.pop_back();
		return url;
	}

	std::shared_ptr<const Fetched> Get(const std::string& url) {
		return Perform(url, 20L, [](CURL*) {});
	}

	std::shared_ptr<const Fetched> CachedGet(const std::string& url, eCache cache) {
		auto& store = cache == eCache::STATUS ? g_StatusCache : g_FileCache;
		{
			std::lock_guard lock(g_CacheMutex);
			if (auto hit = store.Get(url)) return *hit;
		}
		auto fetched = Get(url);
		if (fetched->status == 200 || fetched->status == 404) {
			std::lock_guard lock(g_CacheMutex);
			store.Put(url, fetched, fetched->body.size() + 64);
		}
		return fetched;
	}

	std::shared_ptr<const Fetched> AdminPost(const std::string& url, const std::string& key, const std::string& body) {
		curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
		headers = curl_slist_append(headers, ("X-Ugc-Admin-Key: " + key).c_str());
		auto out = Perform(url, 120L, [&](CURL* curl) {
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
		});
		curl_slist_free_all(headers);
		return out;
	}

	void ReplyError(HTTPReply& reply, const Fetched& fetched, std::string_view shownUrl) {
		using RouteUtils::JsonError;
		if (fetched.status == 408) return JsonError(reply, eHTTPStatusCode::REQUEST_TIMEOUT, "The UGC server is making it; try again in a moment");
		if (fetched.status == 404) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "The UGC server has not made it");
		if (fetched.status == 0) {
			const std::string at = shownUrl.empty() ? std::string() : " at " + std::string(shownUrl);
			return JsonError(reply, eHTTPStatusCode::BAD_GATEWAY, "The UGC server doesn't answer" + at + " (" + fetched.error + ")");
		}
		JsonError(reply, eHTTPStatusCode::BAD_GATEWAY, "The UGC server answered " + std::to_string(fetched.status));
	}
}
