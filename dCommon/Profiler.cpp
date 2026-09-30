#include "Profiler.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <optional>
#include <unordered_set>

#ifdef DLU_TRACY
#include "tracy/TracyC.h"
#endif

namespace Profiler {
	namespace {
		thread_local bool t_Main = false;

#ifdef DLU_TRACY
		// Tracy zones for the scopes (its C interface: zones with names made at run time)
		uint64_t TracyBegin(const char* name, uint64_t arg) {
			const auto location = ___tracy_alloc_srcloc_name(0, "", 0, "", 0, name, std::strlen(name), 0);
			const auto zone = ___tracy_emit_zone_begin_alloc(location, 1);
			if (arg) ___tracy_emit_zone_value(zone, arg);
			return (static_cast<uint64_t>(zone.id) << 32) | static_cast<uint32_t>(zone.active);
		}

		void TracyEnd(uint64_t packed) {
			TracyCZoneCtx zone{};
			zone.id = static_cast<uint32_t>(packed >> 32);
			zone.active = static_cast<int>(static_cast<uint32_t>(packed));
			___tracy_emit_zone_end(zone);
		}
#endif

		constexpr const char* MORE = "(more)";
		constexpr const char* ALL_FRAMES = "All frames";

		uint32_t ClampU32(int64_t value) {
			return static_cast<uint32_t>(std::clamp<int64_t>(value, 0, UINT32_MAX));
		}

		uint64_t Micros(int64_t ns) {
			return ns > 0 ? static_cast<uint64_t>(ns / 1000) : 0;
		}

		std::string Duration(uint64_t us) {
			char buffer[32];
			if (us >= 1000000) std::snprintf(buffer, sizeof(buffer), "%.1f s", static_cast<double>(us) / 1e6);
			else std::snprintf(buffer, sizeof(buffer), "%.1f ms", static_cast<double>(us) / 1e3);
			return buffer;
		}

		bool SameName(const char* a, const char* b) {
			return a == b || std::strcmp(a, b) == 0;
		}

		// The children of nodes[i] in a pre-order list: the following nodes one deeper, until one as shallow as it
		template<typename Fn>
		void ForEachChild(const std::vector<Node>& nodes, size_t i, Fn&& fn) {
			for (size_t j = i + 1; j < nodes.size() && nodes[j].depth > nodes[i].depth; j++) {
				if (nodes[j].depth == nodes[i].depth + 1) fn(j);
			}
		}
	}

	const char* PhaseName(size_t phase) {
		static constexpr const char* NAMES[PHASES] = { "other", "packets", "entities", "physics", "replica", "scripts", "database", "cdclient", "log_flush", "web" };
		return phase < PHASES ? NAMES[phase] : "";
	}

	void Second::Merge(const Second& other) {
		ticks += other.ticks;
		totalUs += other.totalUs;
		maxUs = std::max(maxUs, other.maxUs);
		frames.Merge(other.frames);
		for (size_t i = 0; i < PHASES; i++) phaseUs[i] += other.phaseUs[i];
	}

	std::string DefaultLabel(const Node& node) {
		if (node.name == PACKET) {
			const auto key = TrafficStats::MessageKey::Unpack(node.arg);
			std::string label = "Packet " + std::to_string(key.service) + ":" + std::to_string(key.packet);
			if (key.gameMessage) label += ":" + std::to_string(key.gameMessage);
			return label;
		}
		return node.arg ? node.name + " " + std::to_string(node.arg) : node.name;
	}

