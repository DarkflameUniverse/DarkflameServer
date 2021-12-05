/// \file
/// \brief Types used by RakNet, most of which involve user code.
///
/// This file is part of RakNet Copyright 2003 Kevin Jenkins.
///
/// Usage of RakNet is subject to the appropriate license agreement.
/// Creative Commons Licensees are subject to the
/// license found at
/// http://creativecommons.org/licenses/by-nc/2.5/
/// Single application licensees are subject to the license found at
/// http://www.jenkinssoftware.com/SingleApplicationLicense.html
/// Custom license users are subject to the terms therein.
/// GPL license users are subject to the GNU General Public
/// License as published by the Free
/// Software Foundation; either version 2 of the License, or (at your
/// option) any later version.

#ifndef __NETWORK_TYPES_H
#define __NETWORK_TYPES_H

#include "RakNetDefines.h"
#include "RakNetTypes.h"
#include "Export.h"
#if !defined(_WIN32) && ((defined(__GNUC__)  || defined(__GCCXML__)))
#include "stdint.h"
#endif

/// Forward declaration
namespace RakNet
{
	class BitStream;
};

/// Given a number of bits, return how many bytes are needed to represent that.
#define BITS_TO_BYTES(x) (((x)+7)>>3)
#define BYTES_TO_BITS(x) ((x)<<3)

/// \sa NetworkIDObject.h
typedef unsigned char UniqueIDType;
typedef unsigned short SystemIndex;
typedef unsigned char RPCIndex;
const int MAX_RPC_MAP_SIZE=((RPCIndex)-1)-1;
const int UNDEFINED_RPC_INDEX=((RPCIndex)-1);

/// First byte of a network message
typedef unsigned char MessageID;

typedef unsigned int BitSize_t;

#ifndef _STDINT_H
typedef unsigned char         uint8_t;
typedef unsigned short        uint16_t;
typedef unsigned int          uint32_t;
typedef unsigned long long int    uint64_t;
typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long   	int64_t;
#endif

// Define __GET_TIME_64BIT if you want to use large types for GetTime (takes more bandwidth when you transmit time though!)
// You would want to do this if your system is going to run long enough to overflow the millisecond counter (over a month)
#ifdef __GET_TIME_64BIT
typedef unsigned long long RakNetTime;
typedef unsigned long long RakNetTimeNS;
#else
typedef unsigned int RakNetTime;
typedef unsigned long long RakNetTimeNS;
#endif

#if defined(_MSC_VER) && _MSC_VER > 0
#define PRINTF_TIME_MODIFIER "I64"
#else
#define PRINTF_TIME_MODIFIER "ll"
#endif

/// Describes the local socket to use for RakPeer::Startup
struct RAK_DLL_EXPORT SocketDescriptor
{
	SocketDescriptor();
	SocketDescriptor(unsigned short _port, const char *_hostAddress);

	/// The local port to bind to.  Pass 0 to have the OS autoassign a port.
	unsigned short port;

	/// The local network card address to bind to, such as "127.0.0.1".  Pass an empty string to use INADDR_ANY.
	char hostAddress[32];
};

/// \brief Unique identifier for a system.
/// Corresponds to a network address
struct RAK_DLL_EXPORT SystemAddress
{
	///The peer address from inet_addr.
	unsigned int binaryAddress;
	///The port number
	unsigned short port;

	// Return the systemAddress as a string in the format <IP>:<Port>
	// Note - returns a static string.  Not thread-safe or safe for multiple calls per line.
	const char *ToString(bool writePort=true) const;

	// Sets the binary address part from a string.  Doesn't set the port
	void SetBinaryAddress(const char *str);

	SystemAddress& operator = ( const SystemAddress& input )
	{
		binaryAddress = input.binaryAddress;
		port = input.port;
		return *this;
	}

	bool operator==( const SystemAddress& right ) const;
	bool operator!=( const SystemAddress& right ) const;
	bool operator > ( const SystemAddress& right ) const;
	bool operator < ( const SystemAddress& right ) const;
};

