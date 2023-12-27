#include "BinaryIO.h"
#include <string>

//For reading null-terminated strings
std::string BinaryIO::ReadString(std::istream& instream) {
	std::string toReturn;
	char buffer;

	BinaryIO::BinaryRead(instream, buffer);

	while (buffer != 0x00) {
		toReturn += buffer;
		BinaryRead(instream, buffer);
	}

	return toReturn;
}