	std::string Frame::Path(const std::function<std::string(const Node&)>& label) const {
		if (scopes.empty()) return "";
		const auto name = [&label](const Node& node) { return label ? label(node) : DefaultLabel(node); };
		std::string path;
		size_t current = 0;
		while (true) {
			size_t heaviest = SIZE_MAX;
			ForEachChild(scopes, current, [&](size_t j) {
				if (heaviest == SIZE_MAX || scopes[j].totalUs > scopes[heaviest].totalUs) heaviest = j;
			});
			// Stop where the scope's own time is most of it
			if (heaviest == SIZE_MAX || scopes[heaviest].totalUs * 5 < scopes[current].totalUs) break;
			current = heaviest;
			const auto& node = scopes[current];
			if (!path.empty()) path += " > ";
			path += name(node) + " " + Duration(node.totalUs);
			if (node.count > 1) path += " x" + std::to_string(node.count);
		}
		// The busiest repeated scope below where the path stopped (e.g. many small lookups)
		size_t repeated = SIZE_MAX;
		for (size_t j = current + 1; j < scopes.size() && scopes[j].depth > scopes[current].depth; j++) {
			if (scopes[j].count > 1 && (repeated == SIZE_MAX || scopes[j].totalUs > scopes[repeated].totalUs)) repeated = j;
		}
		if (repeated != SIZE_MAX) {
			path += (path.empty() ? "" : ", ") + name(scopes[repeated]) + " " + Duration(scopes[repeated].totalUs) + " x" + std::to_string(scopes[repeated].count);
		}
		return path;
	}

	std::string Folded(const std::vector<Node>& nodes, const std::function<std::string(const Node&)>& label) {
		std::string out;
		std::vector<std::string> stack;
		for (size_t i = 0; i < nodes.size(); i++) {
			const auto& node = nodes[i];
			std::string name = label ? label(node) : DefaultLabel(node);
			std::replace(name.begin(), name.end(), ';', ',');
			std::replace(name.begin(), name.end(), '\n', ' ');
			stack.resize(node.depth);
			stack.push_back(std::move(name));
			uint64_t children = 0;
			ForEachChild(nodes, i, [&](size_t j) { children += nodes[j].totalUs; });
			const uint64_t self = node.totalUs > children ? node.totalUs - children : 0;
			if (self == 0) continue;
			for (size_t d = 0; d < stack.size(); d++) {
				if (d) out += ';';
				out += stack[d];
			}
			out += ' ';
			out += std::to_string(self);
			out += '\n';
		}
		return out;
	}

	uint32_t Recorder::Child(std::vector<LiveNode>& nodes, uint32_t parent, const char* name, uint64_t arg, size_t maxNodes, bool& full) {
		full = false;
		for (uint32_t c = nodes[parent].firstChild; c; c = nodes[c].nextSibling) {
			if (nodes[c].arg == arg && SameName(nodes[c].name, name)) return c;
		}
		// Too many different children: the rest share one
		if (nodes[parent].children >= MAX_CHILDREN && !(arg == 0 && SameName(name, MORE))) {
			return Child(nodes, parent, MORE, 0, maxNodes, full);
		}
		if (nodes.size() >= maxNodes) {
			full = true;
			return parent;
		}
		const auto index = static_cast<uint32_t>(nodes.size());
		LiveNode node;
		node.name = name;
		node.arg = arg;
		node.parent = parent;
		node.nextSibling = nodes[parent].firstChild;
		nodes.push_back(node);
		nodes[parent].firstChild = index;
		nodes[parent].children++;
		return index;
	}

