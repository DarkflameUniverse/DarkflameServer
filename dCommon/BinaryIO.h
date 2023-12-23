#pragma once

#ifndef __BINARYIO__H__
#define __BINARYIO__H__

#include <iostream>
#include <fstream>
#include <string>

#include "Game.h"
#include "Logger.h"

namespace BinaryIO {

	template<typename T>
	std::ostream& BinaryWrite(std::ostream& stream, const T& value) {
		return stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	template<typename T>
	std::istream& BinaryRead(std::istream& stream, T& value) {
		if (!stream.good()) throw std::runtime_error("Failed to read from istream.");

		return stream.read(reinterpret_cast<char*>(&value), sizeof(T));
	}

	enum class ReadType : int8_t {
		WideString = 0,
		String = 1,
	};

	template<typename SizeType>
	inline void ReadString(std::istream& stream, std::u16string& value) {
		static_assert(std::is_integral<SizeType>::value, "SizeType must be an integral type.");

		SizeType size;
		BinaryRead(stream, size);

		if (!stream.good()) throw std::runtime_error("Failed to read from istream.");
		value.resize(size);
		stream.read(reinterpret_cast<char*>(value.data()), size * sizeof(uint16_t));
	}

	template<typename SizeType>
	inline void ReadString(std::istream& stream, std::string& value, ReadType readType) {
		static_assert(std::is_integral<SizeType>::value, "SizeType must be an integral type.");

		SizeType size;
		BinaryRead(stream, size);

		if (!stream.good()) throw std::runtime_error("Failed to read from istream.");
		value.resize(size);
		if (readType == ReadType::WideString) {
			uint16_t wideChar;

			// Faster to do this than to read a u16string and convert it to a string since we only go through allocator once
			for (SizeType i = 0; i < size; ++i) {
				BinaryRead(stream, wideChar);
				value[i] = static_cast<char>(wideChar);
			}
		} else {
			stream.read(value.data(), size);
		}
	}

	std::string ReadString(std::istream& instream);

	inline bool DoesFileExist(const std::string& name) {
		std::ifstream f(name.c_str());
		return f.good();
	}
}

#endif  //!__BINARYIO__H__
