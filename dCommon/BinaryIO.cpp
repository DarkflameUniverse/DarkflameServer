#include "BinaryIO.h"
#include <string>

//For reading null-terminated strings
template<typename StringType>
StringType ReadString(std::istream& instream) {
	StringType toReturn{};
	typename StringType::value_type buffer{};

	BinaryIO::BinaryRead(instream, buffer);

	while (buffer != 0x00) {
		toReturn += buffer;
		BinaryIO::BinaryRead(instream, buffer);
	}

	return toReturn;
}

std::string BinaryIO::ReadString(std::istream& instream) {
	return ::ReadString<std::string>(instream);
}

std::u8string BinaryIO::ReadU8String(std::istream& instream) {
	return ::ReadString<std::u8string>(instream);
}
