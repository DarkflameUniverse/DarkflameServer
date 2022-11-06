#pragma once
#include <cstdint>
#include <string>
#include <array>

class GUID {
public:
	explicit GUID();
	explicit GUID(const std::string& guid);
	uint32_t GetData1() const;
	uint16_t GetData2() const;
	uint16_t GetData3() const;
	std::array<uint8_t, 8> GetData4() const;
private:
	uint32_t data1 = 0;
	uint16_t data2 = 0;
	uint16_t data3 = 0;
	std::array<uint8_t, 8> data4 = { 0, 0, 0, 0, 0, 0, 0, 0 };
};