	std::vector<Node> Recorder::Flatten(const std::vector<LiveNode>& nodes, size_t limit, bool byStart, bool& truncated) {
		std::vector<Node> out;
		if (nodes.empty()) return out;
		std::vector<char> keep(nodes.size(), 1);
		truncated = nodes.size() > limit;
		if (truncated) {
			std::vector<uint32_t> order(nodes.size());
			for (uint32_t i = 0; i < order.size(); i++) order[i] = i;
			std::stable_sort(order.begin(), order.end(), [&nodes](uint32_t a, uint32_t b) { return nodes[a].totalNs > nodes[b].totalNs; });
			std::fill(keep.begin(), keep.end(), 0);
			keep[0] = 1;
			for (size_t i = 0; i < limit && i < order.size(); i++) {
				// With its parents, so the tree stays whole
				for (uint32_t n = order[i]; !keep[n]; n = nodes[n].parent) keep[n] = 1;
			}
		}
		out.reserve(std::min(limit + 8, nodes.size()));
		// Pre-order, without recursion
		std::vector<std::pair<uint32_t, uint8_t>> pending{ { 0u, uint8_t{ 0 } } };
		std::vector<uint32_t> children;
		while (!pending.empty()) {
			const auto [index, depth] = pending.back();
			pending.pop_back();
			const auto& live = nodes[index];
			Node node;
			node.name = live.name ? live.name : "";
			node.arg = live.arg;
			node.depth = depth;
			node.count = live.count;
			node.totalUs = Micros(live.totalNs);
			node.startUs = ClampU32(live.startNs / 1000);
			out.push_back(std::move(node));

			children.clear();
			for (uint32_t c = live.firstChild; c; c = nodes[c].nextSibling) if (keep[c]) children.push_back(c);
			if (byStart) std::sort(children.begin(), children.end(), [&nodes](uint32_t a, uint32_t b) { return nodes[a].startNs != nodes[b].startNs ? nodes[a].startNs < nodes[b].startNs : a < b; });
			else std::sort(children.begin(), children.end(), [&nodes](uint32_t a, uint32_t b) { return nodes[a].totalNs != nodes[b].totalNs ? nodes[a].totalNs > nodes[b].totalNs : a < b; });
			const auto childDepth = static_cast<uint8_t>(std::min(depth + 1, 255));
			// Pushed in reverse so the first comes out first
			for (auto it = children.rbegin(); it != children.rend(); ++it) pending.emplace_back(*it, childDepth);
		}
		return out;
	}

	void Recorder::FrameBegin(int64_t nowNs, int64_t unixMs, bool implicit) {
		if (m_InFrame) return;
		m_InFrame = true;
		m_Implicit = implicit;
		m_FrameStartNs = nowNs;
		m_FrameUnixMs = unixMs;
		m_Nodes.clear();
		LiveNode root;
		root.name = implicit ? OUTSIDE : FRAME;
		m_Nodes.push_back(root);
		m_Stack.clear();
		m_Phase = Phase::OTHER;
		m_PhaseStartNs = nowNs;
		m_PhaseNs.fill(0);
	}

	Frame Recorder::MakeFrame(int64_t durationNs, size_t scopes) const {
		Frame frame;
		frame.timeMs = m_FrameUnixMs;
		frame.durationUs = ClampU32(durationNs / 1000);
		frame.implicit = m_Implicit;
		for (size_t i = 0; i < PHASES; i++) frame.phaseUs[i] = ClampU32(m_PhaseNs[i] / 1000);
		bool truncated = false;
		frame.scopes = Flatten(m_Nodes, scopes, true, truncated);
		return frame;
	}

