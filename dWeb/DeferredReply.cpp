#include "DeferredReply.h"

#include <unordered_map>

void DeferredReply::Send(HTTPReply reply) const {
	if (!m_State || m_State->answered.exchange(true) || m_State->cancelled.load() || !m_State->queue) return;
	reply.deferred.reset();
	m_State->queue->Push(*m_State, std::move(reply));
}

std::shared_ptr<DeferredState> DeferredQueue::Begin(unsigned long connection) {
	auto state = std::make_shared<DeferredState>();
	state->connection = connection;
	state->id = m_NextId++;
	state->queue = this;
	// A connection answers one request at a time, so an older entry is a request that is no longer waited on
	if (const auto it = m_Pending.find(connection); it != m_Pending.end()) it->second.state->cancelled = true;
	m_Pending[connection] = Waiting{ state, {}, false };
	return state;
}

void DeferredQueue::SetReplyOptions(unsigned long connection, std::vector<std::string> headers, bool close) {
	const auto it = m_Pending.find(connection);
	if (it == m_Pending.end()) return;
	it->second.headers = std::move(headers);
	it->second.close = close;
}

void DeferredQueue::Close(unsigned long connection) {
	const auto it = m_Pending.find(connection);
	if (it == m_Pending.end()) return;
	it->second.state->cancelled = true;
	m_Pending.erase(it);
}

void DeferredQueue::CancelAll() {
	for (auto& [connection, waiting] : m_Pending) waiting.state->cancelled = true;
	m_Pending.clear();
	std::lock_guard lock(m_Mutex);
	m_Done.clear();
}

std::vector<DeferredQueue::Finished> DeferredQueue::Drain() {
	std::vector<std::pair<uint64_t, HTTPReply>> done;
	{
		std::lock_guard lock(m_Mutex);
		done.swap(m_Done);
	}
	std::vector<Finished> finished;
	if (done.empty()) return finished;
	std::unordered_map<uint64_t, unsigned long> connectionOf;
	for (const auto& [connection, waiting] : m_Pending) connectionOf.emplace(waiting.state->id, connection);
	for (auto& [id, reply] : done) {
		const auto it = connectionOf.find(id);
		if (it == connectionOf.end()) continue; // the client went away first
		auto waiting = m_Pending.find(it->second);
		auto headers = std::move(waiting->second.headers);
		headers.insert(headers.end(), std::make_move_iterator(reply.headers.begin()), std::make_move_iterator(reply.headers.end()));
		reply.headers = std::move(headers);
		finished.push_back({ it->second, std::move(reply), waiting->second.close });
		m_Pending.erase(waiting);
	}
	return finished;
}

void DeferredQueue::Push(const DeferredState& state, HTTPReply reply) {
	std::lock_guard lock(m_Mutex);
	m_Done.emplace_back(state.id, std::move(reply));
}
