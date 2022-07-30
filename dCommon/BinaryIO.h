#pragma once
#include <iostream>
#include <fstream>

namespace BinaryIO {
	template<typename T>
	std::ostream& BinaryWrite(std::ostream& stream, const T& value) {
		return stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	template<typename T>
	std::istream& BinaryRead(std::istream& stream, T& value) {
		if (!stream.good())
			printf("bla");

		return stream.read(reinterpret_cast<char*>(&value), sizeof(T));
	}

	void WriteString(const std::string& stringToWrite, std::ofstream& outstream);
	std::string ReadString(std::ifstream& instream);
	std::string ReadString(std::ifstream& instream, size_t size);
	std::string ReadWString(std::ifstream& instream);

	inline bool DoesFileExist(const std::string& name) {
		std::ifstream f(name.c_str());
		return f.good();
	}
}
