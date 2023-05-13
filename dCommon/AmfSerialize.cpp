#include "AmfSerialize.h"

#include "Game.h"
#include "dLogger.h"

// Writes an AMFValue pointer to a RakNet::BitStream
template<>
void RakNet::BitStream::Write<AMFBaseValue&>(AMFBaseValue& value) {
	eAmf type = value.GetValueType();
	this->Write(type);
	switch (type) {
	case eAmf::Integer: {
		this->Write<AMFIntValue&>(*static_cast<AMFIntValue*>(&value));
		break;
	}

	case eAmf::Double: {
		this->Write<AMFDoubleValue&>(*static_cast<AMFDoubleValue*>(&value));
		break;
	}

	case eAmf::String: {
		this->Write<AMFStringValue&>(*static_cast<AMFStringValue*>(&value));
		break;
	}

	case eAmf::Array: {
		this->Write<AMFArrayValue&>(*static_cast<AMFArrayValue*>(&value));
		break;
	}
	default: {
		Game::logger->Log("AmfSerialize", "Encountered unwritable AMFType %i!", type);
	}
	case eAmf::Undefined:
	case eAmf::Null:
	case eAmf::False:
	case eAmf::True:
	case eAmf::Date:
	case eAmf::Object:
	case eAmf::XML:
	case eAmf::XMLDoc:
	case eAmf::ByteArray:
	case eAmf::VectorInt:
	case eAmf::VectorUInt:
	case eAmf::VectorDouble:
	case eAmf::VectorObject:
	case eAmf::Dictionary:
		break;
	}
}

/**
 * A private function to write an value to a RakNet::BitStream
 * RakNet writes in the correct byte order - do not reverse this.
 */
void WriteUInt29(RakNet::BitStream* bs, uint32_t v) {
	unsigned char b4 = (unsigned char)v;
	if (v < 0x00200000) {
		b4 = b4 & 0x7F;
		if (v > 0x7F) {
			unsigned char b3;
			v = v >> 7;
			b3 = ((unsigned char)(v)) | 0x80;
			if (v > 0x7F) {
				unsigned char b2;
				v = v >> 7;
				b2 = ((unsigned char)(v)) | 0x80;
				bs->Write(b2);
			}

			bs->Write(b3);
		}
	} else {
		unsigned char b1;
		unsigned char b2;
		unsigned char b3;

		v = v >> 8;
		b3 = ((unsigned char)(v)) | 0x80;
		v = v >> 7;
		b2 = ((unsigned char)(v)) | 0x80;
		v = v >> 7;
		b1 = ((unsigned char)(v)) | 0x80;

		bs->Write(b1);
		bs->Write(b2);
		bs->Write(b3);
	}

	bs->Write(b4);
}

/**
 * Writes a flag number to a RakNet::BitStream
 * RakNet writes in the correct byte order - do not reverse this.
 */
void WriteFlagNumber(RakNet::BitStream* bs, uint32_t v) {
	v = (v << 1) | 0x01;
	WriteUInt29(bs, v);
}

/**
 * Writes an AMFString to a RakNet::BitStream
 *
 * RakNet writes in the correct byte order - do not reverse this.
 */
void WriteAMFString(RakNet::BitStream* bs, const std::string& str) {
	WriteFlagNumber(bs, (uint32_t)str.size());
	bs->Write(str.c_str(), (uint32_t)str.size());
}

/**
 * Writes an U16 to a bitstream
 *
 * RakNet writes in the correct byte order - do not reverse this.
 */
void WriteAMFU16(RakNet::BitStream* bs, uint16_t value) {
	bs->Write(value);
}

/**
 * Writes an U32 to a bitstream
 *
 * RakNet writes in the correct byte order - do not reverse this.
 */
void WriteAMFU32(RakNet::BitStream* bs, uint32_t value) {
	bs->Write(value);
}

/**
 * Writes an U64 to a bitstream
 *
 * RakNet writes in the correct byte order - do not reverse this.
 */
void WriteAMFU64(RakNet::BitStream* bs, uint64_t value) {
	bs->Write(value);
}

// Writes an AMFIntegerValue to BitStream
template<>
void RakNet::BitStream::Write<AMFIntValue&>(AMFIntValue& value) {
	WriteUInt29(this, value.GetValue());
}

// Writes an AMFDoubleValue to BitStream
template<>
void RakNet::BitStream::Write<AMFDoubleValue&>(AMFDoubleValue& value) {
	double d = value.GetValue();
	WriteAMFU64(this, *reinterpret_cast<uint64_t*>(&d));
}

// Writes an AMFStringValue to BitStream
template<>
void RakNet::BitStream::Write<AMFStringValue&>(AMFStringValue& value) {
	WriteAMFString(this, value.GetValue());
}

// Writes an AMFArrayValue to BitStream
template<>
void RakNet::BitStream::Write<AMFArrayValue&>(AMFArrayValue& value) {
	uint32_t denseSize = value.GetDense().size();
	WriteFlagNumber(this, denseSize);

	auto it = value.GetAssociative().begin();
	auto end = value.GetAssociative().end();

	while (it != end) {
		WriteAMFString(this, it->first);
		this->Write<AMFBaseValue&>(*it->second);
		it++;
	}

	this->Write(eAmf::Null);

	if (denseSize > 0) {
		auto it2 = value.GetDense().begin();
		auto end2 = value.GetDense().end();

		while (it2 != end2) {
			this->Write<AMFBaseValue&>(**it2);
			it2++;
		}
	}
}
