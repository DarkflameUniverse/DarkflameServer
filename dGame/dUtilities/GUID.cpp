#include "GUID.h"

namespace {
	const std::string EMPTY_GUID = "{00000000-0000-0000-0000-000000000000}";
}

GUID::GUID(const std::string& guid) {
	if(guid == EMPTY_GUID) return;
	sscanf(guid.c_str(),
		"{%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx}",
		&this->data1, &this->data2, &this->data3,
		&this->data4[0], &this->data4[1], &this->data4[2], &this->data4[3],
		&this->data4[4], &this->data4[5], &this->data4[6], &this->data4[7]);
}

void GUID::Serialize(RakNet::BitStream* outBitStream) {
	outBitStream->Write(GetData1());
	outBitStream->Write(GetData2());
	outBitStream->Write(GetData3());
	for (const auto& guidSubPart : GetData4()) {
		outBitStream->Write(guidSubPart);
	}
}

GUID::GUID() = default;
