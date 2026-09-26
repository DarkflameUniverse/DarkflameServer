#include "Stamps.h"

#include "BitStreamUtils.h"
#include "Logger.h"
#include "StringifiedEnum.h"

static_assert(sizeof(Stamp) == 16, "the login response's stamp size field has always been 16 bytes per stamp");

void Stamp::Serialize(RakNet::BitStream& outBitStream) const {
	outBitStream.Write(type);
	outBitStream.Write(value);
	outBitStream.Write(timestamp);
}

bool Stamp::Deserialize(RakNet::BitStream& inBitStream) {
	VALIDATE_READ(inBitStream.Read(type));
	VALIDATE_READ(inBitStream.Read(value));
	VALIDATE_READ(inBitStream.Read(timestamp));
	return true;
}

void Stamps::Add(const eStamps type, const uint32_t value) {
	list.emplace_back(type, value, static_cast<uint64_t>(std::time(nullptr)));
}

void Stamps::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write<uint32_t>((sizeof(Stamp) * list.size()) + sizeof(uint32_t));
	for (const auto& stamp : list) stamp.Serialize(bitStream);
}

bool Stamps::Deserialize(RakNet::BitStream& bitStream) {
	uint32_t stampsSize{};
	VALIDATE_READ(bitStream.Read(stampsSize));
	if (stampsSize < sizeof(uint32_t) || (stampsSize - sizeof(uint32_t)) % sizeof(Stamp) != 0) return false;
	const uint32_t stampCount = (stampsSize - sizeof(uint32_t)) / sizeof(Stamp);
	if (stampCount > BITS_TO_BYTES(bitStream.GetNumberOfUnreadBits()) / sizeof(Stamp)) return false;
	list.resize(stampCount);
	for (auto& stamp : list) VALIDATE_READ(stamp.Deserialize(bitStream));
	return true;
}

void Stamps::Log(const std::string& context) const {
	if (list.empty()) return;
	const auto start = list.front().timestamp;
	auto last = start;
	for (const auto& stamp : list) {
		// The same line the client logs for each stamp it receives
		LOG_DEBUG("%s: stamp %s(%u) at %llu (start+%lld, last+%lld)", context.c_str(), StringifiedEnum::ToString(stamp.type).data(), stamp.value,
			stamp.timestamp, static_cast<long long>(stamp.timestamp - start), static_cast<long long>(stamp.timestamp - last));
		last = stamp.timestamp;
	}
}