	void Recorder::FrameEnd(int64_t nowNs) {
		if (!m_InFrame) return;
		// Scopes still open (a frame ended inside one) end with it
		while (!m_Stack.empty()) {
			const auto open = m_Stack.back();
			m_Stack.pop_back();
			if (open.counted) m_Nodes[open.node].totalNs += nowNs - open.startNs;
		}
		m_PhaseNs[static_cast<size_t>(m_Phase)] += nowNs - m_PhaseStartNs;
		const int64_t duration = std::max<int64_t>(nowNs - m_FrameStartNs, 0);
		m_Nodes[0].count = 1;
		m_Nodes[0].totalNs = duration;
		const uint32_t us = ClampU32(duration / 1000);
		const uint32_t threshold = SlowThreshold();
		const bool slow = threshold > 0 && us >= static_cast<uint64_t>(threshold) * 1000;

		std::optional<Frame> slowFrame;
		if (slow) slowFrame = MakeFrame(duration, SLOW_SCOPES);
		{
			std::lock_guard lock(m_Mutex);
			if (!m_Implicit) {
				auto& second = m_Seconds[m_FrameUnixMs / 1000];
				second.time = m_FrameUnixMs / 1000;
				second.ticks++;
				second.totalUs += us;
				second.maxUs = std::max(second.maxUs, us);
				second.frames.Add(us);
				for (size_t i = 0; i < PHASES; i++) second.phaseUs[i] += Micros(m_PhaseNs[i]);
			}
			if (m_Worst.size() < WORST_FRAMES || us > m_Worst.back().durationUs) {
				auto frame = MakeFrame(duration, WORST_SCOPES);
				const auto at = std::find_if(m_Worst.begin(), m_Worst.end(), [us](const Frame& f) { return f.durationUs < us; });
				m_Worst.insert(at, std::move(frame));
				if (m_Worst.size() > WORST_FRAMES) m_Worst.pop_back();
			}
			if (slowFrame && m_Slow.size() < MAX_SLOW_FRAMES) m_Slow.push_back(*slowFrame);
		}
		m_InFrame = false;
		if (slowFrame && m_SlowSink) m_SlowSink(*slowFrame);

		if (m_Session.active) {
			MergeIntoSession();
			m_Session.frames++;
			m_Session.totalNs += duration;
			if (nowNs >= m_Session.endNs) FinishSession(nowNs);
		}
	}

	void Recorder::Enter(const char* name, uint64_t arg, int64_t nowNs) {
		if (!m_InFrame) FrameBegin(nowNs, UnixMs(), true);
		const uint32_t parent = m_Stack.empty() ? 0 : m_Stack.back().node;
		bool full = false;
		const uint32_t index = Child(m_Nodes, parent, name, arg, MAX_NODES, full);
		if (full) {
			m_Stack.push_back({ parent, nowNs, false });
			return;
		}
		auto& node = m_Nodes[index];
		if (node.count == 0) node.startNs = nowNs - m_FrameStartNs;
		node.count++;
		m_Stack.push_back({ index, nowNs, true });
	}

	void Recorder::Exit(int64_t nowNs) {
		if (m_Stack.empty()) return;
		const auto open = m_Stack.back();
		m_Stack.pop_back();
		if (open.counted) m_Nodes[open.node].totalNs += nowNs - open.startNs;
		if (m_Stack.empty() && m_Implicit && m_InFrame) FrameEnd(nowNs);
	}

	Phase Recorder::SetPhase(Phase phase, int64_t nowNs) {
		const auto previous = m_Phase;
		if (!m_InFrame) return previous;
		m_PhaseNs[static_cast<size_t>(previous)] += nowNs - m_PhaseStartNs;
		m_PhaseStartNs = nowNs;
		m_Phase = phase;
		return previous;
	}

	void Recorder::Record(const char* name, uint64_t arg, int64_t durationNs, Phase phase, int64_t nowNs) {
		if (!m_InFrame || durationNs < 0) return;
		const uint32_t parent = m_Stack.empty() ? 0 : m_Stack.back().node;
		bool full = false;
		const uint32_t index = Child(m_Nodes, parent, name, arg, MAX_NODES, full);
		if (!full) {
			auto& node = m_Nodes[index];
			if (node.count == 0) node.startNs = std::max<int64_t>(nowNs - durationNs - m_FrameStartNs, 0);
			node.count++;
			node.totalNs += durationNs;
		}
		// The time moves from the current phase to its own
		if (phase != m_Phase) {
			const int64_t moved = std::min(durationNs, std::max<int64_t>(nowNs - m_PhaseStartNs, 0));
			m_PhaseNs[static_cast<size_t>(phase)] += moved;
			m_PhaseStartNs += moved;
		}
	}

