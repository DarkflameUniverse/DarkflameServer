#pragma once

#include "Amf3.h"
#include "BitStream.h"

#include <memory>
#include <vector>
#include <string>

class AMFDeserialize {
public:
	/**
	 * Read an AMF3 value from a bitstream.
	 *
	 * @param inStream inStream to read value from.
	 * @return Returns an AMFValue with all the information from the bitStream in it.
	 */
	std::unique_ptr<AMFBaseValue> Read(RakNet::BitStream* inStream);
private:
	/**
	 * @brief Private method to read a U29 integer from a bitstream
	 *
	 * @param inStream bitstream to read data from
	 * @return The number as an unsigned 29 bit integer
	 */
	static uint32_t ReadU29(RakNet::BitStream* inStream);

	/**
	 * @brief Reads a string from a bitstream
	 *
	 * @param inStream bitStream to read data from
	 * @return The read string
	 */
	const std::string ReadString(RakNet::BitStream* inStream);

	/**
	 * @brief Read an AMFDouble value from a bitStream
	 *
	 * @param inStream bitStream to read data from
	 * @return Double value represented as an AMFValue
	 */
	static std::unique_ptr<AMFDoubleValue> ReadAmfDouble(RakNet::BitStream* inStream);

	/**
	 * @brief Read an AMFArray from a bitStream
	 *
	 * @param inStream bitStream to read data from
	 * @return Array value represented as an AMFValue
	 */
	std::unique_ptr<AMFArrayValue> ReadAmfArray(RakNet::BitStream* inStream);

	/**
	 * @brief Read an AMFString from a bitStream
	 *
	 * @param inStream bitStream to read data from
	 * @return String value represented as an AMFValue
	 */
	std::unique_ptr<AMFStringValue> ReadAmfString(RakNet::BitStream* inStream);

	/**
	 * @brief Read an AMFInteger from a bitStream
	 *
	 * @param inStream bitStream to read data from
	 * @return Integer value represented as an AMFValue
	 */
	static std::unique_ptr<AMFIntValue> ReadAmfInteger(RakNet::BitStream* inStream);

	/**
	 * List of strings read so far saved to be read by reference.
	 */
	std::vector<std::string> accessedElements;
};