struct RAK_DLL_EXPORT NetworkID
{
	// Set this to true to use peer to peer mode for NetworkIDs.
	// Obviously the value of this must match on all systems.
	// True, and this will write the systemAddress portion with network sends.  Takes more bandwidth, but NetworkIDs can be locally generated
	// False, and only localSystemAddress is used.
	static bool peerToPeerMode;

	// In peer to peer, we use both systemAddress and localSystemAddress
	// In client / server, we only use localSystemAddress
	SystemAddress systemAddress;
	unsigned short localSystemAddress;

	NetworkID& operator = ( const NetworkID& input );

	static bool IsPeerToPeerMode(void);
	static void SetPeerToPeerMode(bool isPeerToPeer);
	bool operator==( const NetworkID& right ) const;
	bool operator!=( const NetworkID& right ) const;
	bool operator > ( const NetworkID& right ) const;
	bool operator < ( const NetworkID& right ) const;
};

/// Size of SystemAddress data
#define SystemAddress_Size 6

/// This represents a user message from another system.
struct Packet
{
	/// Server only - this is the index into the player array that this systemAddress maps to
	SystemIndex systemIndex;

	/// The system that send this packet.
	SystemAddress systemAddress;

	/// The length of the data in bytes
	/// \deprecated You should use bitSize.
	unsigned int length;

	/// The length of the data in bits
	BitSize_t bitSize;

	/// The data from the sender
	unsigned char* data;

	/// @internal
	/// Indicates whether to delete the data, or to simply delete the packet.
	bool deleteData;
};

class RakPeerInterface;

/// All RPC functions have the same parameter list - this structure.
/// \depreciated Use the AutoRPC plugin instead
struct RPCParameters
{
	/// The data from the remote system
	unsigned char *input;

	/// How many bits long \a input is
	BitSize_t numberOfBitsOfData;

	/// Which system called this RPC
	SystemAddress sender;

	/// Which instance of RakPeer (or a derived RakPeer or RakPeer) got this call
	RakPeerInterface *recipient;

	RakNetTime remoteTimestamp;

	/// The name of the function that was called.
	char *functionName;

	/// You can return values from RPC calls by writing them to this BitStream.
	/// This is only sent back if the RPC call originally passed a BitStream to receive the reply.
	/// If you do so and your send is reliable, it will block until you get a reply or you get disconnected from the system you are sending to, whichever is first.
	/// If your send is not reliable, it will block for triple the ping time, or until you are disconnected, or you get a reply, whichever is first.
	RakNet::BitStream *replyToSender;
};

///  Index of an unassigned player
const SystemIndex UNASSIGNED_PLAYER_INDEX = 65535;

/// Index of an invalid SystemAddress
const SystemAddress UNASSIGNED_SYSTEM_ADDRESS =
{
	0xFFFFFFFF, 0xFFFF
};

/// Unassigned object ID
const NetworkID UNASSIGNED_NETWORK_ID =
{
	{0xFFFFFFFF, 0xFFFF}, 65535
};

const int PING_TIMES_ARRAY_SIZE = 5;

/// \brief RPC Function Implementation
///
/// The Remote Procedure Call Subsystem provide the RPC paradigm to
/// RakNet user. It consists in providing remote function call over the
/// network.  A call to a remote function require you to prepare the
/// data for each parameter (using BitStream) for example.
///
/// Use the following C function prototype for your callbacks
/// @code
/// void functionName(RPCParameters *rpcParms);
/// @endcode
/// If you pass input data, you can parse the input data in two ways.
/// 1.
/// Cast input to a struct (such as if you sent a struct)
/// i.e. MyStruct *s = (MyStruct*) input;
/// Make sure that the sizeof(MyStruct) is equal to the number of bytes passed!
/// 2.
/// Create a BitStream instance with input as data and the number of bytes
/// i.e. BitStream myBitStream(input, (numberOfBitsOfData-1)/8+1)
/// (numberOfBitsOfData-1)/8+1 is how convert from bits to bytes
/// Full example:
/// @code
/// void MyFunc(RPCParameters *rpcParms) {}
/// RakPeer *rakClient;
/// REGISTER_AS_REMOTE_PROCEDURE_CALL(rakClient, MyFunc);
/// This would allow MyFunc to be called from the server using  (for example)
/// rakServer->RPC("MyFunc", 0, clientID, false);
/// @endcode


