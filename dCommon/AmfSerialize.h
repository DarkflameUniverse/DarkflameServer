#pragma once

// Custom Classes
#include "Amf3.h"

// RakNet
#include <BitStream.h>

/*!
 \file AmfSerialize.h
 \brief A class that implements native writing of AMF values to RakNet::BitStream
 */

 // We are using the RakNet namespace
namespace RakNet {
	//! Writes an AMFValue pointer to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFBaseValue&>(AMFBaseValue& value);

	//! Writes an AMFIntegerValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFIntValue&>(AMFIntValue& value);

	//! Writes an AMFDoubleValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFDoubleValue&>(AMFDoubleValue& value);

	//! Writes an AMFStringValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFStringValue&>(AMFStringValue& value);

	//! Writes an AMFArrayValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFArrayValue&>(AMFArrayValue& value);
} // namespace RakNet
