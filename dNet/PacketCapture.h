#ifndef __PACKETCAPTURE__H__
#define __PACKETCAPTURE__H__

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "dCommonVars.h"
#include "PacketRecord.h"

class RakPeerInterface;
struct MessageCaptureControl;
struct MessageCaptureData;
struct Packet;
struct SystemAddress;
enum class ServiceType : uint16_t;

/**
 * This server's side of the dashboard's packet capture (docs/CaptureReplay.md): records whole packets, as they went
 * over RakNet, for up to MessageCapture::MAX_SLOTS armed captures at once, and ships them to the dashboard in batches.
 *
 * Every server runs one (auth, chat, world and master): the dashboard arms a capture with MESSAGE_CAPTURE_CONTROL
 * ARM, master passes it to every server, and each records its part:
 *  - auth and world: what their clients send and receive (world: everything, replica constructions included, since
 *    the tap sits in RakPeer::Send), plus their own master link messages that belong to a captured player
 *    (session keys, zone transfers, player added/removed, instance migration);
 *  - chat: what worlds route to and from it (the player is the object ID each chat packet starts with);
 *  - master: for an EVERYTHING capture, all server-to-server traffic except the dashboard's.
 *
 * Nothing is written to disk here and nothing is sent per packet: records are appended to one preallocated chunk,
 * which is sealed when it reaches capture_flush_bytes or capture_flush_interval_ms has passed, and sent from the
 * main loop (Update). Sealed chunks waiting to be sent (master unreachable) are capped at capture_buffer_max_mb; the
 * oldest are dropped past that and counted, so the dashboard shows a gap. With nothing armed the cost is one flag
 * check per received packet and one null check per sent one.
 *
 * RakNet isn't thread safe: everything here runs on the server's main thread (sends from the main thread, receives
 * where the main loop reads them).
 */
namespace PacketCapture {
	extern bool g_Armed;
	extern bool g_Tracking; // connections are bound (they are forgotten when they close, armed or not)

	// dServer: which peer is the listening one and which the link to master (nullptr on master itself)
	void Attach(ServiceType serverType, RakPeerInterface* peer, RakPeerInterface* masterLink, uint32_t zoneId, uint32_t instanceId);
	void Detach();

	// Worlds: their clone (a property's owner), once known
	void SetClone(uint32_t cloneId);

	// Master: packets to and from the dashboard aren't recorded
	void IgnorePeer(const SystemAddress& address);

	// ARM or DISARM from the dashboard (START/STOP are the game message inspector's)
	void Control(const MessageCaptureControl& control);

	// A packet the listening peer received / nullptr when there are no more this frame (dServer::Receive)
	void OnReceiveImpl(const Packet* packet);
	inline void OnReceive(const Packet* packet) { if (g_Armed || g_Tracking) OnReceiveImpl(packet); }

	// A packet from master (dServer::ReceiveFromMaster)
	void OnReceiveFromMasterImpl(const Packet* packet);
	inline void OnReceiveFromMaster(const Packet* packet) { if (g_Armed) OnReceiveFromMasterImpl(packet); }

	// Who is on the other end of a connection: auth when an account logs in (the account may not exist: 0), worlds
	// when a session is validated, and the character when one is picked
	void Bind(const SystemAddress& address, uint32_t accountId, const std::string& accountName);
	void BindCharacter(const SystemAddress& address, LWOOBJID characterId);

	// Main loop: seals the chunk when it is due, sends sealed chunks, ends captures past their time
	void Update();

	/**
	 * Where sealed chunks go; returns false when they can't be sent right now (they are kept, up to the cap). The
	 * default sends them to master; master sends them straight to the dashboard.
	 */
	using Sink = std::function<bool(MessageCaptureData& data)>;
	void SetSink(Sink sink);

	struct Settings {
		uint32_t flushIntervalMs{ 1000 };
		uint32_t flushBytes{ 256 * 1024 };
		uint64_t maxBufferBytes{ 16ull * 1024 * 1024 };
	};
	// Read from capture_flush_interval_ms, capture_flush_bytes and capture_buffer_max_mb when a capture is armed
	void SetSettings(const Settings& settings);

	struct Stats {
		uint64_t recorded{};      // records kept
		uint64_t recordedBytes{}; // their size, headers included
		uint64_t dropped{};       // records dropped over the buffer cap
		uint64_t batches{};       // chunks sent
		uint64_t sentBytes{};
	};
	const Stats& GetStats();

	// Tests: drop every capture, binding and buffer (the attachment stays)
	void Reset();

	// Tests and tools: record as if the listening peer had sent (broadcast: to all but `address`) or received this
	void RecordForTest(const SystemAddress& address, bool sent, bool broadcast, const unsigned char* data, uint32_t bits);
}

#endif  //!__PACKETCAPTURE__H__
