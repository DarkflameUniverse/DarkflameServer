#pragma once

// Custom Classes
#include "AMFFormat.h"

// RakNet
#include <BitStream.h>

/*!
 \file AMFFormat_BitStream.h
 \brief A class that implements native writing of AMF values to RakNet::BitStream
 */

 // We are using the RakNet namespace
namespace RakNet {
	//! Writes an AMFValue pointer to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFValue*>(AMFValue* value);

	//! Writes an AMFUndefinedValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFUndefinedValue>(AMFUndefinedValue value);

	//! Writes an AMFNullValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFNullValue>(AMFNullValue value);

	//! Writes an AMFFalseValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFFalseValue>(AMFFalseValue value);

	//! Writes an AMFTrueValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFTrueValue>(AMFTrueValue value);

	//! Writes an AMFIntegerValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFIntegerValue>(AMFIntegerValue value);

	//! Writes an AMFDoubleValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFDoubleValue>(AMFDoubleValue value);

	//! Writes an AMFStringValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFStringValue>(AMFStringValue value);

	//! Writes an AMFXMLDocValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFXMLDocValue>(AMFXMLDocValue value);

	//! Writes an AMFDateValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFDateValue>(AMFDateValue value);

	//! Writes an AMFArrayValue to a RakNet::BitStream
	/*!
	 \param value The value to write
	 */
	template <>
	void RakNet::BitStream::Write<AMFArrayValue*>(AMFArrayValue* value);
} // namespace RakNet
