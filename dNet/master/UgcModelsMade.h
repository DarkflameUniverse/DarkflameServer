#ifndef __UGCMODELSMADE__H__
#define __UGCMODELSMADE__H__

#include <algorithm>
#include <cstdint>
#include <vector>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "MessageType/Master.h"
#include "dCommonVars.h"

/**
 * UGC_MODELS_MADE payload (UGC server -> master -> every world): the player models (their ugc id, which is the
 * blueprint id) whose mesh the UGC server just made or made again. A world showing one of them tells its clients the
 * new checksum and sends NotifyClientUGCModelReady, so they load the served mesh (docs/UgcServer.md).
 */
struct UgcModelsMade : public LUBitStream {
	UgcModelsMade() : LUBitStream(ServiceType::MASTER, MessageType::Master::UGC_MODELS_MADE) {}

	static constexpr size_t MAX_MODELS = 4096;

	std::vector<LWOOBJID> blueprintIds;

	void Serialize(RakNet::BitStream& stream) const override {
		const auto count = static_cast<uint16_t>(std::min(blueprintIds.size(), MAX_MODELS));
		stream.Write(count);
		for (size_t i = 0; i < count; i++) stream.Write(blueprintIds[i]);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint16_t count{};
		if (!stream.Read(count) || count > MAX_MODELS) return false;
		blueprintIds.resize(count);
		for (auto& id : blueprintIds) {
			if (!stream.Read(id)) return false;
		}
		return true;
	}
};

#endif  //!__UGCMODELSMADE__H__
