#pragma once
#include <cstdint>
#include <string>
#include <array>

const std::string EMPTY_GUID = "{00000000-0000-0000-0000-000000000000}";

class GUID {
public:
	explicit GUID();
	explicit GUID(const std::string& guid);
	void Serialize(RakNet::BitStream* outBitStream);

	uint32_t GetData1() const {
		return data1;
	}

	uint16_t GetData2() const {
		return data2;
	}

	uint16_t GetData3() const {
		return data3;
	}

	std::array<uint8_t, 8> GetData4() const {
		return data4;
	}

private:
	uint32_t data1 = 0;
	uint16_t data2 = 0;
	uint16_t data3 = 0;
	std::array<uint8_t, 8> data4 = { 0, 0, 0, 0, 0, 0, 0, 0 };
};
