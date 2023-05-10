#include "GUID.h"

GUID::GUID(const std::string& guid) {
	sscanf(guid.c_str(),
		"{%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx}",
		&this->data1, &this->data2, &this->data3,
		&this->data4[0], &this->data4[1], &this->data4[2], &this->data4[3],
		&this->data4[4], &this->data4[5], &this->data4[6], &this->data4[7]);
}

uint32_t GUID::GetData1() const {
	return data1;
}

uint16_t GUID::GetData2() const {
	return data2;
}

uint16_t GUID::GetData3() const {
	return data3;
}

std::array<uint8_t, 8> GUID::GetData4() const {
	return data4;
}

GUID::GUID() = default;
