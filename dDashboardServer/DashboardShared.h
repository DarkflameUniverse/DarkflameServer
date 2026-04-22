#ifndef __DASHBOARDSHARED_H__
#define __DASHBOARDSHARED_H__

#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <queue>
#include <functional>
#include <set>
#include <map>
#include <ctime>
#include <random>
#include <optional>
#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "GameDatabase.h"
#include "crow.h"

// Forward declaration
class GameDatabase;
namespace RakNet {
	class BitStream;
};

/**
 * Shared state between the Crow web server (runs in background threads)
 * and the RakNet game loop (runs in main thread).
 *
 * All members use thread-safe types (atomic, mutex-protected)
 *
 * IMPORTANT: RakNet is NOT thread-safe!
 * - Crow threads can READ state and QUEUE packet send requests
 * - Only the RakNet thread (main loop) can actually send packets
 */
namespace DashboardShared {
	
	// ===== Atomic Counters (lock-free, safe for simple reads/writes) =====
	
	inline std::atomic<uint32_t> g_ConnectedClients{0};
	inline std::atomic<bool> g_ConnectedToMaster{false};
	inline std::atomic<uint64_t> g_PacketsReceived{0};
	inline std::atomic<uint64_t> g_PacketsSent{0};
	
	// ===== Mutex-Protected Data (for complex structures) =====
	
	struct ServerStats {
		std::mutex mutex;
		uint64_t uptime_seconds = 0;
		std::string last_packet_type;
		uint32_t raknet_port = 0;
		std::string master_ip;
		
		// Thread-safe getters
		uint64_t GetUptime() {
			std::lock_guard<std::mutex> lock(mutex);
			return uptime_seconds;
		}
		
		std::string GetLastPacketType() {
			std::lock_guard<std::mutex> lock(mutex);
			return last_packet_type;
		}
		
		void SetLastPacketType(const std::string& type) {
			std::lock_guard<std::mutex> lock(mutex);
			last_packet_type = type;
		}
		
		void SetMasterInfo(const std::string& ip, uint32_t port) {
			std::lock_guard<std::mutex> lock(mutex);
			master_ip = ip;
			raknet_port = port;
		}
	};
	
	inline ServerStats g_Stats;
	
	// ===== Packet Send Queue (for Crow -> RakNet communication) =====
	
	/**
	 * Represents a packet send request from Crow to RakNet.
	 * Crow threads add to the queue, RakNet thread processes them.
	 */
	struct PacketSendRequest {
		std::vector<uint8_t> data;      // Packet data (owns the memory)
		SystemAddress target;            // Target address (or UNASSIGNED for broadcast)
		bool broadcast;                  // Whether to broadcast
		
		PacketSendRequest(const std::vector<uint8_t>& packetData, 
		                  const SystemAddress& addr, 
		                  bool isBroadcast)
			: data(packetData), target(addr), broadcast(isBroadcast) {}
	};
	
	// Thread-safe queue of packet send requests
	struct PacketQueue {
		std::mutex mutex;
		std::queue<PacketSendRequest> queue;
		
		// Called from Crow threads to queue a packet for sending
		void Enqueue(const std::vector<uint8_t>& data, const SystemAddress& addr, bool broadcast) {
			std::lock_guard<std::mutex> lock(mutex);
			queue.emplace(data, addr, broadcast);
		}
		
		// Called from RakNet thread to get all pending packets
		std::vector<PacketSendRequest> DequeueAll() {
			std::lock_guard<std::mutex> lock(mutex);
			std::vector<PacketSendRequest> result;
			while (!queue.empty()) {
				result.push_back(std::move(queue.front()));
				queue.pop();
			}
			return result;
		}
		
		// Check if queue has pending packets
		bool HasPending() {
			std::lock_guard<std::mutex> lock(mutex);
			return !queue.empty();
		}
	};
	
	inline PacketQueue g_PacketQueue;
	
	// ===== Helper Functions =====
	
	// Called from RakNet thread when a client connects
	inline void OnClientConnected() {
		g_ConnectedClients++;
	}
	
	// Called from RakNet thread when a client disconnects
	inline void OnClientDisconnected() {
		if (g_ConnectedClients > 0) {
			g_ConnectedClients--;
		}
	}
	
	// Called from RakNet thread when master connection status changes
	inline void SetMasterConnected(bool connected) {
		g_ConnectedToMaster = connected;
	}
	
	// Called from RakNet thread when a packet is processed
	inline void OnPacketReceived(const std::string& packetType = "") {
		g_PacketsReceived++;
		if (!packetType.empty()) {
			g_Stats.SetLastPacketType(packetType);
		}
	}
	
	// Called from RakNet thread when a packet is sent
	inline void OnPacketSent() {
		g_PacketsSent++;
	}
	
	// ===== Crow -> RakNet Communication =====
	
	/**
	 * Queue a RakNet packet to be sent (called from Crow threads).
	 * The packet will be sent on the next RakNet thread update.
	 * 
	 * @param data Packet data to send
	 * @param target Target system address (use UNASSIGNED_SYSTEM_ADDRESS for broadcast)
	 * @param broadcast Whether to broadcast to all connected clients
	 */
	inline void QueuePacketSend(const std::vector<uint8_t>& data, 
	                             const SystemAddress& target = UNASSIGNED_SYSTEM_ADDRESS,
	                             bool broadcast = false) {
		g_PacketQueue.Enqueue(data, target, broadcast);
	}
	
	/**
	 * Helper to queue a BitStream for sending (called from Crow threads).
	 * Converts BitStream to raw data and queues it.
	 */
	inline void QueueBitStreamSend(RakNet::BitStream& bitStream,
	                                const SystemAddress& target = UNASSIGNED_SYSTEM_ADDRESS,
	                                bool broadcast = false) {
		std::vector<uint8_t> data(bitStream.GetData(), 
		                          bitStream.GetData() + bitStream.GetNumberOfBytesUsed());
		QueuePacketSend(data, target, broadcast);
	}
}
#endif // __DASHBOARDSHARED_H__
