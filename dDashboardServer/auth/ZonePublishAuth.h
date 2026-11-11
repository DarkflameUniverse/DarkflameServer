#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <string_view>

#include "ApiKeyScope.h"
#include "eHTTPStatusCode.h"

/**
 * Who may use the zone file API (/api/v1/zones/..., docs/ZonePublish.md), checked on every request before any of
 * its body is taken. Pure, so every refusal is unit tested without a web server or a database: the caller looks up
 * the key and its owner (ApiKeyService does, on every request) and the request's addresses.
 */
namespace ZonePublishAuth {
	// The permission an API key needs in its own list (the zones.publish scope; permission names have no dots, so it
	// is kept as zones_publish and either spelling makes a key with it). A key with "all of the owner's permissions"
	// does not count, so a key can only write zone files when it was made for that.
	constexpr const char* SCOPE = "zones_publish";
	constexpr const char* SCOPE_ALIAS = "zones.publish";

	inline bool IsLoopback(std::string_view address) {
		return address == "::1" || address == "localhost" || address.starts_with("127.") || address == "::ffff:127.0.0.1";
	}

	/**
	 * Whether a request's key travelled safely: over HTTPS, or from this machine. The dashboard itself speaks plain
	 * HTTP; HTTPS is a reverse proxy's in front of it (behind_proxy=1), which says so in X-Forwarded-Proto.
	 * @param peer The address the connection came from
	 * @param forwardedFor The last X-Forwarded-For entry (what the proxy added), empty for none
	 */
	inline bool TransportAllowed(bool behindProxy, std::string_view peer, std::string_view forwardedFor, std::string_view forwardedProto) {
		// Without a proxy nothing is encrypted: this machine only. X-Forwarded-* from anyone is not believed.
		if (!behindProxy) return IsLoopback(peer);
		// Not through the proxy at all: a local caller
		if (forwardedFor.empty() && forwardedProto.empty()) return IsLoopback(peer);
		if (forwardedProto == "https") return true;
		// Through the proxy over plain HTTP: only from the proxy's own machine
		return IsLoopback(forwardedFor) && IsLoopback(peer);
	}

	enum class eDenial : uint8_t {
		NONE,
		RATE_LIMITED,   // too many refused requests from this address
		PLAIN_HTTP,     // a key sent over plain HTTP from another machine
		NO_KEY,         // no key, or one that is unknown, revoked, expired or whose owner can't sign in
		NOT_A_KEY,      // signed in some other way (a session): this API takes API keys only
		SCOPE,          // the key's permissions don't name zones.publish
		READ_ONLY,      // a read-only key changing something
		GM_LEVEL,       // the owner's GM level is below zone_publish_gm_level now
	};

	struct Request {
		bool blocked{};             // FailureLimiter::Blocked for the address
		bool transportAllowed{};
		bool authenticated{};       // a credential was accepted (HTTPContext::isAuthenticated)
		const ApiKeys::Scope* key{}; // the API key that authenticated it, if one did
		uint8_t gmLevel{};          // the key owner's GM level, read from the database for this request
		uint8_t requiredLevel{};    // zone_publish_gm_level
		bool changes{};             // anything but a GET
	};

	inline eDenial Check(const Request& request) {
		if (request.blocked) return eDenial::RATE_LIMITED;
		if (!request.transportAllowed) return eDenial::PLAIN_HTTP;
		if (!request.authenticated) return eDenial::NO_KEY;
		if (!request.key) return eDenial::NOT_A_KEY;
		if (!request.key->permissions.contains(SCOPE)) return eDenial::SCOPE;
		if (request.key->readOnly && request.changes) return eDenial::READ_ONLY;
		if (request.gmLevel < request.requiredLevel) return eDenial::GM_LEVEL;
		return eDenial::NONE;
	}

	inline eHTTPStatusCode Status(eDenial denial) {
		switch (denial) {
		case eDenial::NONE: return eHTTPStatusCode::OK;
		case eDenial::RATE_LIMITED: return eHTTPStatusCode::TOO_MANY_REQUESTS;
		case eDenial::NO_KEY:
		case eDenial::NOT_A_KEY: return eHTTPStatusCode::UNAUTHORIZED;
		default: return eHTTPStatusCode::FORBIDDEN;
		}
	}

	// The reply's "error"
	inline const char* Code(eDenial denial) {
		switch (denial) {
		case eDenial::NONE: return "";
		case eDenial::RATE_LIMITED: return "rate_limited";
		case eDenial::PLAIN_HTTP: return "https_required";
		case eDenial::NO_KEY: return "unauthorized";
		case eDenial::NOT_A_KEY: return "api_key_required";
		case eDenial::SCOPE: return "scope_required";
		case eDenial::READ_ONLY: return "read_only_key";
		case eDenial::GM_LEVEL: return "gm_level";
		}
		return "";
	}

	inline const char* Message(eDenial denial) {
		switch (denial) {
		case eDenial::NONE: return "";
		case eDenial::RATE_LIMITED: return "Too many refused requests from this address; try again later";
		case eDenial::PLAIN_HTTP: return "API keys are only taken over HTTPS, or from the server's own machine";
		case eDenial::NO_KEY: return "Send a valid API key as 'Authorization: Bearer <key>'";
		case eDenial::NOT_A_KEY: return "This API takes API keys only";
		case eDenial::SCOPE: return "This API key doesn't have the zones.publish permission";
		case eDenial::READ_ONLY: return "This API key is read-only";
		case eDenial::GM_LEVEL: return "Your account's GM level is too low for zone files";
		}
		return "";
	}

	/**
	 * Refused requests per address, on top of each key's own rate limit: after maxFailures inside the window, the
	 * address is refused outright until its failures age out. Times are unix seconds, passed in. Web thread only.
	 */
	class FailureLimiter {
	public:
		FailureLimiter(uint32_t maxFailures, int64_t windowSeconds) : m_Max(maxFailures), m_Window(windowSeconds) {}

		bool Blocked(const std::string& address, int64_t now) {
			const auto it = m_Failures.find(address);
			if (it == m_Failures.end()) return false;
			Age(it->second, now);
			if (it->second.empty()) {
				m_Failures.erase(it);
				return false;
			}
			return it->second.size() >= m_Max;
		}

		// Returns true when this failure is the one that blocks the address
		bool RecordFailure(const std::string& address, int64_t now) {
			if (m_Failures.size() > 10000) Prune(now);
			auto& failures = m_Failures[address];
			Age(failures, now);
			if (failures.size() >= m_Max) return false;
			failures.push_back(now);
			return failures.size() == m_Max;
		}

		size_t Size() const { return m_Failures.size(); }

	private:
		void Age(std::deque<int64_t>& failures, int64_t now) const {
			while (!failures.empty() && failures.front() <= now - m_Window) failures.pop_front();
		}
		void Prune(int64_t now) {
			std::erase_if(m_Failures, [&](auto& entry) { Age(entry.second, now); return entry.second.empty(); });
		}

		uint32_t m_Max;
		int64_t m_Window;
		std::map<std::string, std::deque<int64_t>> m_Failures;
	};
}
