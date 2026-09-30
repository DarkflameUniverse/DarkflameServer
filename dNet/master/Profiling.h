#ifndef __PROFILING__H__
#define __PROFILING__H__

#include <algorithm>
#include <cstdint>
#include <string>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "MessageType/Master.h"
#include "Profiler.h"
#include "ServiceType.h"
#include "master/ServerTraffic.h"

/**
 * Profiling sessions from the dashboard (docs/Dashboard.md, "Performance"). PROFILE_REQUEST (dashboard -> master -> the
 * server named by type, zone and instance) starts or stops a session; the server merges its main loop's scope trees
 * (Profiler.h) for the time asked, at most a minute, and answers with PROFILE_RESULT (server -> master -> dashboard):
 * STARTED at once, then DONE with the merged tree, or FAILED with the reason. Master answers FAILED itself when the
 * server isn't running. A server runs one session at a time.
 */
enum class eProfileStatus : uint8_t {
	STARTED,
	DONE,
	FAILED,
};

struct ProfileRequest : public LUBitStream {
	ProfileRequest() : LUBitStream(ServiceType::MASTER, MessageType::Master::PROFILE_REQUEST) {}

	uint32_t sessionId{};
	ServiceType serverType{};
	uint32_t zoneId{};
	uint32_t instanceId{};
	uint32_t durationMs{};
	bool stop{}; // end the session early

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(sessionId);
		stream.Write(serverType);
		stream.Write(zoneId);
		stream.Write(instanceId);
		stream.Write(durationMs);
		stream.Write(static_cast<uint8_t>(stop ? 1 : 0));
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint8_t flags{};
		if (!stream.Read(sessionId) || !stream.Read(serverType) || !stream.Read(zoneId) || !stream.Read(instanceId) || !stream.Read(durationMs) ||
			!stream.Read(flags)) return false;
		stop = (flags & 1) != 0;
		return true;
	}
};

struct ProfileResult : public LUBitStream {
	ProfileResult() : LUBitStream(ServiceType::MASTER, MessageType::Master::PROFILE_RESULT) {}

	static constexpr uint32_t MAX_NODES = 20000;

	uint32_t sessionId{};
	ServiceType serverType{};
	uint32_t zoneId{};
	uint32_t instanceId{};
	eProfileStatus status{};
	std::string error;         // FAILED only
	Profiler::Profile profile; // DONE only

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(sessionId);
		stream.Write(serverType);
		stream.Write(zoneId);
		stream.Write(instanceId);
		stream.Write(status);
		ServerTraffic::WriteText(stream, error);
		stream.Write(profile.durationMs);
		stream.Write(profile.frames);
		stream.Write(profile.totalUs);
		stream.Write(static_cast<uint8_t>(profile.truncated ? 1 : 0));
		ServerTraffic::WriteScopes(stream, profile.nodes, MAX_NODES);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint8_t truncated{};
		if (!stream.Read(sessionId) || !stream.Read(serverType) || !stream.Read(zoneId) || !stream.Read(instanceId) || !stream.Read(status) ||
			!ServerTraffic::ReadText(stream, error) || !stream.Read(profile.durationMs) || !stream.Read(profile.frames) || !stream.Read(profile.totalUs) ||
			!stream.Read(truncated)) return false;
		if (status > eProfileStatus::FAILED) return false;
		profile.id = sessionId;
		profile.truncated = truncated != 0;
		return ServerTraffic::ReadScopes(stream, profile.nodes, MAX_NODES);
	}
};

#endif  //!__PROFILING__H__