/// \def REGISTER_STATIC_RPC
/// \ingroup RAKNET_RPC
/// \depreciated Use the AutoRPC plugin instead
/// Register a C function as a Remote procedure.
/// \param[in] networkObject Your instance of RakPeer, RakPeer, or RakPeer
/// \param[in] functionName The name of the C function to call
/// \attention 12/01/05 REGISTER_AS_REMOTE_PROCEDURE_CALL renamed to REGISTER_STATIC_RPC.  Delete the old name sometime in the future
//#pragma deprecated(REGISTER_AS_REMOTE_PROCEDURE_CALL)
//#define REGISTER_AS_REMOTE_PROCEDURE_CALL(networkObject, functionName) REGISTER_STATIC_RPC(networkObject, functionName)
#define REGISTER_STATIC_RPC(networkObject, functionName) (networkObject)->RegisterAsRemoteProcedureCall((#functionName),(functionName))

/// \def CLASS_MEMBER_ID
/// \ingroup RAKNET_RPC
/// \depreciated Use the AutoRPC plugin instead
/// \brief Concatenate two strings

/// \def REGISTER_CLASS_MEMBER_RPC
/// \depreciated Use the AutoRPC plugin instead
/// \ingroup RAKNET_RPC
/// \brief Register a member function of an instantiated object as a Remote procedure call.
/// RPC member Functions MUST be marked __cdecl!
/// \sa ObjectMemberRPC.cpp
/// \b CLASS_MEMBER_ID is a utility macro to generate a unique signature for a class and function pair and can be used for the Raknet functions RegisterClassMemberRPC(...) and RPC(...)
/// \b REGISTER_CLASS_MEMBER_RPC is a utility macro to more easily call RegisterClassMemberRPC
/// \param[in] networkObject Your instance of RakPeer, RakPeer, or RakPeer
/// \param[in] className The class containing the function
/// \param[in] functionName The name of the function (not in quotes, just the name)
#define CLASS_MEMBER_ID(className, functionName) #className "_" #functionName
#define REGISTER_CLASS_MEMBER_RPC(networkObject, className, functionName) {union {void (__cdecl className::*cFunc)( RPCParameters *rpcParms ); void* voidFunc;}; cFunc=&className::functionName; networkObject->RegisterClassMemberRPC(CLASS_MEMBER_ID(className, functionName),voidFunc);}

/// \def UNREGISTER_AS_REMOTE_PROCEDURE_CALL
/// \brief Only calls UNREGISTER_STATIC_RPC
/// \depreciated Use the AutoRPC plugin instead

/// \def UNREGISTER_STATIC_RPC
/// \ingroup RAKNET_RPC
/// \depreciated Use the AutoRPC plugin instead
/// Unregisters a remote procedure call
/// RPC member Functions MUST be marked __cdecl!  See the ObjectMemberRPC example.
/// \param[in] networkObject The object that manages the function
/// \param[in] functionName The function name
// 12/01/05 UNREGISTER_AS_REMOTE_PROCEDURE_CALL Renamed to UNREGISTER_STATIC_RPC.  Delete the old name sometime in the future
//#pragma deprecated(UNREGISTER_AS_REMOTE_PROCEDURE_CALL)
//#define UNREGISTER_AS_REMOTE_PROCEDURE_CALL(networkObject,functionName) UNREGISTER_STATIC_RPC(networkObject,functionName)
#define UNREGISTER_STATIC_RPC(networkObject,functionName) (networkObject)->UnregisterAsRemoteProcedureCall((#functionName))

/// \def UNREGISTER_CLASS_INST_RPC
/// \ingroup RAKNET_RPC
/// \depreciated Use the AutoRPC plugin instead
/// \brief Unregisters a member function of an instantiated object as a Remote procedure call.
/// \param[in] networkObject The object that manages the function
/// \param[in] className The className that was originally passed to REGISTER_AS_REMOTE_PROCEDURE_CALL
/// \param[in] functionName The function name
#define UNREGISTER_CLASS_MEMBER_RPC(networkObject, className, functionName) (networkObject)->UnregisterAsRemoteProcedureCall((#className "_" #functionName))

#endif
