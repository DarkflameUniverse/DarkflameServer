#pragma once

#ifndef __BINARYIO__H__
#define __BINARYIO__H__

#include <iostream>
#include <fstream>
#include <cstdint>
#include <stdexcept>
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

	// Bytes left to read in a seekable stream (0 when it can't tell)
	inline uint64_t Remaining(std::istream& stream) {
		const auto position = stream.tellg();
		if (position < 0) return 0;
		stream.seekg(0, std::ios::end);
		const auto end = stream.tellg();
		stream.seekg(position);
		return end > position ? static_cast<uint64_t>(end - position) : 0;
	}

	/**
	 * Throws when the stream has fewer than `bytes` left. For counts and lengths read from a file, checked before
	 * memory is set aside for them, so a file can't ask for more than it holds.
	 */
	inline void RequireRemaining(std::istream& stream, uint64_t bytes) {
		if (bytes > Remaining(stream)) throw std::runtime_error("Failed to read from istream.");
	}

	// Lengths up to this are read without looking at what is left (every u8 length)
	constexpr uint64_t UNCHECKED_LENGTH = 255;

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
		if (static_cast<uint64_t>(size) > UNCHECKED_LENGTH) RequireRemaining(stream, static_cast<uint64_t>(size) * sizeof(uint16_t));
		value.resize(size);
		stream.read(reinterpret_cast<char*>(value.data()), size * sizeof(uint16_t));
	}

	template<typename SizeType>
	inline void ReadString(std::istream& stream, std::string& value, ReadType readType) {
		static_assert(std::is_integral<SizeType>::value, "SizeType must be an integral type.");

		SizeType size;
		BinaryRead(stream, size);

		if (!stream.good()) throw std::runtime_error("Failed to read from istream.");
		if (static_cast<uint64_t>(size) > UNCHECKED_LENGTH) {
			RequireRemaining(stream, static_cast<uint64_t>(size) * (readType == ReadType::WideString ? sizeof(uint16_t) : 1));
		}
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

	std::u8string ReadU8String(std::istream& instream);

	inline bool DoesFileExist(const std::string& name) {
		std::ifstream f(name.c_str());
		return f.good();
	}
}

#endif  //!__BINARYIO__H__
