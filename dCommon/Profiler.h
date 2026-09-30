#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "TrafficStats.h"

/**
 * Frame timing and scope profiling of a server's main loop (see docs/Dashboard.md, "Performance").
 *
 * Each server marks its main loop's frames (FrameScope) and named scopes inside them (Scope); a scope can also name the
 * phase of the frame its time counts as (packets, entities, physics, ...). Everything is recorded on the main thread only: scopes on any other
 * thread do nothing, so workers never touch this. Always on and cheap: two steady_clock reads and a short search of the
 * current scope's children per scope; the frame's scope tree is reused from frame to frame.
 *
 * What comes out, every traffic report (dServer, SERVER_TRAFFIC's frames section):
 * - per second: frames, total and longest frame time, a frame time histogram, and the time each phase took;
 * - the packet types that took longest to handle;
 * - the worst frames of the report with their phases and heaviest scopes;
 * - slow frames (over the slow_frame_ms setting) with their scope tree, also logged as one line.
 * On request, a profiling session merges every frame's scope tree for a few seconds into one tree (a flame graph).
 *
 * Scope names must live as long as the process (string literals, or names from Intern).
 */
namespace Profiler {
	enum class Phase : uint8_t { OTHER, PACKETS, ENTITIES, PHYSICS, REPLICA, SCRIPTS, DATABASE, CDCLIENT, LOG_FLUSH, WEB, COUNT };
	constexpr size_t PHASES = static_cast<size_t>(Phase::COUNT);
	// "other", "packets", "entities", ...; "" past the known ones
	const char* PhaseName(size_t phase);

	// Scope names whose argument means something to the dashboard
	inline constexpr const char* PACKET = "Packet";       // arg: TrafficStats::MessageKey::Packed()
	inline constexpr const char* COMPONENT = "Component"; // arg: eReplicaComponentType
	inline constexpr const char* FRAME = "Frame";         // a main loop frame's root
	inline constexpr const char* OUTSIDE = "Outside the main loop"; // the root of work before or between frames

	// One second of frames
	struct Second {
		int64_t time{}; // Unix seconds
		uint32_t ticks{};
		uint64_t totalUs{};
		uint32_t maxUs{};
		TrafficStats::Histogram frames; // frame times
		std::array<uint64_t, PHASES> phaseUs{};

		void Merge(const Second& other); // adds (keeps this one's time)
	};

	// How long handling one packet type took (MessageKey::Packed)
	struct MessageTime {
		uint64_t key{};
		uint32_t count{};
		uint64_t totalUs{};
		uint32_t maxUs{};
	};

	// A scope in a tree, in pre-order: children follow their parent with depth + 1
	struct Node {
		std::string name;
		uint64_t arg{};
		uint8_t depth{};
		uint32_t count{};   // times entered
		uint64_t totalUs{}; // all of them together, children included
		uint32_t startUs{}; // first entered, from the start of the frame (frames only)
		bool operator==(const Node&) const = default;
	};

	struct Frame {
		int64_t timeMs{};     // Unix milliseconds when it started
		uint32_t durationUs{};
		bool implicit{};      // work outside the main loop's frames (startup, a web request between ticks)
		std::array<uint32_t, PHASES> phaseUs{};
		std::vector<Node> scopes; // the heaviest scopes (and their parents), root first

		// "LoadPlayer > CreateEntity > Component 17: 58.1 s, CDClient Objects x9800", following the heaviest child
		std::string Path(const std::function<std::string(const Node&)>& label = {}) const;
	};

	struct Report {
		bool present{};              // false in reports of servers too old to send frames
		uint32_t slowThresholdMs{};
		std::vector<Second> seconds; // oldest first
		std::vector<MessageTime> messages; // longest total first
		std::vector<Frame> worst;    // the longest frames of the report, longest first
		std::vector<Frame> slow;     // frames over the threshold, oldest first
	};

	// What a profiling session collected: every frame's scopes merged
	struct Profile {
		uint32_t id{};
		uint32_t durationMs{}; // wall time it ran
		uint32_t frames{};
		uint64_t totalUs{};    // time in frames (the rest the loop slept or waited)
		bool truncated{};      // scopes were left out (too many)
		std::vector<Node> nodes; // pre-order, root ("All frames") first; count and totalUs summed over the frames
	};

	// Folded stacks ("root;child;grandchild <self microseconds>" per line), the format flame graph tools read
	std::string Folded(const std::vector<Node>& nodes, const std::function<std::string(const Node&)>& label = {});

	// "name" or "name <arg>" when there is an argument
	std::string DefaultLabel(const Node& node);

	class Recorder {
	public:
		static constexpr size_t MAX_NODES = 4096;       // scopes one frame keeps apart; more are counted in their parent
		static constexpr size_t MAX_CHILDREN = 64;      // different children of one scope; more go to "(more)"
		static constexpr size_t MAX_SESSION_NODES = 20000;
		static constexpr size_t PROFILE_NODES = 3000;   // scopes a finished session sends at most
		static constexpr size_t SLOW_SCOPES = 40;       // scopes a slow frame keeps
		static constexpr size_t WORST_SCOPES = 12;      // scopes a worst frame keeps
		static constexpr size_t WORST_FRAMES = 3;       // per report
		static constexpr size_t MAX_SLOW_FRAMES = 8;    // per report; more are only logged
		static constexpr size_t TOP_MESSAGES = 16;      // per report
		static constexpr int64_t MAX_GAP = 120;         // silent seconds a report fills in at most
		static constexpr uint32_t MAX_SESSION_MS = 60000;