	void Recorder::AddMessageTime(uint64_t key, int64_t durationNs) {
		const auto us = Micros(durationNs);
		std::lock_guard lock(m_Mutex);
		auto& message = m_Messages[key];
		message.key = key;
		message.count++;
		message.totalUs += us;
		message.maxUs = std::max(message.maxUs, ClampU32(static_cast<int64_t>(us)));
	}

	void Recorder::SetSlowThreshold(uint32_t milliseconds) {
		std::lock_guard lock(m_Mutex);
		m_SlowThresholdMs = milliseconds;
	}

	uint32_t Recorder::SlowThreshold() const {
		std::lock_guard lock(m_Mutex);
		return m_SlowThresholdMs;
	}

	bool Recorder::StartSession(uint32_t id, uint32_t durationMs, int64_t nowNs, std::function<void(Profile&&)> done) {
		if (m_Session.active) return false;
		m_Session = Session{};
		m_Session.active = true;
		m_Session.id = id;
		m_Session.startNs = nowNs;
		m_Session.endNs = nowNs + static_cast<int64_t>(std::clamp<uint32_t>(durationMs, 1, MAX_SESSION_MS)) * 1000000;
		LiveNode root;
		root.name = ALL_FRAMES;
		m_Session.nodes.push_back(root);
		m_Session.done = std::move(done);
		return true;
	}

	bool Recorder::StopSession(uint32_t id, int64_t nowNs) {
		if (!m_Session.active || m_Session.id != id) return false;
		FinishSession(nowNs);
		return true;
	}

	void Recorder::CheckSession(int64_t nowNs) {
		if (m_Session.active && !m_InFrame && nowNs >= m_Session.endNs) FinishSession(nowNs);
	}

	void Recorder::MergeIntoSession() {
		auto& session = m_Session;
		// Parents come before their children in m_Nodes, so each parent is mapped before its children
		std::vector<uint32_t> mapped(m_Nodes.size(), 0);
		for (uint32_t i = 1; i < m_Nodes.size(); i++) {
			const auto& node = m_Nodes[i];
			const uint32_t parent = mapped[node.parent];
			bool full = false;
			const uint32_t index = Child(session.nodes, parent, node.name, node.arg, MAX_SESSION_NODES, full);
			mapped[i] = index;
			if (full) {
				// Its time stays in the parent's (the parent's total includes it)
				session.truncated = true;
				continue;
			}
			session.nodes[index].count += m_Nodes[i].count;
			session.nodes[index].totalNs += m_Nodes[i].totalNs;
		}
		session.nodes[0].count++;
		session.nodes[0].totalNs += m_Nodes[0].totalNs;
	}

	void Recorder::FinishSession(int64_t nowNs) {
		Profile profile;
		profile.id = m_Session.id;
		profile.durationMs = ClampU32((nowNs - m_Session.startNs) / 1000000);
		profile.frames = m_Session.frames;
		profile.totalUs = Micros(m_Session.totalNs);
		bool truncated = false;
		profile.nodes = Flatten(m_Session.nodes, PROFILE_NODES, false, truncated);
		profile.truncated = truncated || m_Session.truncated;
		auto done = std::move(m_Session.done);
		m_Session = Session{};
		if (done) done(std::move(profile));
	}

	Report Recorder::Take(int64_t now) {
		Report report;
		report.present = true;
		std::lock_guard lock(m_Mutex);
		report.slowThresholdMs = m_SlowThresholdMs;
		int64_t from = m_LastReported ? m_LastReported + 1 : (m_Seconds.empty() ? now : std::min(m_Seconds.begin()->first, now - 1));
		from = std::max(from, now - MAX_GAP);
		for (int64_t t = from; t < now; t++) {
			const auto it = m_Seconds.find(t);
			if (it != m_Seconds.end()) report.seconds.push_back(std::move(it->second));
			else report.seconds.push_back(Second{ .time = t });
		}
		m_Seconds.erase(m_Seconds.begin(), m_Seconds.lower_bound(now));
		if (now - 1 > m_LastReported) m_LastReported = now - 1;

		report.messages.reserve(m_Messages.size());
		for (const auto& [_, message] : m_Messages) report.messages.push_back(message);
		m_Messages.clear();
		std::sort(report.messages.begin(), report.messages.end(), [](const MessageTime& a, const MessageTime& b) {
			return a.totalUs != b.totalUs ? a.totalUs > b.totalUs : a.key < b.key;
		});
		if (report.messages.size() > TOP_MESSAGES) report.messages.resize(TOP_MESSAGES);

		report.worst = std::move(m_Worst);
		m_Worst.clear();
		report.slow = std::move(m_Slow);
		m_Slow.clear();
		return report;
	}

