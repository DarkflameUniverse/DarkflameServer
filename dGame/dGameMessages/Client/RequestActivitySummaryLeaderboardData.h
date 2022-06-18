#pragma once

#include "../GameMessage.h"

class RequestActivitySummaryLeaderboardData : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_REQUEST_ACTIVITY_SUMMARY_LEADERBOARD_DATA; }

    int32_t gameID = 0;
    int32_t queryType = 1;
    int32_t resultsEnd = 10;
    int32_t resultsStart = 0;
    LWOOBJID target{};
    bool weekly;

	void Deserialize(RakNet::BitStream* inStream) override {
        if (inStream->ReadBit()) inStream->Read(gameID);
        if (inStream->ReadBit()) inStream->Read(queryType);
        if (inStream->ReadBit()) inStream->Read(resultsEnd);
        if (inStream->ReadBit()) inStream->Read(resultsStart);
        inStream->Read(target);
	    weekly = inStream->ReadBit();
    };
}; 