		// All of these: main thread (the explicit clock is for tests; Scope and friends read steady_clock)
		void FrameBegin(int64_t nowNs, int64_t unixMs, bool implicit = false);
		void FrameEnd(int64_t nowNs);
		bool InFrame() const { return m_InFrame; }
		void Enter(const char* name, uint64_t arg, int64_t nowNs);
		void Exit(int64_t nowNs);
		// The phase time goes to from now on; returns the one before
		Phase SetPhase(Phase phase, int64_t nowNs);
		// A finished piece of work of `durationNs` inside the current scope (a database statement timed elsewhere)
		void Record(const char* name, uint64_t arg, int64_t durationNs, Phase phase, int64_t nowNs);
		void AddMessageTime(uint64_t key, int64_t durationNs);

		// Any thread
		void SetSlowThreshold(uint32_t milliseconds);
		uint32_t SlowThreshold() const;
		// Called on the main thread with each slow frame (dServer logs it); none by default
		void SetSlowSink(std::function<void(const Frame&)> sink) { m_SlowSink = std::move(sink); }

		// Main thread. A session merges frames until `durationMs` passed (checked at the end of each frame), then
		// calls `done`. One at a time: false when one runs already.
		bool StartSession(uint32_t id, uint32_t durationMs, int64_t nowNs, std::function<void(Profile&&)> done);
		// Ends it early (the result goes to `done` as usual); false when that session doesn't run
		bool StopSession(uint32_t id, int64_t nowNs);
		bool SessionActive() const { return m_Session.active; }
		uint32_t SessionId() const { return m_Session.id; }
		// Ends a session whose time is up, if no frame did (a loop that stopped framing)
		void CheckSession(int64_t nowNs);

		// The seconds before `now` (Unix seconds) not reported yet, the message times, worst and slow frames since the
		// last report; any thread
		Report Take(int64_t now);

	private:
		struct LiveNode {
			const char* name{};
			uint64_t arg{};
			uint32_t parent{};
			uint32_t firstChild{};  // 0: none (node 0 is the root, never a child)
			uint32_t nextSibling{};
			uint32_t children{};
			uint32_t count{};
			int64_t totalNs{};
			int64_t startNs{};      // first entered, from the start of the frame
		};
		struct Open {
			uint32_t node{};
			int64_t startNs{};
			bool counted{};         // false when it was folded into its parent (no room)
		};
		struct Session {
			bool active{};
			uint32_t id{};
			int64_t startNs{};
			int64_t endNs{};
			uint32_t frames{};
			int64_t totalNs{};
			bool truncated{};
			std::vector<LiveNode> nodes;
			std::function<void(Profile&&)> done;
		};

		static uint32_t Child(std::vector<LiveNode>& nodes, uint32_t parent, const char* name, uint64_t arg, size_t maxNodes, bool& full);
		// The `limit` heaviest nodes (and so their parents) in pre-order, children by first start or heaviest first
		static std::vector<Node> Flatten(const std::vector<LiveNode>& nodes, size_t limit, bool byStart, bool& truncated);
		Frame MakeFrame(int64_t durationNs, size_t scopes) const;
		void FinishSession(int64_t nowNs);
		void MergeIntoSession();

		// Main thread only
		bool m_InFrame{};
		bool m_Implicit{};
		int64_t m_FrameStartNs{};
		int64_t m_FrameUnixMs{};
		std::vector<LiveNode> m_Nodes;
		std::vector<Open> m_Stack;
		Phase m_Phase{ Phase::OTHER };
		int64_t m_PhaseStartNs{};
		std::array<int64_t, PHASES> m_PhaseNs{};
		Session m_Session;
		std::function<void(const Frame&)> m_SlowSink;

		// Shared with Take
		mutable std::mutex m_Mutex;
		uint32_t m_SlowThresholdMs{ 250 };
		std::map<int64_t, Second> m_Seconds;
		int64_t m_LastReported{};
		std::unordered_map<uint64_t, MessageTime> m_Messages;
		std::vector<Frame> m_Worst; // longest first
		std::vector<Frame> m_Slow;
	};

	// This process's recorder
	Recorder& Local();

	// Marks the calling thread as the one whose scopes count (each server's main); the others' do nothing
	void SetMainThread();
	bool IsMainThread();

	int64_t NowNs(); // steady clock
	int64_t UnixMs();

	// A name that lives as long as the process, for scope names made at run time (main thread)
	const char* Intern(const std::string& name);

	// A pass of the main loop begins or ends (FrameScope does both for a block); nothing off the main thread
	void BeginFrame();
	void EndFrame();

	// One pass of the main loop
	class FrameScope {
	public:
		FrameScope();
		~FrameScope();
		FrameScope(const FrameScope&) = delete;
		FrameScope& operator=(const FrameScope&) = delete;
	private:
		bool m_Active{};
	};

	// A named scope; with a phase, time inside it (less nested phases) counts as that phase
	class Scope {
	public:
		explicit Scope(const char* name, uint64_t arg = 0);
		Scope(const char* name, Phase phase);
		~Scope();
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;
	private:
		bool m_Active{};
		bool m_SetPhase{};
		Phase m_Previous{};
		uint64_t m_Tracy{}; // the Tracy zone, when built with DLU_TRACY
	};

	// Handling one packet: a PACKET scope named by its type, and its time counted for that type
	class PacketScope {
	public:
		PacketScope(const uint8_t* data, size_t length);
		~PacketScope();
		PacketScope(const PacketScope&) = delete;
		PacketScope& operator=(const PacketScope&) = delete;
	private:
		bool m_Active{};
		bool m_SetPhase{};
		Phase m_Previous{};
		uint64_t m_Key{};
		int64_t m_StartNs{};
		uint64_t m_Tracy{};
	};
}
