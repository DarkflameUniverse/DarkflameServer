#ifndef __PACKETRECORD__H__
#define __PACKETRECORD__H__

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

/**
 * One recorded packet of a packet capture (PacketCapture.h), as the servers pack them into batches and as the
 * dashboard appends them to a capture file: a fixed little-endian header, then the packet's bytes exactly as they
 * went over RakNet (the RakNet message ID first; for LU packets the 8 byte header follows).
 *
 * Files of records (a capture on the dashboard's disk, an exported bundle) are described in CaptureBundle.h.
 */
enum class eCaptureSource : uint8_t {
	UNKNOWN,
	AUTH,
	CHAT,
	WORLD,
	MASTER,
};

enum class ePacketDirection : uint8_t {
	RECEIVED, // by the server that recorded it
	SENT,     // by the server that recorded it
};

namespace PacketRecordFlags {
	constexpr uint8_t MASTER_LINK = 1 << 0; // on a server's own link to master (not its listening socket)
	constexpr uint8_t BROADCAST = 1 << 1;   // sent to everyone connected (peer is who it skipped)
	constexpr uint8_t CUT = 1 << 2;         // longer than the server keeps; `bits` is the full size
	constexpr uint8_t GAP = 1 << 3;         // not a packet: `bits` packets were lost here (a server's buffer was full)
}

#pragma pack(push, 1)
struct PacketRecordHeader {
	int64_t timeUs{};       // Unix time in microseconds
	uint32_t seq{};         // per server, from 1 when it first armed a capture; a jump means records were dropped
	uint8_t mask{};         // the capture slots this record belongs to (one bit each)
	uint8_t source{};       // eCaptureSource
	uint8_t direction{};    // ePacketDirection
	uint8_t flags{};        // PacketRecordFlags
	uint64_t peer{};        // the other end: IPv4 address << 16 | port (0: none)
	uint32_t accountId{};   // whose it is, when known
	int64_t characterId{};
	uint16_t zoneId{};      // the recording server's zone (worlds), else 0
	uint16_t instanceId{};
	uint32_t cloneId{};
	uint32_t bits{};        // the packet's full size in bits
	uint32_t length{};      // bytes of the packet stored after this header
};
#pragma pack(pop)
static_assert(sizeof(PacketRecordHeader) == 52, "The record header is part of the capture file format");

namespace PacketRecord {
	// Bytes of one packet kept; longer ones are cut (large replica constructions and character data fit)
	constexpr uint32_t MAX_BYTES = 256 * 1024;

	inline void Append(std::string& out, const PacketRecordHeader& header, const void* data) {
		const auto at = out.size();
		out.resize(at + sizeof(header) + header.length);
		std::memcpy(out.data() + at, &header, sizeof(header));
		if (header.length) std::memcpy(out.data() + at + sizeof(header), data, header.length);
	}

	/**
	 * Calls fn(header, bytes) for every record in `blob`, in order. Returns false if the blob ends in the middle of a
	 * record or a record is larger than records can be (what came before was still passed to fn).
	 */
	template<typename Fn>
	bool ForEach(std::string_view blob, Fn&& fn) {
		size_t at = 0;
		while (at < blob.size()) {
			if (blob.size() - at < sizeof(PacketRecordHeader)) return false;
			PacketRecordHeader header;
			std::memcpy(&header, blob.data() + at, sizeof(header));
			at += sizeof(header);
			if (header.length > MAX_BYTES || blob.size() - at < header.length) return false;
			fn(header, blob.substr(at, header.length));
			at += header.length;
		}
		return true;
	}

	// "a.b.c.d:port" of a record's peer
	inline std::string PeerText(uint64_t peer) {
		if (peer == 0) return "";
		const auto address = static_cast<uint32_t>(peer >> 16);
		// RakNet keeps IPv4 addresses in network order
		return std::to_string(address & 0xff) + "." + std::to_string((address >> 8) & 0xff) + "." + std::to_string((address >> 16) & 0xff) + "." +
			std::to_string(address >> 24) + ":" + std::to_string(peer & 0xffff);
	}
}

#endif  //!__PACKETRECORD__H__
