#include "PropertySelectQueryProperty.h"

void PropertySelectQueryProperty::Serialize(RakNet::BitStream& stream) const {
	stream.Write(CloneId);

	stream.Write<uint32_t>(OwnerName.size());
	for (const auto& c : OwnerName) {
		stream.Write<uint16_t>(c);
	}

	stream.Write<uint32_t>(Name.size());
	for (const auto& c : Name) {
		stream.Write<uint16_t>(c);
	}

	stream.Write<uint32_t>(Description.size());
	for (const auto& c : Description) {
		stream.Write<uint16_t>(c);
	}

	stream.Write(Reputation);
	stream.Write(IsBestFriend);
	stream.Write(IsFriend);
	stream.Write(IsModeratorApproved);
	stream.Write(IsAlt);
	stream.Write(IsOwned);
	stream.Write(AccessType);
	stream.Write(DateLastPublished);
	stream.Write(PerformanceIndex);
	stream.Write(PerformanceCost);
}
