#include "AMFFormat_BitStream.h"

// Writes an AMFValue pointer to a RakNet::BitStream
template<>
void RakNet::BitStream::Write<AMFValue*>(AMFValue* value) {
	if (value != nullptr) {
		AMFValueType type = value->GetValueType();

		switch (type) {
		case AMFUndefined: {
			AMFUndefinedValue* v = (AMFUndefinedValue*)value;
			this->Write(*v);
			break;
		}

		case AMFNull: {
			AMFNullValue* v = (AMFNullValue*)value;
			this->Write(*v);
			break;
		}

		case AMFFalse: {
			AMFFalseValue* v = (AMFFalseValue*)value;
			this->Write(*v);
			break;
		}

		case AMFTrue: {
			AMFTrueValue* v = (AMFTrueValue*)value;
			this->Write(*v);
			break;
		}

		case AMFInteger: {
			AMFIntegerValue* v = (AMFIntegerValue*)value;
			this->Write(*v);
			break;
		}

		case AMFDouble: {
			AMFDoubleValue* v = (AMFDoubleValue*)value;
			this->Write(*v);
			break;
		}

		case AMFString: {
			AMFStringValue* v = (AMFStringValue*)value;
			this->Write(*v);
			break;
		}

		case AMFXMLDoc: {
			AMFXMLDocValue* v = (AMFXMLDocValue*)value;
			this->Write(*v);
			break;
		}

		case AMFDate: {
			AMFDateValue* v = (AMFDateValue*)value;
			this->Write(*v);
			break;
		}

		case AMFArray: {
			this->Write((AMFArrayValue*)value);
			break;
		}
		}
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


// Writes an AMFUndefinedValue to BitStream
template<>
void RakNet::BitStream::Write<AMFUndefinedValue>(AMFUndefinedValue value) {
	this->Write(AMFUndefined);
}

// Writes an AMFNullValue to BitStream
template<>
void RakNet::BitStream::Write<AMFNullValue>(AMFNullValue value) {
	this->Write(AMFNull);
}

// Writes an AMFFalseValue to BitStream
template<>
void RakNet::BitStream::Write<AMFFalseValue>(AMFFalseValue value) {
	this->Write(AMFFalse);
}

// Writes an AMFTrueValue to BitStream
template<>
void RakNet::BitStream::Write<AMFTrueValue>(AMFTrueValue value) {
	this->Write(AMFTrue);
}

// Writes an AMFIntegerValue to BitStream
template<>
void RakNet::BitStream::Write<AMFIntegerValue>(AMFIntegerValue value) {
	this->Write(AMFInteger);
	WriteUInt29(this, value.GetIntegerValue());
}

// Writes an AMFDoubleValue to BitStream
template<>
void RakNet::BitStream::Write<AMFDoubleValue>(AMFDoubleValue value) {
	this->Write(AMFDouble);
	double d = value.GetDoubleValue();
	WriteAMFU64(this, *((unsigned long long*) & d));
}

// Writes an AMFStringValue to BitStream
template<>
void RakNet::BitStream::Write<AMFStringValue>(AMFStringValue value) {
	this->Write(AMFString);
	std::string v = value.GetStringValue();
	WriteAMFString(this, v);
}

// Writes an AMFXMLDocValue to BitStream
template<>
void RakNet::BitStream::Write<AMFXMLDocValue>(AMFXMLDocValue value) {
	this->Write(AMFXMLDoc);
	std::string v = value.GetXMLDocValue();
	WriteAMFString(this, v);
}

// Writes an AMFDateValue to BitStream
template<>
void RakNet::BitStream::Write<AMFDateValue>(AMFDateValue value) {
	this->Write(AMFDate);
	uint64_t date = value.GetDateValue();
	WriteAMFU64(this, date);
}

// Writes an AMFArrayValue to BitStream
template<>
void RakNet::BitStream::Write<AMFArrayValue*>(AMFArrayValue* value) {
	this->Write(AMFArray);
	uint32_t denseSize = value->GetDenseValueSize();
	WriteFlagNumber(this, denseSize);

	_AMFArrayMap_::iterator it = value->GetAssociativeIteratorValueBegin();
	_AMFArrayMap_::iterator end = value->GetAssociativeIteratorValueEnd();

	while (it != end) {
		WriteAMFString(this, it->first);
		this->Write(it->second);
		it++;
	}

	this->Write(AMFNull);

	if (denseSize > 0) {
		_AMFArrayList_::iterator it2 = value->GetDenseIteratorBegin();
		_AMFArrayList_::iterator end2 = value->GetDenseIteratorEnd();

		while (it2 != end2) {
			this->Write(*it2);
			it2++;
		}
	}
}