	Recorder& Local() {
		static Recorder recorder;
		return recorder;
	}

	void SetMainThread() {
		t_Main = true;
	}

	bool IsMainThread() {
		return t_Main;
	}

	int64_t NowNs() {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	int64_t UnixMs() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	const char* Intern(const std::string& name) {
		// Never freed: scope trees point at these until the process ends
		static auto* names = new std::unordered_set<std::string>();
		static std::mutex mutex;
		std::lock_guard lock(mutex);
		return names->insert(name).first->c_str();
	}

	void BeginFrame() {
		if (t_Main) Local().FrameBegin(NowNs(), UnixMs(), false);
	}

	void EndFrame() {
		if (!t_Main) return;
		Local().FrameEnd(NowNs());
#ifdef DLU_TRACY
		___tracy_emit_frame_mark(nullptr);
#endif
	}

	FrameScope::FrameScope() {
		if (!t_Main || Local().InFrame()) return;
		m_Active = true;
		Local().FrameBegin(NowNs(), UnixMs(), false);
	}

	FrameScope::~FrameScope() {
		if (!m_Active) return;
		Local().FrameEnd(NowNs());
#ifdef DLU_TRACY
		___tracy_emit_frame_mark(nullptr);
#endif
	}

	Scope::Scope(const char* name, uint64_t arg) {
		if (!t_Main) return;
		m_Active = true;
		Local().Enter(name, arg, NowNs());
#ifdef DLU_TRACY
		m_Tracy = TracyBegin(name, arg);
#endif
	}

	Scope::Scope(const char* name, Phase phase) {
		if (!t_Main) return;
		m_Active = true;
		const auto now = NowNs();
		auto& recorder = Local();
		recorder.Enter(name, 0, now);
		m_Previous = recorder.SetPhase(phase, now);
		m_SetPhase = true;
#ifdef DLU_TRACY
		m_Tracy = TracyBegin(name, 0);
#endif
	}

	Scope::~Scope() {
		if (!m_Active) return;
#ifdef DLU_TRACY
		TracyEnd(m_Tracy);
#endif
		const auto now = NowNs();
		auto& recorder = Local();
		if (m_SetPhase) recorder.SetPhase(m_Previous, now);
		recorder.Exit(now);
	}

	PacketScope::PacketScope(const uint8_t* data, size_t length) {
		if (!t_Main) return;
		m_Active = true;
		m_Key = TrafficStats::KeyOf(data, length, false).Packed();
		m_StartNs = NowNs();
		auto& recorder = Local();
		recorder.Enter(PACKET, m_Key, m_StartNs);
		m_Previous = recorder.SetPhase(Phase::PACKETS, m_StartNs);
		m_SetPhase = true;
#ifdef DLU_TRACY
		m_Tracy = TracyBegin(PACKET, m_Key);
#endif
	}

	PacketScope::~PacketScope() {
		if (!m_Active) return;
#ifdef DLU_TRACY
		TracyEnd(m_Tracy);
#endif
		const auto now = NowNs();
		auto& recorder = Local();
		if (m_SetPhase) recorder.SetPhase(m_Previous, now);
		recorder.Exit(now);
		recorder.AddMessageTime(m_Key, now - m_StartNs);
	}
}
