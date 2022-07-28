#include "BinaryIO.h"
#include <string>

void BinaryIO::WriteString(const std::string& stringToWrite, std::ofstream& outstream) {
	//BinaryWrite(outstream, uint32_t(stringToWrite.length()));

	for (size_t i = 0; i < size_t(stringToWrite.length()); ++i) {
		BinaryIO::BinaryWrite(outstream, stringToWrite[i]);
	}
}

//For reading null-terminated strings
std::string BinaryIO::ReadString(std::ifstream& instream) {
	std::string toReturn;
	char buffer;

	BinaryIO::BinaryRead(instream, buffer);

	while (buffer != 0x00) {
		toReturn += buffer;
		BinaryRead(instream, buffer);
	}

	return toReturn;
}

//For reading strings of a specific size
std::string BinaryIO::ReadString(std::ifstream& instream, size_t size) {
	std::string toReturn;
	char buffer;

	for (size_t i = 0; i < size; ++i) {
		BinaryIO::BinaryRead(instream, buffer);
		toReturn += buffer;
	}

	return toReturn;
}

std::string BinaryIO::ReadWString(std::ifstream& instream) {
	size_t size;
	BinaryRead(instream, size);
	//toReturn.resize(size);
	std::string test;
	unsigned char buf;

	for (size_t i = 0; i < size; ++i) {
		//instream.ignore(1);
		BinaryRead(instream, buf);
		test += buf;
	}

	//printf("%s\n", test.c_str());

	//instream.read((char*)&toReturn[0], size * 2);
	//std::string str(toReturn.begin(), toReturn.end());
	return test;
}
