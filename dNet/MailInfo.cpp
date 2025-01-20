#include "MailInfo.h"
#include "BitStream.h"
#include "DluAssert.h"

void MailInfo::Serialize(RakNet::BitStream& bitStream) const {
	LOG("Writing MailInfo");
	LOG("ID: %llu", id);
	bitStream.Write(id);
	LOG("Subject: %s", subject.c_str());
	const LUWString subject(this->subject, 50);
	bitStream.Write(subject);
	LOG("Body: %s", body.c_str());
	const LUWString body(this->body, 400);
	bitStream.Write(body);
	LOG("Sender: %s", senderUsername.c_str());
	const LUWString sender(this->senderUsername, 32);
	bitStream.Write(sender);
	bitStream.Write<uint32_t>(0); // packing

	bitStream.Write<uint64_t>(0); // attachedCurrency
	LOG("ItemID: %llu", itemID);
	bitStream.Write(itemID);

	LOT lot = itemLOT;
	LOG("ItemLOT: %u", lot);
	if (lot <= 0) bitStream.Write<LOT>(LOT_NULL);
	else bitStream.Write(lot);
	bitStream.Write<uint32_t>(0); // packing

	bitStream.Write(itemSubkey);

	bitStream.Write<uint16_t>(itemCount);
	bitStream.Write<uint8_t>(0); // subject type (used for auction)
	bitStream.Write<uint8_t>(0); // packing
	bitStream.Write<uint32_t>(0); //  packing

	bitStream.Write<uint64_t>(timeSent); // expiration date
	bitStream.Write<uint64_t>(timeSent);// send date
	bitStream.Write<uint8_t>(wasRead); // was read

	bitStream.Write<uint8_t>(0); // isLocalized
	bitStream.Write<uint16_t>(0); // packing
	bitStream.Write<uint32_t>(0); // packing
}

bool MailInfo::Deserialize(RakNet::BitStream& bitStream) {
	VALIDATE_READ(bitStream.Read(id));

	LUWString subject(50);
	VALIDATE_READ(bitStream.Read(subject));
	this->subject = subject.GetAsString();

	LUWString body(400);
	VALIDATE_READ(bitStream.Read(body));
	this->body = body.GetAsString();

	LUWString sender(32);
	VALIDATE_READ(bitStream.Read(sender));
	this->senderUsername = sender.GetAsString();

	bitStream.IgnoreBytes(4); // packing

	bitStream.IgnoreBytes(8); // attachedCurrency
	VALIDATE_READ(bitStream.Read(itemID));

	LOT lot;
	VALIDATE_READ(bitStream.Read(lot));
	if (lot == LOT_NULL) itemLOT = 0;
	else itemLOT = lot;
	bitStream.IgnoreBytes(4); // packing

	VALIDATE_READ(bitStream.Read(itemSubkey));

	VALIDATE_READ(bitStream.Read(itemCount));

	bitStream.IgnoreBytes(1); // subject type (used for auction)
	bitStream.IgnoreBytes(1); // packing
	bitStream.IgnoreBytes(4); // packing

	VALIDATE_READ(bitStream.Read(timeSent)); // expiration date
	VALIDATE_READ(bitStream.Read(timeSent)); // send date
	VALIDATE_READ(bitStream.Read(wasRead)); // was read

	bitStream.IgnoreBytes(1); // isLocalized
	bitStream.IgnoreBytes(2); // packing
	bitStream.IgnoreBytes(4); // packing

	DluAssert(bitStream.GetNumberOfUnreadBits() == 0);

	return true;
}