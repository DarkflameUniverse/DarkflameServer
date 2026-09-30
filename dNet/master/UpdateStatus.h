#ifndef __UPDATESTATUS__H__
#define __UPDATESTATUS__H__

#include <cstdint>
#include <string>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "DashboardMessages.h"
#include "MessageType/Master.h"

/**
 * UPDATE_STATUS (dashboard -> master): what the dashboard's update check found, sent after each check and when the
 * link to master comes up, so master's log says when a newer release or newer commits are out. Nothing is updated.
 */
struct UpdateStatus : public LUBitStream {
	UpdateStatus() : LUBitStream(ServiceType::MASTER, MessageType::Master::UPDATE_STATUS) {}

	static constexpr uint16_t MAX_SUMMARY = 1024;

	// UpdateCheck::eState: 0 not checked, 1 up to date, 2 update available, 3 the check failed
	uint8_t state{};
	std::string summary; // one line for the log

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(state);
		DashboardMessages::WriteText(stream, summary, MAX_SUMMARY);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		return stream.Read(state) && DashboardMessages::ReadText(stream, summary, MAX_SUMMARY);
	}
};

#endif //!__UPDATESTATUS__H__
