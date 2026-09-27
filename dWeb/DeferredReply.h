#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "HTTPReply.h"

class DeferredQueue;

// Shared between the web thread and whoever answers a deferred request
struct DeferredState {
	unsigned long connection{};
	uint64_t id{};               // unique per request, so a late answer never reaches a later request
	DeferredQueue* queue{};
	std::atomic<bool> cancelled{};
	std::atomic<bool> answered{};
};

/**
 * A request a route answers later, from any thread: for slow work (converting a big model) that would otherwise hold
 * up every other request, since the web server answers requests one at a time on one thread. Get one with
 * Web::Defer, hand it to a worker, and Send the reply when the work is done; the web thread sends it on its next
 * poll. When the client has gone first, Cancelled() turns true and the reply is dropped.
 */
class DeferredReply {
public:
	DeferredReply() = default;
	explicit DeferredReply(std::shared_ptr<DeferredState> state) : m_State(std::move(state)) {}

	// Answer the request (any thread). Only the first answer counts; it is dropped when the client has gone.
	void Send(HTTPReply reply) const;

	// Whether the client has gone (closed the connection, or the server is stopping), so the work can be skipped
	bool Cancelled() const { return !m_State || m_State->cancelled.load(); }

	explicit operator bool() const { return m_State != nullptr; }

private:
	std::shared_ptr<DeferredState> m_State;
};

/**
 * Deferred requests waiting for their answers, and the answers that have arrived. Everything but Push (which
 * DeferredReply::Send calls from any thread) is for the web thread. No sockets, so it can be unit tested.
 */
class DeferredQueue {
public:
	struct Finished {
		unsigned long connection{};
		HTTPReply reply;
		bool close{}; // the client asked for Connection: close
	};

	// The request on `connection` will be answered later
	std::shared_ptr<DeferredState> Begin(unsigned long connection);

	// Headers the reply is sent with besides its own (e.g. a session cookie middleware refreshed), and whether the
	// connection closes after it
	void SetReplyOptions(unsigned long connection, std::vector<std::string> headers, bool close);

	// The connection closed (or the request was answered another way): a late answer is dropped
	void Close(unsigned long connection);

	// Cancel every pending request (the server is stopping)
	void CancelAll();

	// The answers that arrived for requests still pending; those requests stop being pending
	std::vector<Finished> Drain();

	bool IsPending(unsigned long connection) const { return m_Pending.contains(connection); }
	size_t Pending() const { return m_Pending.size(); }

	// DeferredReply::Send's half: any thread
	void Push(const DeferredState& state, HTTPReply reply);

private:
	struct Waiting {
		std::shared_ptr<DeferredState> state;
		std::vector<std::string> headers;
		bool close{};
	};

	std::map<unsigned long, Waiting> m_Pending; // web thread only
	uint64_t m_NextId{ 1 };

	std::mutex m_Mutex;
	std::vector<std::pair<uint64_t, HTTPReply>> m_Done; // guarded by m_Mutex, keyed by DeferredState::id
};
