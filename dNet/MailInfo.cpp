#include "MailInfo.h"
#include "BitStream.h"
#include "DluAssert.h"

void MailInfo::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(id);
	const LUWString subject(this->subject, 50);
	bitStream.Write(subject);
	const LUWString body(this->body, 400);
	bitStream.Write(body);
	const LUWString sender(this->senderUsername, 32);
	bitStream.Write(sender);
	bitStream.Write<uint32_t>(0); // packing

	bitStream.Write<uint64_t>(0); // attachedCurrency
	bitStream.Write(itemID);

	LOT lot = itemLOT;
	if (lot <= 0) bitStream.Write<LOT>(LOT_NULL);
	else bitStream.Write(lot);
	bitStream.Write<uint32_t>(0); // packing

	bitStream.Write(itemSubkey);

	bitStream.Write(itemCount);
	bitStream.Write<uint8_t>(0); // subject type (used for auction)
	bitStream.Write<uint8_t>(0); // packing
	bitStream.Write<uint32_t>(0); //  packing

	bitStream.Write<uint64_t>(timeSent); // expiration date
	bitStream.Write<uint64_t>(timeSent);// send date
	bitStream.Write<uint8_t>(wasRead); // was read

	bitStream.Write<uint8_t>(0); // isLocalized
	bitStream.Write<uint16_t>(1033); // language code
	bitStream.Write<uint32_t>(0); // packing
}

bool MailInfo::Deserialize(RakNet::BitStream& bitStream) {
	LUWString subject(50);
	VALIDATE_READ(bitStream.Read(subject));
	this->subject = subject.GetAsString();

	LUWString body(400);
	VALIDATE_READ(bitStream.Read(body));
	this->body = body.GetAsString();

	LUWString recipientName(32);
	VALIDATE_READ(bitStream.Read(recipientName));
	this->recipient = recipientName.GetAsString();

	uint64_t unknown;
	VALIDATE_READ(bitStream.Read(unknown));

	VALIDATE_READ(bitStream.Read(itemID));
	VALIDATE_READ(bitStream.Read(itemCount));
	VALIDATE_READ(bitStream.Read(languageCode));
	bitStream.IgnoreBytes(4); // padding

	DluAssert(bitStream.GetNumberOfUnreadBits() == 0);
	LOG_DEBUG("MailInfo: %llu, %s, %s, %s, %llu, %i, %llu, %i, %llu, %i", id, subject.GetAsString().c_str(), body.GetAsString().c_str(), recipientName.GetAsString().c_str(), itemID, itemLOT, itemSubkey, itemCount, timeSent, wasRead);

	return true;
}
