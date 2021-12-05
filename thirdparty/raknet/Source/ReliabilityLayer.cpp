/// \file
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

#include "ReliabilityLayer.h"
#include "GetTime.h"
#include "SocketLayer.h"
#include "PluginInterface.h"
#include "RakAssert.h"
#include "Rand.h"
#include "MessageIdentifiers.h"
#include <math.h>


static const int DEFAULT_HAS_RECEIVED_PACKET_QUEUE_SIZE=512;
static const RakNetTimeNS MAX_TIME_BETWEEN_PACKETS= 350000; // 350 milliseconds
static const RakNetTimeNS STARTING_TIME_BETWEEN_PACKETS=MAX_TIME_BETWEEN_PACKETS;
static const RakNetTimeNS HISTOGRAM_RESTART_CYCLE=10000000; // Every 10 seconds reset the histogram
static const long double TIME_BETWEEN_PACKETS_INCREASE_MULTIPLIER_DEFAULT=.02;
static const long double TIME_BETWEEN_PACKETS_DECREASE_MULTIPLIER_DEFAULT=.04;

#ifdef _MSC_VER
#pragma warning( push )
#endif

#ifdef _WIN32
//#define _DEBUG_LOGGER
#ifdef _DEBUG_LOGGER
#include <windows.h>
#endif
#endif

//#define DEBUG_SPLIT_PACKET_PROBLEMS
#if defined (DEBUG_SPLIT_PACKET_PROBLEMS)
static int waitFlag=-1;
#endif

using namespace RakNet;

int SplitPacketChannelComp( SplitPacketIdType const &key, SplitPacketChannel* const &data )
{
	if (key < data->splitPacketList[0]->splitPacketId)
		return -1;
	if (key == data->splitPacketList[0]->splitPacketId)
		return 0;
	return 1;
}

int SplitPacketIndexComp( SplitPacketIndexType const &key, InternalPacket* const &data )
{
	if (key < data->splitPacketIndex)
		return -1;
	if (key == data->splitPacketIndex)
		return 0;
	return 1;
}


//-------------------------------------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------------------------------------
// Add 21 to the default MTU so if we encrypt it can hold potentially 21 more bytes of extra data + padding.
ReliabilityLayer::ReliabilityLayer() : updateBitStream( DEFAULT_MTU_SIZE + 21 )   // preallocate the update bitstream so we can avoid a lot of reallocs at runtime
{
	freeThreadedMemoryOnNextUpdate = false;
#ifdef _DEBUG
	// Wait longer to disconnect in debug so I don't get disconnected while tracing
	timeoutTime=30000;
#else
	timeoutTime=10000;
#endif

#ifndef _RELEASE
	maxSendBPS=minExtraPing=extraPingVariance=0;
#endif

	InitializeVariables();
}

//-------------------------------------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------------------------------------
ReliabilityLayer::~ReliabilityLayer()
{
	FreeMemory( true ); // Free all memory immediately
}

//-------------------------------------------------------------------------------------------------------
// Resets the layer for reuse
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::Reset( bool resetVariables )
{
	FreeMemory( true ); // true because making a memory reset pending in the update cycle causes resets after reconnects.  Instead, just call Reset from a single thread
	if (resetVariables)
		InitializeVariables();
}

//-------------------------------------------------------------------------------------------------------
// Sets up encryption
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::SetEncryptionKey( const unsigned char* key )
{
	if ( key )
		encryptor.SetKey( key );
	else
		encryptor.UnsetKey();
}

//-------------------------------------------------------------------------------------------------------
// Set the time, in MS, to use before considering ourselves disconnected after not being able to deliver a reliable packet
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::SetTimeoutTime( RakNetTime time )
{
	timeoutTime=time;
}

//-------------------------------------------------------------------------------------------------------
// Returns the value passed to SetTimeoutTime. or the default if it was never called
//-------------------------------------------------------------------------------------------------------
RakNetTime ReliabilityLayer::GetTimeoutTime(void)
{
	return timeoutTime;
}

//-------------------------------------------------------------------------------------------------------
// Initialize the variables
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::InitializeVariables( void )
{
	memset( waitingForOrderedPacketReadIndex, 0, NUMBER_OF_ORDERED_STREAMS * sizeof(OrderingIndexType));
	memset( waitingForSequencedPacketReadIndex, 0, NUMBER_OF_ORDERED_STREAMS * sizeof(OrderingIndexType) );
	memset( waitingForOrderedPacketWriteIndex, 0, NUMBER_OF_ORDERED_STREAMS * sizeof(OrderingIndexType) );
	memset( waitingForSequencedPacketWriteIndex, 0, NUMBER_OF_ORDERED_STREAMS * sizeof(OrderingIndexType) );
	memset( &statistics, 0, sizeof( statistics ) );
	statistics.connectionStartTime = RakNet::GetTime();
	splitPacketId = 0;
	throughputCapCountdown=0;
	sendMessageNumberIndex = 0;
	internalOrderIndex=0;
	lastUpdateTime= RakNet::GetTimeNS();
	lastTimeBetweenPacketsIncrease=lastTimeBetweenPacketsDecrease=0;
    remoteSystemTime=0;
	// lastPacketSendTime=retransmittedFrames=sentPackets=sentFrames=receivedPacketsCount=bytesSent=bytesReceived=0;

	nextAllowedThroughputSample=0;
	deadConnection = cheater = false;
	lastAckTime = 0;
	// ackPing=0;

	receivedPacketsBaseIndex=0;
	resetReceivedPackets=true;
	sendPacketCount=receivePacketCount=0; 
        
//	SetPing( 1000 );

	timeBetweenPackets=STARTING_TIME_BETWEEN_PACKETS;

	ackPingIndex=0;
	ackPingSum=(RakNetTimeNS)0;

	nextSendTime=lastUpdateTime;
	//nextLowestPingReset=(RakNetTimeNS)0;
	continuousSend=false;

	histogramStart=(RakNetTimeNS)0;
	histogramBitsSent=0;
	packetlossThisSample=backoffThisSample=false;
	packetlossThisSampleResendCount=0;


//	timeBetweenPacketsIncreaseMultiplier=TIME_BETWEEN_PACKETS_INCREASE_MULTIPLIER_DEFAULT;
//	timeBetweenPacketsDecreaseMultiplier=TIME_BETWEEN_PACKETS_DECREASE_MULTIPLIER_DEFAULT;
}

//-------------------------------------------------------------------------------------------------------
// Frees all allocated memory
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::FreeMemory( bool freeAllImmediately )
{
	if ( freeAllImmediately )
	{
		FreeThreadedMemory();
		FreeThreadSafeMemory();
	}
	else
	{
		FreeThreadSafeMemory();
		freeThreadedMemoryOnNextUpdate = true;
	}
}

void ReliabilityLayer::FreeThreadedMemory( void )
{
}

void ReliabilityLayer::FreeThreadSafeMemory( void )
{
	unsigned i,j;
	InternalPacket *internalPacket;

	for (i=0; i < splitPacketChannelList.Size(); i++)
	{
		for (j=0; j < splitPacketChannelList[i]->splitPacketList.Size(); j++)
		{
			rakFree(splitPacketChannelList[i]->splitPacketList[j]->data);
			//		internalPacketPool.Release( splitPacketChannelList[i]->splitPacketList[j] );
		}
		delete splitPacketChannelList[i];
	}
	splitPacketChannelList.Clear();

	while ( outputQueue.Size() > 0 )
	{
		internalPacket = outputQueue.Pop();
		rakFree(internalPacket->data);
		//		internalPacketPool.Release( internalPacket );
	}

	outputQueue.ClearAndForceAllocation( 32 );

	for ( i = 0; i < orderingList.Size(); i++ )
	{
		if ( orderingList[ i ] )
		{
			DataStructures::LinkedList<InternalPacket*>* theList = orderingList[ i ];

			if ( theList )
			{
				while ( theList->Size() )
				{
					internalPacket = orderingList[ i ]->Pop();
					rakFree(internalPacket->data);
					//		internalPacketPool.Release( internalPacket );
				}

				delete theList;
			}
		}
	}

	orderingList.Clear();

	//resendList.ForEachData(DeleteInternalPacket);
	resendList.Clear();
	while ( resendQueue.Size() )
	{
		// The resend Queue can have NULL pointer holes.  This is so we can deallocate blocks without having to compress the array
		internalPacket = resendQueue.Pop();

		if ( internalPacket )
		{
			rakFree(internalPacket->data);
			//	internalPacketPool.Release( internalPacket );
		}
	}

	resendQueue.ClearAndForceAllocation( DEFAULT_HAS_RECEIVED_PACKET_QUEUE_SIZE );

	for ( i = 0; i < NUMBER_OF_PRIORITIES; i++ )
	{
		j = 0;
		for ( ; j < sendPacketSet[ i ].Size(); j++ )
		{
			rakFree(( sendPacketSet[ i ] ) [ j ]->data);
			//internalPacketPool.Release( ( sendPacketSet[ i ] ) [ j ] );
		}

		sendPacketSet[ i ].ClearAndForceAllocation( 32 ); // Preallocate the send lists so we don't do a bunch of reallocations unnecessarily
	}

#ifndef _RELEASE
	for (unsigned i = 0; i < delayList.Size(); i++ )
		delete delayList[ i ];
	delayList.Clear();
#endif


	internalPacketPool.Clear();
	acknowlegements.Clear();
}

//-------------------------------------------------------------------------------------------------------
// Packets are read directly from the socket layer and skip the reliability
//layer  because unconnected players do not use the reliability layer
// This function takes packet data after a player has been confirmed as
//connected.  The game should not use that data directly
// because some data is used internally, such as packet acknowledgment and
//split packets
//-------------------------------------------------------------------------------------------------------
bool ReliabilityLayer::HandleSocketReceiveFromConnectedPlayer( const char *buffer, unsigned int length, SystemAddress systemAddress, DataStructures::List<PluginInterface*> &messageHandlerList, int MTUSize )
{
#ifdef _DEBUG
	assert( !( buffer == 0 ) );
#endif

	(void) MTUSize;

	if ( length <= 2 || buffer == 0 )   // Length of 1 is a connection request resend that we just ignore
		return true;

	//int numberOfAcksInFrame = 0;
	RakNetTimeNS time;
	bool indexFound;
	int count, size;
	MessageNumberType holeCount;
	unsigned i;
	bool hasAcks=false;

	//	bool duplicatePacket;

	// bytesReceived+=length + UDP_HEADER_SIZE;

	UpdateThreadedMemory();

	// decode this whole chunk if the decoder is defined.
	if ( encryptor.IsKeySet() )
	{
		if ( encryptor.Decrypt( ( unsigned char* ) buffer, length, ( unsigned char* ) buffer, &length ) == false )
		{
			statistics.bitsWithBadCRCReceived += length * 8;
			statistics.packetsWithBadCRCReceived++;
			return false;
		}
	}

	statistics.bitsReceived += length * 8;
	statistics.packetsReceived++;

	RakNet::BitStream socketData( (unsigned char*) buffer, length, false ); // Convert the incoming data to a bitstream for easy parsing
	time = RakNet::GetTimeNS();

	DataStructures::RangeList<MessageNumberType> incomingAcks;
	socketData.Read(hasAcks);
	if (hasAcks)
	{
		MessageNumberType messageNumber;
		RemoteSystemTimeType ourSystemTime;
		socketData.Read(ourSystemTime);
		if (incomingAcks.Deserialize(&socketData)==false)
			return false;
		for (i=0; i<incomingAcks.ranges.Size();i++)
		{
			if (incomingAcks.ranges[i].minIndex>incomingAcks.ranges[i].maxIndex)
			{
				RakAssert(incomingAcks.ranges[i].minIndex<=incomingAcks.ranges[i].maxIndex);
				return false;
			}

			for (messageNumber=incomingAcks.ranges[i].minIndex; messageNumber >= incomingAcks.ranges[i].minIndex && messageNumber <= incomingAcks.ranges[i].maxIndex; messageNumber++)
			{
				hasAcks=true;

				// GOT ACK

				// SHOW - ack received
		//		printf("Got Ack for %i\n",messageNumber);
				RemovePacketFromResendListAndDeleteOlderReliableSequenced( messageNumber, time );

#ifdef _DEBUG_LOGGER
				{
					char temp[256];
					sprintf(temp, "%p: Got ack for %i. Resend queue size=%i\n", this, messageNumber, resendQueue.Size());
					OutputDebugStr(temp);
				}
#endif

				if ( resendList.IsEmpty() )
				{
					lastAckTime = 0; // Not resending anything so clear this var so we don't drop the connection on not getting any more acks
				}
				else
				{
					lastAckTime = time; // Just got an ack.  Record when we got it so we know the connection is alive
				}
			}
		}

		RakNetTimeNS rst = (RakNetTimeNS) ourSystemTime * (RakNetTimeNS) 1000;
		if (time >= rst)
			ackPing = time - rst;
		else
			ackPing=0;
		RakNetTimeNS avePing;
		if (ackPingSum==0)
		{
			// First time between packets is just the ping.
			timeBetweenPackets = ackPing;
			if (timeBetweenPackets > MAX_TIME_BETWEEN_PACKETS)
				timeBetweenPackets=MAX_TIME_BETWEEN_PACKETS;
			else if (timeBetweenPackets < (RakNetTimeNS)10000 )
				timeBetweenPackets=(RakNetTimeNS)10000;
			ackPingSum=ackPing<<8; // Multiply by 256
			for (int i=0; i < 256; i++)
				ackPingSamples[i]=ackPing;
		}			

		avePing=ackPingSum>>8; // divide by 256
                              
		if (continuousSend && time >= nextAllowedThroughputSample)
		{
			// 10 milliseconds tolerance, because at small pings percentage fluctuations vary widely naturally
			bool pingLowering = ackPing <= avePing * (RakNetTimeNS)6 / (RakNetTimeNS)5
				+ (RakNetTimeNS)10000;

			// Only increase if the ping is lowering and there was no packetloss
			if (pingLowering==true && packetlossThisSample==false) 
			{
				// Only count towards the average when ping declining and no packetloss the last sample.
				// Otherwise it eventually averages to when we get packetloss.
				ackPingSum-=ackPingSamples[ackPingIndex];
				ackPingSamples[ackPingIndex]=ackPing;
				ackPingSum+=ackPing;
				ackPingIndex++; // Might wrap to 0

				RakNetTimeNS diff;
				// Decrease time between packets by 2%
				diff = (RakNetTimeNS)ceil(((long double) timeBetweenPackets * TIME_BETWEEN_PACKETS_INCREASE_MULTIPLIER_DEFAULT));
				
				if (diff < timeBetweenPackets)
				{
					nextSendTime-=diff;
					timeBetweenPackets-=diff;
				}
				
	//			printf("- CurPing: %i. AvePing: %i. Ploss=%.1f.  Time between= %i\n", (RakNetTime)(ackPing/(RakNetTimeNS)1000), (RakNetTime)(avePing/(RakNetTimeNS)1000), 100.0f * ( float ) statistics.messagesTotalBitsResent / ( float ) statistics.totalBitsSent, timeBetweenPackets);
				lastTimeBetweenPacketsDecrease=time;
			}
			// Decrease if the ping is rising or we got packetloss for a new group of resends
			else if (pingLowering==false || backoffThisSample==true) // if (ackPing >= ((avePing*(RakNetTimeNS)4)/(RakNetTimeNS)3) + (RakNetTimeNS) 1000) // 1 milliseconds tolerance, because at small pings percentage fluctuations vary widely naturally
			{
				// Increase time between packets by 4%
				RakNetTimeNS diff = (RakNetTimeNS)ceil(((long double) timeBetweenPackets * TIME_BETWEEN_PACKETS_DECREASE_MULTIPLIER_DEFAULT));
				nextSendTime+=diff;
				timeBetweenPackets+=diff;

//				printf("+ CurPing: %i. AvePing: %i. Ploss=%.1f. Time between = %i\n",  (RakNetTime)(ackPing/(RakNetTimeNS)1000), (RakNetTime)(avePing/(RakNetTimeNS)1000), 100.0f * ( float ) statistics.messagesTotalBitsResent / ( float ) statistics.totalBitsSent,timeBetweenPackets);
				lastTimeBetweenPacketsIncrease=time;

				if (timeBetweenPackets > MAX_TIME_BETWEEN_PACKETS)
				{
					// Something wrong, perhaps the entire network clogged up, or a different path was taken.
					timeBetweenPackets=MAX_TIME_BETWEEN_PACKETS;
				}
			}
			
			packetlossThisSample=false;
			backoffThisSample=false;

			if (ackPing < 2000000) 
				// 1 milliseconds tolerance, Otherwise we overreact at very small pings
				nextAllowedThroughputSample=time+ackPing + (RakNetTimeNS)1000;
			else
				// Don't resample over super long periods
				nextAllowedThroughputSample=time+200000;

                //        printf("time=%i ",time);
                  //      printf("nextAllowedThroughputSample=%i\n",nextAllowedThroughputSample);
		}
	//	else
	//		printf("Continuous Send = false\n");

//		if (ackPing < lowestPing)
//			lowestPing=ackPing;

		//		if (time > nextLowestPingReset)
		//		{
		//			lowestPing=ackPing;
		//			nextLowestPingReset=time+(RakNetTimeNS)60000000; // Once per minute, reset the lowest ping to account for high level ping changes.
		//		}

	}

	bool hasRemoteSystemTime=false;
	socketData.Read(hasRemoteSystemTime);
	if (hasRemoteSystemTime)
		socketData.Read(remoteSystemTime);


	// Parse the bitstream to create an internal packet
	InternalPacket* internalPacket = CreateInternalPacketFromBitStream( &socketData, time );

	if (internalPacket==0)
		return hasAcks;

	while ( internalPacket )
	{
		for (i=0; i < messageHandlerList.Size(); i++)
			messageHandlerList[i]->OnInternalPacket(internalPacket, receivePacketCount, systemAddress, (RakNetTime)(time/(RakNetTimeNS)1000), false);

		{
#ifdef _DEBUG_LOGGER
			{
				char temp[256];
				sprintf(temp, "%p: Got packet %i data: %i bitlen: %i\n", this, internalPacket->messageNumber, (unsigned char) internalPacket->data[0], internalPacket->dataBitLength);
				OutputDebugStr(temp);
			}
#endif

			// resetReceivedPackets is set from a non-threadsafe function.
			// We do the actual reset in this function so the data is not modified by multiple threads
			if (resetReceivedPackets)
			{
				hasReceivedPacketQueue.ClearAndForceAllocation(DEFAULT_HAS_RECEIVED_PACKET_QUEUE_SIZE);
				receivedPacketsBaseIndex=0;
				resetReceivedPackets=false;
			}

			/*
			// REMOVEME
			if (internalPacket->reliability==RELIABLE_ORDERED)
			{
				RakNet::BitStream bitStream;
				bitStream.Write((char*)internalPacket->data, BITS_TO_BYTES(internalPacket->dataBitLength));
				bitStream.IgnoreBits(8);
				unsigned int receivedPacketNumber;
				bitStream.Read(receivedPacketNumber);
				printf("-- %i, OI=%i\n", receivedPacketNumber, internalPacket->orderingIndex);

			}
			*/
			

			// If the following conditional is true then this either a duplicate packet
			// or an older out of order packet
			// The subtraction unsigned overflow is intentional
			holeCount = (MessageNumberType)(internalPacket->messageNumber-receivedPacketsBaseIndex);
			const MessageNumberType typeRange = (MessageNumberType)-1;

			if ( internalPacket->reliability == RELIABLE_SEQUENCED || internalPacket->reliability == RELIABLE_ORDERED || internalPacket->reliability == RELIABLE )
			{
#ifdef _DEBUG
	//			printf("Sending ack for %i, rpbi=%i\n", internalPacket->messageNumber, receivedPacketsBaseIndex);
#endif
				SendAcknowledgementPacket( internalPacket->messageNumber, time );
			}

			if (holeCount==0)
			{
				// Got what we were expecting
				if (hasReceivedPacketQueue.Size())
					hasReceivedPacketQueue.Pop();
				++receivedPacketsBaseIndex;
			}
			else if (holeCount > typeRange/2)
			{

#ifdef _DEBUG
		//		printf("under_holeCount=%i, pnum=%i, rpbi=%i\n", (int)holeCount, internalPacket->messageNumber, receivedPacketsBaseIndex);
#endif
				// Underflow - got a packet we have already counted past
				statistics.duplicateMessagesReceived++;

#if defined(DEBUG_SPLIT_PACKET_PROBLEMS)
				if (internalPacket->splitPacketCount>0)
					printf("Underflow with hole count %i for split packet with ID %i\n",holeCount, internalPacket->splitPacketId);
#endif

				// Duplicate packet
				rakFree(internalPacket->data);
				internalPacketPool.Release( internalPacket );
				goto CONTINUE_SOCKET_DATA_PARSE_LOOP;
			}
			else if (holeCount<hasReceivedPacketQueue.Size())
			{

#ifdef _DEBUG
		//		printf("over_Fill_holeCount=%i, pnum=%i, rpbi=%i\n", (int)holeCount, internalPacket->messageNumber, receivedPacketsBaseIndex);
#endif

				// Got a higher count out of order packet that was missing in the sequence or we already got
				if (hasReceivedPacketQueue[holeCount]!=0) // non-zero means this is a hole
				{
					// Fill in the hole
					hasReceivedPacketQueue[holeCount]=0; // We got the packet at holeCount
				}
				else
				{
#ifdef _DEBUG
	//				printf("Duplicate, holeCount=%i, pnum=%i, rpbi=%i\n", (int)holeCount, internalPacket->messageNumber, receivedPacketsBaseIndex);
#endif

					// Not a hole - just a duplicate packet
					statistics.duplicateMessagesReceived++;

#if defined(DEBUG_SPLIT_PACKET_PROBLEMS)
					if (internalPacket->splitPacketCount>0)
						printf("Duplicate hole count %i for split packet with ID %i\n",holeCount, internalPacket->splitPacketId);
#endif

					// Duplicate packet
					rakFree(internalPacket->data);
					internalPacketPool.Release( internalPacket );
					goto CONTINUE_SOCKET_DATA_PARSE_LOOP;
				}
			}
			else // holeCount>=receivedPackets.Size()
			{
				if (holeCount > 1000000)
				{
#if defined(DEBUG_SPLIT_PACKET_PROBLEMS)
					if (internalPacket->splitPacketCount>0)
						printf("High hole count %i for split packet with ID %i\n",holeCount, internalPacket->splitPacketId);
#endif

					// Would crash due to out of memory!
					rakFree(internalPacket->data);
					internalPacketPool.Release( internalPacket );
					goto CONTINUE_SOCKET_DATA_PARSE_LOOP;
				}

				// Got a higher count out of order packet whose messageNumber is higher than we have ever got
#ifdef _DEBUG
	//			printf("over_Buffered_holeCount=%i, pnum=%i, rpbi=%i\n", (int) holeCount, internalPacket->messageNumber, receivedPacketsBaseIndex);
#endif

				// Fix - sending on a higher priority gives us a very very high received packets base index if we formerly had pre-split a lot of messages and
				// used that as the message number.  Because of this, a lot of time is spent in this linear loop and the timeout time expires because not
				// all of the message is sent in time.
				// Fixed by late assigning message IDs on the sender

				// Add 0 times to the queue until (messageNumber - baseIndex) < queue size.
			//	RakAssert(holeCount<10000);
				while ((MessageNumberType)(holeCount) > hasReceivedPacketQueue.Size())
					hasReceivedPacketQueue.Push(time+(RakNetTimeNS)60 * (RakNetTimeNS)1000 * (RakNetTimeNS)1000); // Didn't get this packet - set the time to give up waiting
				hasReceivedPacketQueue.Push(0); // Got the packet
#ifdef _DEBUG
				// If this assert hits then MessageNumberType has overflowed
				assert(hasReceivedPacketQueue.Size() < (unsigned int)((MessageNumberType)(-1)));
#endif
			}

			// Pop all expired times.  0 means we got the packet, in which case we don't track this index either.
			while ( hasReceivedPacketQueue.Size()>0 && hasReceivedPacketQueue.Peek() < time )
			{
				hasReceivedPacketQueue.Pop();
				++receivedPacketsBaseIndex;
			}

			statistics.messagesReceived++;

			// If the allocated buffer is > DEFAULT_HAS_RECEIVED_PACKET_QUEUE_SIZE and it is 3x greater than the number of elements actually being used
			if (hasReceivedPacketQueue.AllocationSize() > (unsigned int) DEFAULT_HAS_RECEIVED_PACKET_QUEUE_SIZE && hasReceivedPacketQueue.AllocationSize() > hasReceivedPacketQueue.Size() * 3)
				hasReceivedPacketQueue.Compress();

			if ( internalPacket->reliability == RELIABLE_SEQUENCED || internalPacket->reliability == UNRELIABLE_SEQUENCED )
			{
#ifdef _DEBUG
				assert( internalPacket->orderingChannel < NUMBER_OF_ORDERED_STREAMS );
#endif

				if ( internalPacket->orderingChannel >= NUMBER_OF_ORDERED_STREAMS )
				{
					// Invalid packet
#ifdef _DEBUG
//					printf( "Got invalid packet\n" );
#endif

					rakFree(internalPacket->data);
					internalPacketPool.Release( internalPacket );
					goto CONTINUE_SOCKET_DATA_PARSE_LOOP;
				}

				if ( IsOlderOrderedPacket( internalPacket->orderingIndex, waitingForSequencedPacketReadIndex[ internalPacket->orderingChannel ] ) == false )
				{
					statistics.sequencedMessagesInOrder++;


					// Is this a split packet?
					if ( internalPacket->splitPacketCount > 0 )
					{
						// Generate the split
						// Verify some parameters to make sure we don't get junk data


						// Check for a rebuilt packet
						InsertIntoSplitPacketList( internalPacket, time );

						// Sequenced
						internalPacket = BuildPacketFromSplitPacketList( internalPacket->splitPacketId, time );

						if ( internalPacket )
						{
							// Update our index to the newest packet
							waitingForSequencedPacketReadIndex[ internalPacket->orderingChannel ] = internalPacket->orderingIndex + 1;

							// If there is a rebuilt packet, add it to the output queue
							outputQueue.Push( internalPacket );
							internalPacket = 0;
						}

						// else don't have all the parts yet
					}

					else
					{
						// Update our index to the newest packet
						waitingForSequencedPacketReadIndex[ internalPacket->orderingChannel ] = internalPacket->orderingIndex + 1;

						// Not a split packet. Add the packet to the output queue
						outputQueue.Push( internalPacket );
						internalPacket = 0;
					}
				}
				else
				{
					statistics.sequencedMessagesOutOfOrder++;

					// Older sequenced packet. Discard it
					rakFree(internalPacket->data);
					internalPacketPool.Release( internalPacket );
				}

				goto CONTINUE_SOCKET_DATA_PARSE_LOOP;
			}

			// Is this an unsequenced split packet?
			if ( internalPacket->splitPacketCount > 0 )
			{
				// An unsequenced split packet.  May be ordered though.

				// Check for a rebuilt packet
				if ( internalPacket->reliability != RELIABLE_ORDERED )
					internalPacket->orderingChannel = 255; // Use 255 to designate not sequenced and not ordered

				InsertIntoSplitPacketList( internalPacket, time );

				internalPacket = BuildPacketFromSplitPacketList( internalPacket->splitPacketId, time );

				if ( internalPacket == 0 )
				{

					// Don't have all the parts yet
					goto CONTINUE_SOCKET_DATA_PARSE_LOOP;
				}

				// else continue down to handle RELIABLE_ORDERED


#if defined(DEBUG_SPLIT_PACKET_PROBLEMS)
				printf("%i,", internalPacket->orderingIndex);
#endif
			}

			if ( internalPacket->reliability == RELIABLE_ORDERED )
			{
#ifdef _DEBUG
				assert( internalPacket->orderingChannel < NUMBER_OF_ORDERED_STREAMS );
#endif

				if ( internalPacket->orderingChannel >= NUMBER_OF_ORDERED_STREAMS )
				{
#ifdef _DEBUG
					//printf("Got invalid ordering channel %i from packet %i\n", internalPacket->orderingChannel, internalPacket->messageNumber);
#endif
					// Invalid packet
					rakFree(internalPacket->data);
					internalPacketPool.Release( internalPacket );
					goto CONTINUE_SOCKET_DATA_PARSE_LOOP;
				}

				if ( waitingForOrderedPacketReadIndex[ internalPacket->orderingChannel ] == internalPacket->orderingIndex )
				{
					// Get the list to hold ordered packets for this stream
					DataStructures::LinkedList<InternalPacket*> *orderingListAtOrderingStream;
					unsigned char orderingChannelCopy = internalPacket->orderingChannel;

					statistics.orderedMessagesInOrder++;

					// Show ordering index increment
					//printf("Pushing immediate packet %i with ordering index %i\n", internalPacket->messageNumber, internalPacket->orderingIndex );

					// Push the packet for the user to read
					outputQueue.Push( internalPacket );
					internalPacket = 0; // Don't reference this any longer since other threads access it

					// Wait for the next ordered packet in sequence
					waitingForOrderedPacketReadIndex[ orderingChannelCopy ] ++; // This wraps

					orderingListAtOrderingStream = GetOrderingListAtOrderingStream( orderingChannelCopy );

					if ( orderingListAtOrderingStream != 0)
					{
						while ( orderingListAtOrderingStream->Size() > 0 )
						{
							// Cycle through the list until nothing is found
							orderingListAtOrderingStream->Beginning();
							indexFound=false;
							size=orderingListAtOrderingStream->Size();
							count=0;

							while (count++ < size)
							{
								if ( orderingListAtOrderingStream->Peek()->orderingIndex == waitingForOrderedPacketReadIndex[ orderingChannelCopy ] )
								{
									/*
									RakNet::BitStream temp(orderingListAtOrderingStream->Peek()->data, BITS_TO_BYTES(orderingListAtOrderingStream->Peek()->dataBitLength), false);
									temp.IgnoreBits(8);
									unsigned int receivedPacketNumber=0;
									temp.Read(receivedPacketNumber);
									printf("Receive: receivedPacketNumber=%i orderingIndex=%i waitingFor=%i\n", receivedPacketNumber, orderingListAtOrderingStream->Peek()->orderingIndex, waitingForOrderedPacketReadIndex[ orderingChannelCopy ]);
									*/

									//printf("Pushing delayed packet %i with ordering index %i. outputQueue.Size()==%i\n", orderingListAtOrderingStream->Peek()->messageNumber, orderingListAtOrderingStream->Peek()->orderingIndex, outputQueue.Size() );
									outputQueue.Push( orderingListAtOrderingStream->Pop() );
									waitingForOrderedPacketReadIndex[ orderingChannelCopy ]++;
									indexFound=true;
								}
								else
									(*orderingListAtOrderingStream)++;
							}

							if (indexFound==false)
								break;
						}
					}

					internalPacket = 0;
				}
				else
				{
#if defined(DEBUG_SPLIT_PACKET_PROBLEMS)
					if (internalPacket->splitPacketCount>0)
						printf("Split packet OOO. Waiting=%i. Got=%i\n", waitingForOrderedPacketReadIndex[ internalPacket->orderingChannel ], internalPacket->orderingIndex);
					waitFlag=internalPacket->splitPacketId;
#endif

					//	assert(waitingForOrderedPacketReadIndex[ internalPacket->orderingChannel ] < internalPacket->orderingIndex);
					statistics.orderedMessagesOutOfOrder++;

					// This is a newer ordered packet than we are waiting for. Store it for future use
					AddToOrderingList( internalPacket );
				}

				goto CONTINUE_SOCKET_DATA_PARSE_LOOP;
			}

			// Nothing special about this packet.  Add it to the output queue
			outputQueue.Push( internalPacket );

			internalPacket = 0;
		}

		// Used for a goto to jump to the next packet immediately

CONTINUE_SOCKET_DATA_PARSE_LOOP:
		// Parse the bitstream to create an internal packet
		internalPacket = CreateInternalPacketFromBitStream( &socketData, time );
	}

	/*
	if (numberOfAcksInFrame > 0)
	//		if (time > lastWindowAdjustTime+ping)
	{
	//	printf("Window size up\n");
	windowSize+=1 + numberOfAcksInFrame/windowSize;
	if ( windowSize > MAXIMUM_WINDOW_SIZE )
	windowSize = MAXIMUM_WINDOW_SIZE;
	//lastWindowAdjustTime=time;
	}
	//else
	//	printf("No acks in frame\n");
	*/
	/*
	// numberOfAcksInFrame>=windowSize means that all the packets we last sent from the resendList are cleared out
	// 11/17/05 - the problem with numberOfAcksInFrame >= windowSize is that if the entire frame is filled with resends but not all resends filled the frame
	// then the sender is limited by how many resends can fit in one frame
	if ( numberOfAcksInFrame >= windowSize && ( sendPacketSet[ SYSTEM_PRIORITY ].Size() > 0 || sendPacketSet[ HIGH_PRIORITY ].Size() > 0 || sendPacketSet[ MEDIUM_PRIORITY ].Size() > 0 ) )
	{
	// reliabilityLayerMutexes[windowSize_MUTEX].Lock();
	//printf("windowSize=%i lossyWindowSize=%i\n", windowSize, lossyWindowSize);

	if ( windowSize < lossyWindowSize || (time>lastWindowIncreaseSizeTime && time-lastWindowIncreaseSizeTime>lostPacketResendDelay*2) )   // Increases the window size slowly, testing for packetloss
	{
	// If we get a frame which clears out the resend queue after handling one or more acks, and we have packets waiting to go out,
	// and we didn't recently lose a packet then increase the window size by 1
	windowSize++;

	if ( (time>lastWindowIncreaseSizeTime && time-lastWindowIncreaseSizeTime>lostPacketResendDelay*2) )   // The increase is to test for packetloss
	lastWindowIncreaseSizeTime = time;

	// If the window is so large that we couldn't possibly fit any more packets into the frame, then just leave it alone
	if ( windowSize > MAXIMUM_WINDOW_SIZE )
	windowSize = MAXIMUM_WINDOW_SIZE;

	// SHOW - WINDOWING
	//else
	//	printf("Increasing windowSize to %i.  Lossy window size = %i\n", windowSize, lossyWindowSize);

	// If we are more than 5 over the lossy window size, increase the lossy window size by 1
	if ( windowSize == MAXIMUM_WINDOW_SIZE || windowSize - lossyWindowSize > 5 )
	lossyWindowSize++;
	}
	// reliabilityLayerMutexes[windowSize_MUTEX].Unlock();
	}
	*/

	if (hasAcks)
	{
		UpdateWindowFromAck(time);
	}

	receivePacketCount++;

	return true;
}

//-------------------------------------------------------------------------------------------------------
// This gets an end-user packet already parsed out. Returns number of BITS put into the buffer
//-------------------------------------------------------------------------------------------------------
BitSize_t ReliabilityLayer::Receive( unsigned char **data )
{
	// Wait until the clear occurs
	if (freeThreadedMemoryOnNextUpdate)
		return 0;

	InternalPacket * internalPacket;

	if ( outputQueue.Size() > 0 )
	{
		//  #ifdef _DEBUG
		//  assert(bitStream->GetNumberOfBitsUsed()==0);
		//  #endif
		internalPacket = outputQueue.Pop();

		BitSize_t bitLength;
		*data = internalPacket->data;
		bitLength = internalPacket->dataBitLength;
		internalPacketPool.Release( internalPacket );
		return bitLength;
	}

	else
	{
		return 0;
	}

}

//-------------------------------------------------------------------------------------------------------
// Puts data on the send queue
// bitStream contains the data to send
// priority is what priority to send the data at
// reliability is what reliability to use
// ordering channel is from 0 to 255 and specifies what stream to use
//-------------------------------------------------------------------------------------------------------
bool ReliabilityLayer::Send( char *data, BitSize_t numberOfBitsToSend, PacketPriority priority, PacketReliability reliability, unsigned char orderingChannel, bool makeDataCopy, int MTUSize, RakNetTimeNS currentTime )
{
#ifdef _DEBUG
	assert( !( reliability > RELIABLE_SEQUENCED || reliability < 0 ) );
	assert( !( priority > NUMBER_OF_PRIORITIES || priority < 0 ) );
	assert( !( orderingChannel >= NUMBER_OF_ORDERED_STREAMS ) );
	assert( numberOfBitsToSend > 0 );
#endif


//	int a = BITS_TO_BYTES(numberOfBitsToSend);

	// Fix any bad parameters
	if ( reliability > RELIABLE_SEQUENCED || reliability < 0 )
		reliability = RELIABLE;

	if ( priority > NUMBER_OF_PRIORITIES || priority < 0 )
		priority = HIGH_PRIORITY;

	if ( orderingChannel >= NUMBER_OF_ORDERED_STREAMS )
		orderingChannel = 0;

	// For the first 60 seconds of a new connection, send everything reliable, so that acks arrive and available bandwidth can be determined.
	if (currentTime-(RakNetTimeNS)statistics.connectionStartTime*(RakNetTimeNS)1000 < (RakNetTimeNS)60000000)
	{
		if (reliability==UNRELIABLE)
			reliability=RELIABLE;
		else if (reliability==UNRELIABLE_SEQUENCED)
			reliability=RELIABLE_ORDERED;
	}

	unsigned int numberOfBytesToSend=(unsigned int) BITS_TO_BYTES(numberOfBitsToSend);
	if ( numberOfBitsToSend == 0 )
	{
#ifdef _DEBUG
//		printf( "Error!! ReliabilityLayer::Send bitStream->GetNumberOfBytesUsed()==0\n" );
#endif

		return false;
	}
	InternalPacket * internalPacket = internalPacketPool.Allocate();
	if (internalPacket==0)
	{
		notifyOutOfMemory(__FILE__, __LINE__);
		return false; // Out of memory
	}

	//InternalPacket * internalPacket = sendPacketSet[priority].WriteLock();
#ifdef _DEBUG
	// Remove accessing undefined memory warning
	memset( internalPacket, 255, sizeof( InternalPacket ) );
#endif

	internalPacket->creationTime = currentTime;

	if ( makeDataCopy )
	{
		internalPacket->data = (unsigned char*) rakMalloc( numberOfBytesToSend );
		memcpy( internalPacket->data, data, numberOfBytesToSend );
		//		printf("Allocated %i\n", internalPacket->data);
	}
	else
	{
		// Allocated the data elsewhere, delete it in here
		internalPacket->data = ( unsigned char* ) data;
		//		printf("Using Pre-Allocated %i\n", internalPacket->data);
	}

	internalPacket->dataBitLength = numberOfBitsToSend;
	internalPacket->nextActionTime = 0;

//	printf("%i\n", messageNumber);
	//internalPacket->messageNumber = sendMessageNumberIndex;
	internalPacket->messageNumber = (MessageNumberType) -1;
	internalPacket->messageNumberAssigned=false;

	internalPacket->messageInternalOrder = internalOrderIndex++;

	internalPacket->priority = priority;
	internalPacket->reliability = reliability;
	internalPacket->splitPacketCount = 0;

	// Calculate if I need to split the packet
	int headerLength = BITS_TO_BYTES( GetBitStreamHeaderLength( internalPacket ) );

	unsigned int maxDataSize = MTUSize - UDP_HEADER_SIZE - headerLength;

	if ( encryptor.IsKeySet() )
		maxDataSize -= 16; // Extra data for the encryptor

	bool splitPacket = numberOfBytesToSend > maxDataSize;

	// If a split packet, we might have to upgrade the reliability
	if ( splitPacket )
		statistics.numberOfSplitMessages++;
	else
		statistics.numberOfUnsplitMessages++;

//	++sendMessageNumberIndex;

	if ( internalPacket->reliability == RELIABLE_SEQUENCED || internalPacket->reliability == UNRELIABLE_SEQUENCED )
	{
		// Assign the sequence stream and index
		internalPacket->orderingChannel = orderingChannel;
		internalPacket->orderingIndex = waitingForSequencedPacketWriteIndex[ orderingChannel ] ++;

		// This packet supersedes all other sequenced packets on the same ordering channel
		// Delete all packets in all send lists that are sequenced and on the same ordering channel
		// UPDATE:
		// Disabled.  We don't have enough info to consistently do this.  Sometimes newer data does supercede
		// older data such as with constantly declining health, but not in all cases.
		// For example, with sequenced unreliable sound packets just because you send a newer one doesn't mean you
		// don't need the older ones because the odds are they will still arrive in order
		/*
		for (int i=0; i < NUMBER_OF_PRIORITIES; i++)
		{
		DeleteSequencedPacketsInList(orderingChannel, sendQueue[i]);
		}
		*/
	}

	else
		if ( internalPacket->reliability == RELIABLE_ORDERED )
		{
			// Assign the ordering channel and index
			internalPacket->orderingChannel = orderingChannel;
			internalPacket->orderingIndex = waitingForOrderedPacketWriteIndex[ orderingChannel ] ++;
		}

		if ( splitPacket )   // If it uses a secure header it will be generated here
		{
			// Must split the packet.  This will also generate the SHA1 if it is required. It also adds it to the send list.
			//InternalPacket packetCopy;
			//memcpy(&packetCopy, internalPacket, sizeof(InternalPacket));
			//sendPacketSet[priority].CancelWriteLock(internalPacket);
			//SplitPacket( &packetCopy, MTUSize );
			SplitPacket( internalPacket, MTUSize );
			//delete [] packetCopy.data;
			return true;
		}

		sendPacketSet[ internalPacket->priority ].Push( internalPacket );

		//	sendPacketSet[priority].WriteUnlock();
		return true;
}
//-------------------------------------------------------------------------------------------------------
// Run this once per game cycle.  Handles internal lists and actually does the send
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::Update( SOCKET s, SystemAddress systemAddress, int MTUSize, RakNetTimeNS time,
							  unsigned bitsPerSecondLimit,
							  DataStructures::List<PluginInterface*> &messageHandlerList )
{
	// Debug memory leaks
	/*
	if (0)
	{
		printf("orderingList.Size()=%i\n", orderingList.Size());
		printf("outputQueue.Size()=%i\n", outputQueue.Size());
		printf("acknowlegements.Size()=%i\n", acknowlegements.Size());
		printf("resendList.Size()=%i\n", resendList.Size());
		printf("resendQueue.Size()=%i\n", resendQueue.Size());
		for (int i=0; i < NUMBER_OF_PRIORITIES; i++)
			printf("sendPacketSet[%i].Size()=%i\n", i, sendPacketSet[i].Size());
		printf("splitPacketChannelList.Size()=%i\n", splitPacketChannelList.Size());
		printf("hasReceivedPacketQueue.Size()=%i\n", hasReceivedPacketQueue.Size());
#ifndef _RELEASE
		printf("delayList.Size()=%i\n", delayList.Size());
#endif
		printf("internalPacketPool.GetAvailablePagesSize()=%i\n", internalPacketPool.GetAvailablePagesSize());
		printf("internalPacketPool.GetUnavailablePagesSize()=%i\n", internalPacketPool.GetUnavailablePagesSize());
		printf("internalPacketPool.GetMemoryPoolPageSize()=%i\n", internalPacketPool.GetMemoryPoolPageSize());
		printf("sendMessageNumberIndex=%i\n", sendMessageNumberIndex);
		printf("deadConnection=%i\n", deadConnection);
		printf("cheater=%i\n", cheater);
		printf("splitPacketId=%i\n", splitPacketId);
		printf("blockWindowIncreaseUntilTime=%i\n", blockWindowIncreaseUntilTime);
		printf("receivedPacketsBaseIndex=%i\n", receivedPacketsBaseIndex);
		printf("lastUpdateTime=%I64u\n", lastUpdateTime);
		printf("timeBetweenPackets=%I64u\n", timeBetweenPackets);
		printf("nextSendTime=%I64u\n", nextSendTime);
		printf("ackPing=%I64u\n", ackPing);
		printf("ackPingSum=%I64u\n", ackPingSum);
		printf("ackPingIndex=%i\n", ackPingIndex);
		printf("continuousSend=%i\n", continuousSend);
		printf("sendPacketCount=%i\n", sendPacketCount);
		printf("receivePacketCount=%i\n", receivePacketCount);
		char buffer[10000];
		StatisticsToString( &statistics, buffer, 2 );
		printf("%s\n", buffer);
	}
*/

	// This line is necessary because the timer isn't accurate
	if (time <= lastUpdateTime)
	{
		// Always set the last time in case of overflow
		lastUpdateTime=time;
		return;
	}

	RakNetTimeNS elapsed = time - lastUpdateTime;
	lastUpdateTime=time;
	UpdateThreadedMemory();

	// Due to thread vagarities and the way I store the time to avoid slow calls to RakNet::GetTime
	// time may be less than lastAck
	if ( resendList.IsEmpty()==false && AckTimeout(time) )
	{
		// SHOW - dead connection
		// printf("The connection has been lost.\n");
		// We've waited a very long time for a reliable packet to get an ack and it never has
		deadConnection = true;
		return;
	}

	if (time >= histogramStart + HISTOGRAM_RESTART_CYCLE)
	{
		histogramStart=time;
		histogramBitsSent=0;
	}

	bool limitThroughput;
	if (bitsPerSecondLimit==0 || throughputCapCountdown <= 0)
		limitThroughput=false;
	else
	{
		throughputCapCountdown-=elapsed;
		if (throughputCapCountdown<0)
		{
			limitThroughput=false;
			throughputCapCountdown=0;
		}
		else
		{
			limitThroughput=true;
			continuousSend=false; // Blocked from sending due to throughput limit, so not sending continuously
		}
	}

	const int maxDataBitSize = BYTES_TO_BITS((MTUSize - UDP_HEADER_SIZE));
	bool hitMTUCap;
	bool reliableDataSent;
	RakNetTimeNS usedTime;
#ifdef _ENABLE_FLOW_CONTROL
	RakNetTimeNS availableTime;
	if (limitThroughput==false)
	{
		if (time >= nextSendTime)
		{
			hitMTUCap=false;
			usedTime=(RakNetTimeNS)0;
			availableTime = time - nextSendTime;
			if (availableTime > MAX_TIME_BETWEEN_PACKETS)
				availableTime=MAX_TIME_BETWEEN_PACKETS;

			while ( usedTime <= availableTime )
			{
#else
	while (1)
	{
#endif
				(void) reliableDataSent;
				updateBitStream.Reset();
				if ( GenerateDatagram( &updateBitStream, MTUSize, &reliableDataSent, time, systemAddress, &hitMTUCap, messageHandlerList ) )
				{
#ifndef _RELEASE
					if (minExtraPing > 0 || extraPingVariance > 0)
					{
						// Delay the send to simulate lag
						DataAndTime *dt;
						dt = new DataAndTime;
						memcpy( dt->data, updateBitStream.GetData(), (size_t) updateBitStream.GetNumberOfBytesUsed() );
						dt->length = (unsigned int) updateBitStream.GetNumberOfBytesUsed();
						dt->sendTime = time + (RakNetTimeNS)minExtraPing*1000;
						if (extraPingVariance > 0)
							dt->sendTime += ( randomMT() % (int)extraPingVariance );
						delayList.Insert( dt );
					}
					else
#endif
					{
						SendBitStream( s, systemAddress, &updateBitStream );
					}

#ifdef _ENABLE_FLOW_CONTROL
					if (bitsPerSecondLimit > 0)
					{
						throughputCapCountdown+=(RakNetTimeNS)(1000000.0 * (double) updateBitStream.GetNumberOfBitsUsed() / (double) bitsPerSecondLimit);
						// Prevent sending a large burst and then doing nothing for a long time
						if (throughputCapCountdown>30000)
							break;
					}

					if (hitMTUCap)
						usedTime+=timeBetweenPackets;
					else
						usedTime+=(RakNetTimeNS)((long double) timeBetweenPackets * ((long double) updateBitStream.GetNumberOfBitsUsed() / (long double) maxDataBitSize));
#endif
				}
				else
					break;
			}

#ifdef _ENABLE_FLOW_CONTROL
			if ( nextSendTime + MAX_TIME_BETWEEN_PACKETS < time )
			{
				// Long time since last send
				nextSendTime=time+usedTime;
				continuousSend=false;
			}
			else
			{
				nextSendTime+=usedTime;
				if (nextSendTime>time)
				{
					continuousSend=true;
				}
				else
				{
					nextSendTime+=usedTime;
					if (nextSendTime<time)
						nextSendTime=time;
					continuousSend=false;
				}
			}
		}
		else
		{
			// Send acks only
			if (acknowlegements.Size()>0)
			{
				updateBitStream.Reset();
				updateBitStream.Write(true);
				updateBitStream.Write(remoteSystemTime);
				BitSize_t bitsSent = acknowlegements.Serialize(&updateBitStream, (MTUSize-UDP_HEADER_SIZE)*8-1, true);
				statistics.acknowlegementBitsSent +=bitsSent;
				SendBitStream( s, systemAddress, &updateBitStream );

				if (bitsPerSecondLimit > 0)
				{
					throughputCapCountdown+=(RakNetTimeNS)(1000000.0 * (double) updateBitStream.GetNumberOfBitsUsed() / (double) bitsPerSecondLimit);
				}
			}
		}
	}
#endif
	

#ifndef _RELEASE
	// Do any lagged sends
	unsigned i = 0;
	while ( i < delayList.Size() )
	{
		if ( delayList[ i ]->sendTime < time )
		{
			updateBitStream.Reset();
			updateBitStream.Write( delayList[ i ]->data, delayList[ i ]->length );
			// Send it now
			SendBitStream( s, systemAddress, &updateBitStream );

			delete delayList[ i ];
			if (i != delayList.Size() - 1)
				delayList[ i ] = delayList[ delayList.Size() - 1 ];
			delayList.RemoveFromEnd();
		}

		else
			i++;
	}
#endif

	// Keep on top of deleting old unreliable split packets so they don't clog the list.
	DeleteOldUnreliableSplitPackets( time );
}

//-------------------------------------------------------------------------------------------------------
// Writes a bitstream to the socket
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::SendBitStream( SOCKET s, SystemAddress systemAddress, RakNet::BitStream *bitStream )
{
	// SHOW - showing reliable flow
	// if (bitStream->GetNumberOfBytesUsed()>50)
	//  printf("Sending %i bytes. sendQueue[0].Size()=%i, resendList.Size()=%i\n", bitStream->GetNumberOfBytesUsed(), sendQueue[0].Size(),resendList.Size());

	unsigned int oldLength, length;

	// sentFrames++;

#ifndef _RELEASE
	if (maxSendBPS>0)
	{
//		double chanceToLosePacket = (double)currentBandwidth / (double)maxSendBPS;
	//	if (frandomMT() < (float)chanceToLosePacket)
		//	return;

		

	}
#endif
// REMOVEME
//	if (frandomMT() < .15f)
//		return;

	// Encode the whole bitstream if the encoder is defined.

	if ( encryptor.IsKeySet() )
	{
		length = (unsigned int) bitStream->GetNumberOfBytesUsed();
		oldLength = length;

		encryptor.Encrypt( ( unsigned char* ) bitStream->GetData(), length, ( unsigned char* ) bitStream->GetData(), &length );
		statistics.encryptionBitsSent = BYTES_TO_BITS( length - oldLength );
		
		assert( ( length % 16 ) == 0 );
	}
	else
	{
		length = (unsigned int) bitStream->GetNumberOfBytesUsed();
	}

	statistics.packetsSent++;
	statistics.totalBitsSent += length * 8;
	histogramBitsSent += length * 8;
	//printf("total bits=%i length=%i\n", BITS_TO_BYTES(statistics.totalBitsSent), length);

	SocketLayer::Instance()->SendTo( s, ( char* ) bitStream->GetData(), length, systemAddress.binaryAddress, systemAddress.port );

	// lastPacketSendTime=time;
}

//-------------------------------------------------------------------------------------------------------
// Generates a datagram (coalesced packets)
//-------------------------------------------------------------------------------------------------------
bool ReliabilityLayer::GenerateDatagram( RakNet::BitStream *output, int MTUSize, bool *reliableDataSent, RakNetTimeNS time, SystemAddress systemAddress, bool *hitMTUCap, DataStructures::List<PluginInterface*> &messageHandlerList  )
{
	InternalPacket * internalPacket;
	//	InternalPacket *temp;
	BitSize_t maxDataBitSize;
	BitSize_t reliableBits = 0;
	BitSize_t nextPacketBitLength;
	unsigned i, messageHandlerIndex;
	bool isReliable, onlySendUnreliable;
	//bool writeFalseToHeader;
	bool wroteAcks;
	bool wroteData;
	unsigned messagesSent=0;
        *hitMTUCap=false;
        
	maxDataBitSize = MTUSize - UDP_HEADER_SIZE;

	if ( encryptor.IsKeySet() )
		maxDataBitSize -= 16; // Extra data for the encryptor

	maxDataBitSize <<= 3;

	*reliableDataSent = false;

	if (acknowlegements.Size()>0)
	{
		output->Write(true);
		output->Write(remoteSystemTime);
		messagesSent++;
		wroteAcks=true;
		statistics.acknowlegementBitsSent +=acknowlegements.Serialize(output, (MTUSize-UDP_HEADER_SIZE)*8-1, true);

		//	writeFalseToHeader=false;
	}
	else
	{
		wroteAcks=false;
		output->Write(false);

		//		writeFalseToHeader=true;
	}

	wroteData=false;


	while ( resendQueue.Size() > 0 )
	{
		internalPacket = resendQueue.Peek();
		RakAssert(internalPacket->messageNumberAssigned==true);
		// The resend Queue can have holes.  This is so we can deallocate blocks without having to compress the array
		if ( internalPacket->nextActionTime == 0 )
		{
			if (packetlossThisSampleResendCount>0)
				packetlossThisSampleResendCount--;

			resendQueue.Pop();
			rakFree(internalPacket->data);
			internalPacketPool.Release( internalPacket );
			continue; // This was a hole
		}

		if ( resendQueue.Peek()->nextActionTime < time )
		{
			internalPacket = resendQueue.Pop();

			nextPacketBitLength = GetBitStreamHeaderLength( internalPacket ) + internalPacket->dataBitLength;

			if ( output->GetNumberOfBitsUsed() + nextPacketBitLength > maxDataBitSize )
			{
				resendQueue.PushAtHead( internalPacket ); // Not enough room to use this packet after all!
                *hitMTUCap=true;

				goto END_OF_GENERATE_FRAME;
			}

			RakAssert(internalPacket->priority >= 0);

#ifdef _DEBUG_LOGGER
			{
				char temp[256];
				sprintf(temp, "%p: Resending packet %i data: %i bitlen: %i\n", this, internalPacket->messageNumber, (unsigned char) internalPacket->data[0], internalPacket->dataBitLength);
				OutputDebugStr(temp);
			}
#endif

			for (messageHandlerIndex=0; messageHandlerIndex < messageHandlerList.Size(); messageHandlerIndex++)
				messageHandlerList[messageHandlerIndex]->OnInternalPacket(internalPacket, sendPacketCount, systemAddress, (RakNetTime)(time/(RakNetTimeNS)1000), true);

			// Write to the output bitstream
			statistics.messageResends++;
			statistics.messageDataBitsResent += internalPacket->dataBitLength;

			//			if (writeFalseToHeader)
			//			{
			//				output->Write(false);
			//				writeFalseToHeader=false;
			//			}

			if (wroteData==false)
			{
				wroteData=true;
				output->Write(true);
				// Write our own system time for ping calculations for flow control
				output->Write((RemoteSystemTimeType)(time / (RakNetTimeNS)1000));
			}
			statistics.messagesTotalBitsResent += WriteToBitStreamFromInternalPacket( output, internalPacket, time );
			internalPacket->packetNumber=sendPacketCount;
			messagesSent++;

			*reliableDataSent = true;

			statistics.packetsContainingOnlyAcknowlegementsAndResends++;

	//		printf("internalPacket->messageNumber=%i time=%i timeBetween=%i\n", internalPacket->messageNumber, (RakNetTime)(time/(RakNetTimeNS)1000), (RakNetTime)(timeBetweenPackets/(RakNetTimeNS)1000));
			// Using a one second resend for the 2nd resend on fixes abnormal disconnections due to flooding
			internalPacket->nextActionTime = time + (RakNetTimeNS)1000000;

			//printf("PACKETLOSS\n ");

			packetlossThisSample=true;

			//printf("packetlossThisSample=true\n");
			if (packetlossThisSampleResendCount==0)
			{
				backoffThisSample=true;
				packetlossThisSampleResendCount=resendQueue.Size();
			}
			else
			{
				packetlossThisSampleResendCount--;
			}

			// Put the packet back into the resend list at the correct spot
			// Don't make a copy since I'm reinserting an allocated struct
			InsertPacketIntoResendList( internalPacket, time, false, false );
			
		}
		else
		{
			break;
		}
	}


	onlySendUnreliable = false;


	// From highest to lowest priority, fill up the output bitstream from the send lists
	for ( i = 0; i < NUMBER_OF_PRIORITIES; i++ )
	{
		while ( sendPacketSet[ i ].Size() )
		{
			internalPacket = sendPacketSet[ i ].Pop();

			// REMOVEME
	//		if (internalPacket->messageNumber > 30000)
	//		{
	//			int a=5;
	//		}


			nextPacketBitLength = GetBitStreamHeaderLength( internalPacket ) + internalPacket->dataBitLength;

			if (unreliableTimeout!=0 &&
				(internalPacket->reliability==UNRELIABLE || internalPacket->reliability==UNRELIABLE_SEQUENCED) &&
				time > internalPacket->creationTime+(RakNetTimeNS)unreliableTimeout)
			{
				// Unreliable packets are deleted
				rakFree(internalPacket->data);
				internalPacketPool.Release( internalPacket );
				continue;
			}


			if ( output->GetNumberOfBitsUsed() + nextPacketBitLength > maxDataBitSize )
			{
				// This output won't fit.
                *hitMTUCap=true;
				sendPacketSet[ i ].PushAtHead( internalPacket ); // Push this back at the head so it is the next thing to go out
				break;
			}

			if ( internalPacket->reliability == RELIABLE || internalPacket->reliability == RELIABLE_SEQUENCED || internalPacket->reliability == RELIABLE_ORDERED )
				isReliable = true;
			else
				isReliable = false;

			// Write to the output bitstream
			statistics.messagesSent[ i ] ++;
			statistics.messageDataBitsSent[ i ] += internalPacket->dataBitLength;

#ifdef _DEBUG_LOGGER
			{
				char temp[256];
				sprintf(temp, "%p: Sending packet %i data: %i bitlen: %i\n", this, internalPacket->messageNumber, (unsigned char) internalPacket->data[0], internalPacket->dataBitLength);
				OutputDebugStr(temp);
			}
#endif

			for (messageHandlerIndex=0; messageHandlerIndex < messageHandlerList.Size(); messageHandlerIndex++)
				messageHandlerList[messageHandlerIndex]->OnInternalPacket(internalPacket, sendPacketCount, systemAddress, (RakNetTime)(time/(RakNetTimeNS)1000), true);

			//			if (writeFalseToHeader)
			//			{
			//				output->Write(false);
			//				writeFalseToHeader=false;
			//			}
			if (wroteData==false)
			{
				wroteData=true;
				output->Write(true);
				// Write our own system time for ping calculations for flow control
				output->Write((RemoteSystemTimeType)(time / (RakNetTimeNS)1000));
			}
			// I assign the message number in the order the messages are sent, rather than the order they are created.
			// This fixes a bug where I was formerly creating a huge number of split packets, then sending on a different ordering channel on a higher priority.
			// This would cause a huge gap on the recipient for receivedPacketsBaseIndex causing performance problems and data timeout
			RakAssert(internalPacket->messageNumber==(MessageNumberType)-1);
			RakAssert(internalPacket->messageNumberAssigned==false);
			internalPacket->messageNumberAssigned=true;
			internalPacket->messageNumber=sendMessageNumberIndex;
			sendMessageNumberIndex++;
#if defined (DEBUG_SPLIT_PACKET_PROBLEMS)
//			if (internalPacket->splitPacketId!=66)
#endif
			statistics.messageTotalBitsSent[ i ] += WriteToBitStreamFromInternalPacket( output, internalPacket, time );
			//output->PrintBits();
			internalPacket->packetNumber=sendPacketCount;
			messagesSent++;

			if ( isReliable )
			{
				// Reliable packets are saved to resend later
				reliableBits += internalPacket->dataBitLength;
	//			printf("internalPacket->messageNumber=%i time=%i timeBetween=%i\n", internalPacket->messageNumber, (RakNetTime)(time/(RakNetTimeNS)1000), (RakNetTime)(timeBetweenPackets/(RakNetTimeNS)1000));
				if (ackPingSum==0)
					internalPacket->nextActionTime = time + (RakNetTimeNS)1000000;
				else
					internalPacket->nextActionTime = time + (RakNetTimeNS)100000 + (ackPingSum>>7);

#if defined (DEBUG_SPLIT_PACKET_PROBLEMS)
//				if (internalPacket->splitPacketId==66)
//					internalPacket->nextActionTime+=1000000;
#endif

				resendList.Insert( internalPacket->messageNumber, internalPacket);
				//printf("ackTimeIncrement=%i\n", ackTimeIncrement/1000);
				InsertPacketIntoResendList( internalPacket, time, false, true);
				*reliableDataSent = true;
			}
			else
			{
				// Unreliable packets are deleted
				rakFree(internalPacket->data);
				internalPacketPool.Release( internalPacket );
			}
		}
	}

END_OF_GENERATE_FRAME:
	;

	// if (output->GetNumberOfBitsUsed()>0)
	// {
	// Update the throttle with the header
	//  bytesSent+=output->GetNumberOfBytesUsed() + UDP_HEADER_SIZE;
	//}

	if (wroteData==false)
	{
		output->Write(false);
	}

	if (wroteAcks || wroteData)
		sendPacketCount++;

	return wroteAcks || wroteData;
}

//-------------------------------------------------------------------------------------------------------
// Are we waiting for any data to be sent out or be processed by the player?
//-------------------------------------------------------------------------------------------------------
bool ReliabilityLayer::IsOutgoingDataWaiting(void)
{
	unsigned i;
	for ( i = 0; i < NUMBER_OF_PRIORITIES; i++ )
	{
		if (sendPacketSet[ i ].Size() > 0)
			return true;
	}

	return acknowlegements.Size() > 0 || resendList.IsEmpty()==false;// || outputQueue.Size() > 0 || orderingList.Size() > 0 || splitPacketChannelList.Size() > 0;
}
bool ReliabilityLayer::IsReliableOutgoingDataWaiting(void)
{
	unsigned i,j;
	for ( i = 0; i < NUMBER_OF_PRIORITIES; i++ )
	{
		for (j=0; j < sendPacketSet[ i ].Size(); j++)
		{
			if (sendPacketSet[ i ][ j ]->reliability==RELIABLE_ORDERED ||
				sendPacketSet[ i ][ j ]->reliability==RELIABLE_SEQUENCED ||
				sendPacketSet[ i ][ j ]->reliability==RELIABLE)
				return true;
		}
	}


	return /*acknowlegements.Size() > 0 ||*/ resendList.IsEmpty()==false;// || outputQueue.Size() > 0 || orderingList.Size() > 0 || splitPacketChannelList.Size() > 0;
}

bool ReliabilityLayer::AreAcksWaiting(void)
{
	return acknowlegements.Size() > 0;
}

//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::ApplyNetworkSimulator( double _maxSendBPS, RakNetTime _minExtraPing, RakNetTime _extraPingVariance )
{
#ifndef _RELEASE
	maxSendBPS=_maxSendBPS;
	minExtraPing=_minExtraPing;
	extraPingVariance=_extraPingVariance;
	//	if (ping < (unsigned int)(minExtraPing+extraPingVariance)*2)
	//		ping=(minExtraPing+extraPingVariance)*2;
#endif
}
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::SetSplitMessageProgressInterval(int interval)
{
	splitMessageProgressInterval=interval;
}
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::SetUnreliableTimeout(RakNetTime timeoutMS)
{
	unreliableTimeout=(RakNetTimeNS)timeoutMS*(RakNetTimeNS)1000;
}

//-------------------------------------------------------------------------------------------------------
// This will return true if we should not send at this time
//-------------------------------------------------------------------------------------------------------
bool ReliabilityLayer::IsSendThrottled( int MTUSize )
{
	(void) MTUSize;

	return false;
	//	return resendList.Size() > windowSize;

	// Disabling this, because it can get stuck here forever
	/*
	unsigned packetsWaiting;
	unsigned resendListDataSize=0;
	unsigned i;
	for (i=0; i < resendList.Size(); i++)
	{
	if (resendList[i])
	resendListDataSize+=resendList[i]->dataBitLength;
	}
	packetsWaiting = 1 + ((BITS_TO_BYTES(resendListDataSize)) / (MTUSize - UDP_HEADER_SIZE - 10)); // 10 to roughly estimate the raknet header

	return packetsWaiting >= windowSize;
	*/
}

//-------------------------------------------------------------------------------------------------------
// We lost a packet
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::UpdateWindowFromPacketloss( RakNetTimeNS time )
{
	(void) time;
}

//-------------------------------------------------------------------------------------------------------
// Increase the window size
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::UpdateWindowFromAck( RakNetTimeNS time )
{
	(void) time;
}

//-------------------------------------------------------------------------------------------------------
// Does what the function name says
//-------------------------------------------------------------------------------------------------------
unsigned ReliabilityLayer::RemovePacketFromResendListAndDeleteOlderReliableSequenced( const MessageNumberType messageNumber, RakNetTimeNS time )
{
	(void) time;

	InternalPacket * internalPacket;
	//InternalPacket *temp;
	PacketReliability reliability; // What type of reliability algorithm to use with this packet
	unsigned char orderingChannel; // What ordering channel this packet is on, if the reliability type uses ordering channels
	OrderingIndexType orderingIndex; // The ID used as identification for ordering channels
	//	unsigned j;

	bool deleted;
	deleted=resendList.Delete(messageNumber, internalPacket);
	if (deleted)
	{
		reliability = internalPacket->reliability;
		orderingChannel = internalPacket->orderingChannel;
		orderingIndex = internalPacket->orderingIndex;
		//		delete [] internalPacket->data;
		//		internalPacketPool.ReleasePointer( internalPacket );
		internalPacket->nextActionTime=0; // Will be freed in the update function
		return 0;

		// Rarely used and thus disabled for speed
		/*
		// If the deleted packet was reliable sequenced, also delete all older reliable sequenced resends on the same ordering channel.
		// This is because we no longer need to send these.
		if ( reliability == RELIABLE_SEQUENCED )
		{
		unsigned j = 0;
		while ( j < resendList.Size() )
		{
		internalPacket = resendList[ j ];

		if ( internalPacket && internalPacket->reliability == RELIABLE_SEQUENCED && internalPacket->orderingChannel == orderingChannel && IsOlderOrderedPacket( internalPacket->orderingIndex, orderingIndex ) )
		{
		// Delete the packet
		delete [] internalPacket->data;
		internalPacketPool.ReleasePointer( internalPacket );
		resendList[ j ] = 0; // Generate a hole
		}

		j++;
		}

		}
		*/
	}
	else
	{

		statistics.duplicateAcknowlegementsReceived++;
	}

	return (unsigned)-1;
}

//-------------------------------------------------------------------------------------------------------
// Acknowledge receipt of the packet with the specified messageNumber
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::SendAcknowledgementPacket( const MessageNumberType messageNumber, RakNetTimeNS time )
{
	(void) time;
	statistics.acknowlegementsSent++;
	acknowlegements.Insert(messageNumber);
}

//-------------------------------------------------------------------------------------------------------
// Parse an internalPacket and figure out how many header bits would be
// written.  Returns that number
//-------------------------------------------------------------------------------------------------------
int ReliabilityLayer::GetBitStreamHeaderLength( const InternalPacket *const internalPacket )
{
#ifdef _DEBUG
	assert( internalPacket );
#endif

	int bitLength;

	bitLength=sizeof(MessageNumberType)*8;

	// Write the PacketReliability.  This is encoded in 3 bits
	//bitStream->WriteBits((unsigned char*)&(internalPacket->reliability), 3, true);
	bitLength += 3;

	// If the reliability requires an ordering channel and ordering index, we Write those.
	if ( internalPacket->reliability == UNRELIABLE_SEQUENCED || internalPacket->reliability == RELIABLE_SEQUENCED || internalPacket->reliability == RELIABLE_ORDERED )
	{
		// ordering channel encoded in 5 bits (from 0 to 31)
		//bitStream->WriteBits((unsigned char*)&(internalPacket->orderingChannel), 5, true);
		bitLength+=5;

		// ordering index is one byte
		//bitStream->WriteCompressed(internalPacket->orderingIndex);
		bitLength+=sizeof(OrderingIndexType)*8;
	}

//	if (internalPacket->reliability == RELIABLE || internalPacket->reliability == RELIABLE_ORDERED || internalPacket->reliability==RELIABLE_SEQUENCED)
	//	bitLength+=8*sizeof(RemoteSystemTimeType);

	// Write if this is a split packet (1 bit)
	bool isSplitPacket = internalPacket->splitPacketCount > 0;

	//bitStream->Write(isSplitPacket);
	bitLength += 1;

	if ( isSplitPacket )
	{
		// split packet indices are two bytes (so one packet can be split up to 65535
		// times - maximum packet size would be about 500 * 65535)
		//bitStream->Write(internalPacket->splitPacketId);
		//bitStream->WriteCompressed(internalPacket->splitPacketIndex);
		//bitStream->WriteCompressed(internalPacket->splitPacketCount);
		bitLength += (sizeof(SplitPacketIdType) + sizeof(SplitPacketIndexType) * 2) * 8;
	}

	// Write how many bits the packet data is. Stored in an unsigned short and
	// read from 16 bits
	//bitStream->WriteBits((unsigned char*)&(internalPacket->dataBitLength), 16, true);

	// Read how many bits the packet data is.  Stored in 16 bits
	bitLength += 16;

	// Byte alignment
	//bitLength += 8 - ((bitLength -1) %8 + 1);

	return bitLength;
}

//-------------------------------------------------------------------------------------------------------
// Parse an internalPacket and create a bitstream to represent this data
//-------------------------------------------------------------------------------------------------------
BitSize_t ReliabilityLayer::WriteToBitStreamFromInternalPacket( RakNet::BitStream *bitStream, const InternalPacket *const internalPacket, RakNetTimeNS curTime )
{
	(void) curTime;

#ifdef _DEBUG
	assert( bitStream && internalPacket );
#endif

	BitSize_t start = bitStream->GetNumberOfBitsUsed();
	const unsigned char c = (unsigned char) internalPacket->reliability;

	// testing
	//if (internalPacket->reliability==UNRELIABLE)
	//  printf("Sending unreliable packet %i\n", internalPacket->messageNumber);
	//else if (internalPacket->reliability==RELIABLE_SEQUENCED || internalPacket->reliability==RELIABLE_ORDERED || internalPacket->reliability==RELIABLE)
	//	  printf("Sending reliable packet number %i\n", internalPacket->messageNumber);

	//bitStream->AlignWriteToByteBoundary();

	// Write the message number (2 bytes)
	bitStream->Write( internalPacket->messageNumber );
	
	// REMOVEME
//	if (internalPacket->messageNumber > 30000)
//	{
//		int a=5;
//	}


	// Acknowledgment packets have no more data than the messageNumber and whether it is anacknowledgment


#ifdef _DEBUG
	assert( internalPacket->dataBitLength > 0 );
#endif

	// Write the PacketReliability.  This is encoded in 3 bits
	bitStream->WriteBits( (const unsigned char *)&c, 3, true );

	// If the reliability requires an ordering channel and ordering index, we Write those.
	if ( internalPacket->reliability == UNRELIABLE_SEQUENCED || internalPacket->reliability == RELIABLE_SEQUENCED || internalPacket->reliability == RELIABLE_ORDERED )
	{
		// ordering channel encoded in 5 bits (from 0 to 31)
		bitStream->WriteBits( ( unsigned char* ) & ( internalPacket->orderingChannel ), 5, true );

		// One or two bytes
		bitStream->Write( internalPacket->orderingIndex );
	}

//	if (internalPacket->reliability == RELIABLE || internalPacket->reliability == RELIABLE_ORDERED || internalPacket->reliability==RELIABLE_SEQUENCED)
	//{
	//	// Write the time in reliable packets for flow control
//		bitStream->Write((RemoteSystemTimeType) (curTime/(RakNetTimeNS)1000));
//	}

	// Write if this is a split packet (1 bit)
	bool isSplitPacket = internalPacket->splitPacketCount > 0;

	bitStream->Write( isSplitPacket );

	if ( isSplitPacket )
	{
		bitStream->Write( internalPacket->splitPacketId );
		bitStream->WriteCompressed( internalPacket->splitPacketIndex );
		bitStream->WriteCompressed( internalPacket->splitPacketCount );
	}

	// Write how many bits the packet data is. Stored in 13 bits
#ifdef _DEBUG
	assert( BITS_TO_BYTES( internalPacket->dataBitLength ) < MAXIMUM_MTU_SIZE ); // I never send more than MTU_SIZE bytes

#endif

	unsigned short length = ( unsigned short ) internalPacket->dataBitLength; // Ignore the 2 high bytes for WriteBits

	bitStream->WriteCompressed( length );

	// Write the actual data.
	bitStream->WriteAlignedBytes( ( unsigned char* ) internalPacket->data, BITS_TO_BYTES( internalPacket->dataBitLength ) );

	//bitStream->WriteBits((unsigned char*)internalPacket->data, internalPacket->dataBitLength);

	//printf("WRITE:\n");
	//bitStream->PrintBits();


	return bitStream->GetNumberOfBitsUsed() - start;
}

//-------------------------------------------------------------------------------------------------------
// Parse a bitstream and create an internal packet to represent this data
//-------------------------------------------------------------------------------------------------------
InternalPacket* ReliabilityLayer::CreateInternalPacketFromBitStream( RakNet::BitStream *bitStream, RakNetTimeNS time )
{
	bool bitStreamSucceeded;
	InternalPacket* internalPacket;

	if ( bitStream->GetNumberOfUnreadBits() < (int) sizeof( internalPacket->messageNumber ) * 8 )
		return 0; // leftover bits

	internalPacket = internalPacketPool.Allocate();

	//printf("READ:\n");
	//bitStream->PrintBits();

	if (internalPacket==0)
	{
		// Out of memory
		RakAssert(0);
		return 0;
	}

#ifdef _DEBUG
	// Remove accessing undefined memory error
	memset( internalPacket, 255, sizeof( InternalPacket ) );
#endif

	internalPacket->creationTime = time;

	//bitStream->AlignReadToByteBoundary();

	// Read the packet number (4 bytes)
	bitStreamSucceeded = bitStream->Read( internalPacket->messageNumber );

	// REMOVEME
//	if (internalPacket->messageNumber > 30000)
//	{
//		int a=5;
//	}

#ifdef _DEBUG
	// 10/08/05 - Disabled assert since this hits from offline packets
	//assert( bitStreamSucceeded );
#endif

	if ( bitStreamSucceeded == false )
	{
		internalPacketPool.Release( internalPacket );
		return 0;
	}

#ifdef _DEBUG
	// 10/08/05 - Disabled assert since this hits from offline packets
	//assert( bitStreamSucceeded );
#endif

	// Read the PacketReliability. This is encoded in 3 bits
	unsigned char reliability;

	bitStreamSucceeded = bitStream->ReadBits( ( unsigned char* ) ( &( reliability ) ), 3 );

	internalPacket->reliability = ( const PacketReliability ) reliability;

#ifdef _DEBUG
	// 10/08/05 - Disabled assert since this hits from offline packets
	// assert( bitStreamSucceeded );
#endif

	if ( bitStreamSucceeded == false )
	{
		internalPacketPool.Release( internalPacket );
		return 0;
	}

	// If the reliability requires an ordering channel and ordering index, we read those.
	if ( internalPacket->reliability == UNRELIABLE_SEQUENCED || internalPacket->reliability == RELIABLE_SEQUENCED || internalPacket->reliability == RELIABLE_ORDERED )
	{
		// ordering channel encoded in 5 bits (from 0 to 31)
		bitStreamSucceeded = bitStream->ReadBits( ( unsigned char* ) & ( internalPacket->orderingChannel ), 5 );
#ifdef _DEBUG
		// 10/08/05 - Disabled assert since this hits from offline packets
		//assert( bitStreamSucceeded );
#endif

		if ( bitStreamSucceeded == false )
		{
			internalPacketPool.Release( internalPacket );
			return 0;
		}

		bitStreamSucceeded = bitStream->Read( internalPacket->orderingIndex );

#ifdef _DEBUG
		// 10/08/05 - Disabled assert since this hits from offline packets
		//assert( bitStreamSucceeded );
#endif

		if ( bitStreamSucceeded == false )
		{
			internalPacketPool.Release( internalPacket );
			return 0;
		}
	}

//	if (internalPacket->reliability == RELIABLE || internalPacket->reliability == RELIABLE_ORDERED || internalPacket->reliability==RELIABLE_SEQUENCED)
//	{
//		// Read the time in reliable packets for flow control
//		bitStream->Read(internalPacket->remoteSystemTime);
//	}


	// Read if this is a split packet (1 bit)
	bool isSplitPacket;

	bitStreamSucceeded = bitStream->Read( isSplitPacket );

#ifdef _DEBUG
	// 10/08/05 - Disabled assert since this hits from offline packets
	//assert( bitStreamSucceeded );
#endif

	if ( bitStreamSucceeded == false )
	{
		internalPacketPool.Release( internalPacket );
		return 0;
	}

	if ( isSplitPacket )
	{
		bitStreamSucceeded = bitStream->Read( internalPacket->splitPacketId );
#ifdef _DEBUG
		// 10/08/05 - Disabled assert since this hits from offline packets
		// assert( bitStreamSucceeded );
#endif

		if ( bitStreamSucceeded == false )
		{
			internalPacketPool.Release( internalPacket );
			return 0;
		}

		bitStreamSucceeded = bitStream->ReadCompressed( internalPacket->splitPacketIndex );
#ifdef _DEBUG
		// 10/08/05 - Disabled assert since this hits from offline packets
		//assert( bitStreamSucceeded );
#endif

		if ( bitStreamSucceeded == false )
		{
			internalPacketPool.Release( internalPacket );
			return 0;
		}

		bitStreamSucceeded = bitStream->ReadCompressed( internalPacket->splitPacketCount );
#ifdef _DEBUG
		// 10/08/05 - Disabled assert since this hits from offline packets
		//assert( bitStreamSucceeded );
#endif

		if ( bitStreamSucceeded == false )
		{
			internalPacketPool.Release( internalPacket );
			return 0;
		}
	}

	else
		internalPacket->splitPacketIndex = internalPacket->splitPacketCount = 0;

	// Optimization - do byte alignment here
	//unsigned char zero;
	//bitStream->ReadBits(&zero, 8 - (bitStream->GetNumberOfBitsUsed() %8));
	//assert(zero==0);


	unsigned short length;

	bitStreamSucceeded = bitStream->ReadCompressed( length );

	// Read into an unsigned short.  Otherwise the data would be offset too high by two bytes
#ifdef _DEBUG
	// 10/08/05 - Disabled assert since this hits from offline packets
	//assert( bitStreamSucceeded );
#endif

	if ( bitStreamSucceeded == false )
	{
		internalPacketPool.Release( internalPacket );
		return 0;
	}

	internalPacket->dataBitLength = length;
#ifdef _DEBUG
	// 10/08/05 - Disabled assert since this hits from offline packets arriving when the sender does not know we just connected, which is an unavoidable condition sometimes
	//	assert( internalPacket->dataBitLength > 0 && BITS_TO_BYTES( internalPacket->dataBitLength ) < MAXIMUM_MTU_SIZE );
#endif
	if ( ! ( internalPacket->dataBitLength > 0 && BITS_TO_BYTES( internalPacket->dataBitLength ) < MAXIMUM_MTU_SIZE ) )
	{
		// 10/08/05 - internalPacket->data wasn't allocated yet
		//	delete [] internalPacket->data;
		internalPacketPool.Release( internalPacket );
		return 0;
	}

	// Allocate memory to hold our data
	internalPacket->data = (unsigned char*) rakMalloc( (size_t) BITS_TO_BYTES( internalPacket->dataBitLength ) );
	//printf("Allocating %i\n",  internalPacket->data);

	// Set the last byte to 0 so if ReadBits does not read a multiple of 8 the last bits are 0'ed out
	internalPacket->data[ BITS_TO_BYTES( internalPacket->dataBitLength ) - 1 ] = 0;

	// Read the data the packet holds
	bitStreamSucceeded = bitStream->ReadAlignedBytes( ( unsigned char* ) internalPacket->data, BITS_TO_BYTES( internalPacket->dataBitLength ) );

	//bitStreamSucceeded = bitStream->ReadBits((unsigned char*)internalPacket->data, internalPacket->dataBitLength);
#ifdef _DEBUG

	// 10/08/05 - Disabled assert since this hits from offline packets
	//assert( bitStreamSucceeded );
#endif

	if ( bitStreamSucceeded == false )
	{
		rakFree(internalPacket->data);
		internalPacketPool.Release( internalPacket );
		return 0;
	}


	// PRINTING UNRELIABLE STRINGS
	// if (internalPacket->data && internalPacket->dataBitLength>5*8)
	//  printf("Received %s\n",internalPacket->data);

	return internalPacket;
}

//-------------------------------------------------------------------------------------------------------
// Get the SHA1 code
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::GetSHA1( unsigned char * const buffer, unsigned int
							   nbytes, char code[ SHA1_LENGTH ] )
{
	CSHA1 sha1;

	sha1.Reset();
	sha1.Update( ( unsigned char* ) buffer, nbytes );
	sha1.Final();
	memcpy( code, sha1.GetHash(), SHA1_LENGTH );
}

//-------------------------------------------------------------------------------------------------------
// Check the SHA1 code
//-------------------------------------------------------------------------------------------------------
bool ReliabilityLayer::CheckSHA1( char code[ SHA1_LENGTH ], unsigned char *
								 const buffer, unsigned int nbytes )
{
	char code2[ SHA1_LENGTH ];
	GetSHA1( buffer, nbytes, code2 );

	for ( int i = 0; i < SHA1_LENGTH; i++ )
		if ( code[ i ] != code2[ i ] )
			return false;

	return true;
}

//-------------------------------------------------------------------------------------------------------
// Search the specified list for sequenced packets on the specified ordering
// stream, optionally skipping those with splitPacketId, and delete them
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::DeleteSequencedPacketsInList( unsigned char orderingChannel, DataStructures::List<InternalPacket*>&theList, int splitPacketId )
{
	unsigned i = 0;

	while ( i < theList.Size() )
	{
		if ( ( theList[ i ]->reliability == RELIABLE_SEQUENCED || theList[ i ]->reliability == UNRELIABLE_SEQUENCED ) &&
			theList[ i ]->orderingChannel == orderingChannel && ( splitPacketId == -1 || theList[ i ]->splitPacketId != (unsigned int) splitPacketId ) )
		{
			InternalPacket * internalPacket = theList[ i ];
			theList.RemoveAtIndex( i );
			rakFree(internalPacket->data);
			internalPacketPool.Release( internalPacket );
		}

		else
			i++;
	}
}

//-------------------------------------------------------------------------------------------------------
// Search the specified list for sequenced packets with a value less than orderingIndex and delete them
// Note - I added functionality so you can use the Queue as a list (in this case for searching) but it is less efficient to do so than a regular list
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::DeleteSequencedPacketsInList( unsigned char orderingChannel, DataStructures::Queue<InternalPacket*>&theList )
{
	InternalPacket * internalPacket;
	int listSize = theList.Size();
	int i = 0;

	while ( i < listSize )
	{
		if ( ( theList[ i ]->reliability == RELIABLE_SEQUENCED || theList[ i ]->reliability == UNRELIABLE_SEQUENCED ) && theList[ i ]->orderingChannel == orderingChannel )
		{
			internalPacket = theList[ i ];
			theList.RemoveAtIndex( i );
			rakFree(internalPacket->data);
			internalPacketPool.Release( internalPacket );
			listSize--;
		}

		else
			i++;
	}
}

//-------------------------------------------------------------------------------------------------------
// Returns true if newPacketOrderingIndex is older than the waitingForPacketOrderingIndex
//-------------------------------------------------------------------------------------------------------
bool ReliabilityLayer::IsOlderOrderedPacket( OrderingIndexType newPacketOrderingIndex, OrderingIndexType waitingForPacketOrderingIndex )
{
	OrderingIndexType maxRange = (OrderingIndexType) -1;

	if ( waitingForPacketOrderingIndex > maxRange/2 )
	{
		if ( newPacketOrderingIndex >= waitingForPacketOrderingIndex - maxRange/2+1 && newPacketOrderingIndex < waitingForPacketOrderingIndex )
		{
			return true;
		}
	}

	else
		if ( newPacketOrderingIndex >= ( OrderingIndexType ) ( waitingForPacketOrderingIndex - (( OrderingIndexType ) maxRange/2+1) ) ||
			newPacketOrderingIndex < waitingForPacketOrderingIndex )
		{
			return true;
		}

		// Old packet
		return false;
}

//-------------------------------------------------------------------------------------------------------
// Split the passed packet into chunks under MTU_SIZEbytes (including headers) and save those new chunks
// Optimized version
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::SplitPacket( InternalPacket *internalPacket, int MTUSize )
{
	// Doing all sizes in bytes in this function so I don't write partial bytes with split packets
	internalPacket->splitPacketCount = 1; // This causes GetBitStreamHeaderLength to account for the split packet header
	unsigned int headerLength = (unsigned int) BITS_TO_BYTES( GetBitStreamHeaderLength( internalPacket ) );
	unsigned int dataByteLength = (unsigned int) BITS_TO_BYTES( internalPacket->dataBitLength );
	int maxDataSize;
	int maximumSendBlock, byteOffset, bytesToSend;
	SplitPacketIndexType splitPacketIndex;
	int i;
	InternalPacket **internalPacketArray;

	maxDataSize = MTUSize - UDP_HEADER_SIZE;

	if ( encryptor.IsKeySet() )
		maxDataSize -= 16; // Extra data for the encryptor

#ifdef _DEBUG
	// Make sure we need to split the packet to begin with
	assert( dataByteLength > maxDataSize - headerLength );
#endif

	// How much to send in the largest block
	maximumSendBlock = maxDataSize - headerLength;

	// Calculate how many packets we need to create
	internalPacket->splitPacketCount = ( ( dataByteLength - 1 ) / ( maximumSendBlock ) + 1 );

	statistics.totalSplits += internalPacket->splitPacketCount;

	// Optimization
	// internalPacketArray = new InternalPacket*[internalPacket->splitPacketCount];
	bool usedAlloca=false;
#if !defined(_XBOX360)
	if (sizeof( InternalPacket* ) * internalPacket->splitPacketCount < MAX_ALLOCA_STACK_ALLOCATION)
	{
		internalPacketArray = ( InternalPacket** ) alloca( sizeof( InternalPacket* ) * internalPacket->splitPacketCount );
		usedAlloca=true;
	}
	else
#endif
		internalPacketArray = (InternalPacket**) rakMalloc( sizeof(InternalPacket*) * internalPacket->splitPacketCount );

	for ( i = 0; i < ( int ) internalPacket->splitPacketCount; i++ )
	{
		internalPacketArray[ i ] = internalPacketPool.Allocate();
		//internalPacketArray[ i ] = (InternalPacket*) alloca( sizeof( InternalPacket ) );
		//		internalPacketArray[ i ] = sendPacketSet[internalPacket->priority].WriteLock();
		memcpy( internalPacketArray[ i ], internalPacket, sizeof( InternalPacket ) );
	}

	// This identifies which packet this is in the set
	splitPacketIndex = 0;

	// Do a loop to send out all the packets
	do
	{
		byteOffset = splitPacketIndex * maximumSendBlock;
		bytesToSend = dataByteLength - byteOffset;

		if ( bytesToSend > maximumSendBlock )
			bytesToSend = maximumSendBlock;

		// Copy over our chunk of data
		internalPacketArray[ splitPacketIndex ]->data = (unsigned char*) rakMalloc( bytesToSend );

		memcpy( internalPacketArray[ splitPacketIndex ]->data, internalPacket->data + byteOffset, bytesToSend );

		if ( bytesToSend != maximumSendBlock )
			internalPacketArray[ splitPacketIndex ]->dataBitLength = internalPacket->dataBitLength - splitPacketIndex * ( maximumSendBlock << 3 );
		else
			internalPacketArray[ splitPacketIndex ]->dataBitLength = bytesToSend << 3;

		internalPacketArray[ splitPacketIndex ]->splitPacketIndex = splitPacketIndex;
		internalPacketArray[ splitPacketIndex ]->splitPacketId = splitPacketId;
		internalPacketArray[ splitPacketIndex ]->splitPacketCount = internalPacket->splitPacketCount;

		if ( splitPacketIndex > 0 )   // For the first split packet index we keep the messageNumber already assigned
		{
			// For every further packet we use a new messageNumber.
			// Note that all split packets are reliable
		//	internalPacketArray[ splitPacketIndex ]->messageNumber = sendMessageNumberIndex;
			internalPacketArray[ splitPacketIndex ]->messageNumber = (MessageNumberType) -1;
			internalPacketArray[ splitPacketIndex ]->messageNumberAssigned=false;

			//if ( ++messageNumber == RECEIVED_PACKET_LOG_LENGTH )
			//	messageNumber = 0;
		//	++sendMessageNumberIndex;

		}

		// Add the new packet to send list at the correct priority
		//  sendQueue[internalPacket->priority].Insert(newInternalPacket);
		// SHOW SPLIT PACKET GENERATION
		// if (splitPacketIndex % 100 == 0)
		//  printf("splitPacketIndex=%i\n",splitPacketIndex);
		//} while(++splitPacketIndex < internalPacket->splitPacketCount);
	}

	while ( ++splitPacketIndex < internalPacket->splitPacketCount );

	splitPacketId++; // It's ok if this wraps to 0

	//	InternalPacket *workingPacket;

	// Copy all the new packets into the split packet list
	for ( i = 0; i < ( int ) internalPacket->splitPacketCount; i++ )
	{
		sendPacketSet[ internalPacket->priority ].Push( internalPacketArray[ i ] );
		//		workingPacket=sendPacketSet[internalPacket->priority].WriteLock();
		//		memcpy(workingPacket, internalPacketArray[ i ], sizeof(InternalPacket));
		//		sendPacketSet[internalPacket->priority].WriteUnlock();
	}

	// Delete the original
	rakFree(internalPacket->data);
	internalPacketPool.Release( internalPacket );

	if (usedAlloca==false)
		rakFree(internalPacketArray);
}

//-------------------------------------------------------------------------------------------------------
// Insert a packet into the split packet list
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::InsertIntoSplitPacketList( InternalPacket * internalPacket, RakNetTimeNS time )
{
	bool objectExists;
	unsigned index;
	index=splitPacketChannelList.GetIndexFromKey(internalPacket->splitPacketId, &objectExists);
	if (objectExists==false)
	{
		SplitPacketChannel *newChannel = new SplitPacketChannel;
		index=splitPacketChannelList.Insert(internalPacket->splitPacketId, newChannel, true);
	}
	splitPacketChannelList[index]->splitPacketList.Insert(internalPacket->splitPacketIndex, internalPacket, true);
	splitPacketChannelList[index]->lastUpdateTime=time;

	if (splitMessageProgressInterval &&
		splitPacketChannelList[index]->splitPacketList[0]->splitPacketIndex==0 &&
		splitPacketChannelList[index]->splitPacketList.Size()!=splitPacketChannelList[index]->splitPacketList[0]->splitPacketCount &&
		(splitPacketChannelList[index]->splitPacketList.Size()%splitMessageProgressInterval)==0)
	{
		//		printf("msgID=%i Progress %i/%i Partsize=%i\n",
		//			splitPacketChannelList[index]->splitPacketList[0]->data[0],
		//			splitPacketChannelList[index]->splitPacketList.Size(),
		//			internalPacket->splitPacketCount,
		//			BITS_TO_BYTES(splitPacketChannelList[index]->splitPacketList[0]->dataBitLength));

		// Return ID_DOWNLOAD_PROGRESS
		// Write splitPacketIndex (SplitPacketIndexType)
		// Write splitPacketCount (SplitPacketIndexType)
		// Write byteLength (4)
		// Write data, splitPacketChannelList[index]->splitPacketList[0]->data
		InternalPacket *progressIndicator = internalPacketPool.Allocate();
		unsigned int length = sizeof(MessageID) + sizeof(unsigned int)*2 + sizeof(unsigned int) + (unsigned int) BITS_TO_BYTES(splitPacketChannelList[index]->splitPacketList[0]->dataBitLength);
		progressIndicator->data = (unsigned char*) rakMalloc( length );
		progressIndicator->dataBitLength=BYTES_TO_BITS(length);
		progressIndicator->data[0]=(MessageID)ID_DOWNLOAD_PROGRESS;
		unsigned int temp;
		temp=splitPacketChannelList[index]->splitPacketList.Size();
		memcpy(progressIndicator->data+sizeof(MessageID), &temp, sizeof(unsigned int));
		temp=(unsigned int)internalPacket->splitPacketCount;
		memcpy(progressIndicator->data+sizeof(MessageID)+sizeof(unsigned int)*1, &temp, sizeof(unsigned int));
		temp=(unsigned int) BITS_TO_BYTES(splitPacketChannelList[index]->splitPacketList[0]->dataBitLength);
		memcpy(progressIndicator->data+sizeof(MessageID)+sizeof(unsigned int)*2, &temp, sizeof(unsigned int));
		memcpy(progressIndicator->data+sizeof(MessageID)+sizeof(unsigned int)*3, splitPacketChannelList[index]->splitPacketList[0]->data, (size_t) BITS_TO_BYTES(splitPacketChannelList[index]->splitPacketList[0]->dataBitLength));
		outputQueue.Push(progressIndicator);
	}
}

//-------------------------------------------------------------------------------------------------------
// Take all split chunks with the specified splitPacketId and try to
//reconstruct a packet.  If we can, allocate and return it.  Otherwise return 0
// Optimized version
//-------------------------------------------------------------------------------------------------------
InternalPacket * ReliabilityLayer::BuildPacketFromSplitPacketList( SplitPacketIdType splitPacketId, RakNetTimeNS time )
{
	unsigned i, j;
	unsigned byteProgress;
	InternalPacket * internalPacket;
	bool objectExists;

	i=splitPacketChannelList.GetIndexFromKey(splitPacketId, &objectExists);
#ifdef _DEBUG
	assert(objectExists);
#endif

	if (splitPacketChannelList[i]->splitPacketList.Size()==splitPacketChannelList[i]->splitPacketList[0]->splitPacketCount)
	{
		// Reconstruct
		internalPacket = CreateInternalPacketCopy( splitPacketChannelList[i]->splitPacketList[0], 0, 0, time );
		internalPacket->dataBitLength=0;
		for (j=0; j < splitPacketChannelList[i]->splitPacketList.Size(); j++)
			internalPacket->dataBitLength+=splitPacketChannelList[i]->splitPacketList[j]->dataBitLength;

		internalPacket->data = (unsigned char*) rakMalloc( (size_t) BITS_TO_BYTES( internalPacket->dataBitLength ) );

		byteProgress=0;
		for (j=0; j < splitPacketChannelList[i]->splitPacketList.Size(); j++)
		{
			memcpy(internalPacket->data+byteProgress, splitPacketChannelList[i]->splitPacketList[j]->data, (size_t) BITS_TO_BYTES(splitPacketChannelList[i]->splitPacketList[j]->dataBitLength));
			byteProgress+=(unsigned int) BITS_TO_BYTES(splitPacketChannelList[i]->splitPacketList[j]->dataBitLength);
		}

		for (j=0; j < splitPacketChannelList[i]->splitPacketList.Size(); j++)
		{
			rakFree(splitPacketChannelList[i]->splitPacketList[j]->data);
			internalPacketPool.Release(splitPacketChannelList[i]->splitPacketList[j]);
		}
		delete splitPacketChannelList[i];
		splitPacketChannelList.RemoveAtIndex(i);

		return internalPacket;
	}

	return 0;
}

// Delete any unreliable split packets that have long since expired
void ReliabilityLayer::DeleteOldUnreliableSplitPackets( RakNetTimeNS time )
{
	unsigned i,j;
	i=0;
	while (i < splitPacketChannelList.Size())
	{
		if (time > splitPacketChannelList[i]->lastUpdateTime + (RakNetTimeNS)timeoutTime*(RakNetTimeNS)1000 &&
			(splitPacketChannelList[i]->splitPacketList[0]->reliability==UNRELIABLE || splitPacketChannelList[i]->splitPacketList[0]->reliability==UNRELIABLE_SEQUENCED))
		{
			for (j=0; j < splitPacketChannelList[i]->splitPacketList.Size(); j++)
			{
				delete [] splitPacketChannelList[i]->splitPacketList[j]->data;
				internalPacketPool.Release(splitPacketChannelList[i]->splitPacketList[j]);
			}
			delete splitPacketChannelList[i];
			splitPacketChannelList.RemoveAtIndex(i);

#if defined(DEBUG_SPLIT_PACKET_PROBLEMS)
			printf("DeleteOldUnreliableSplitPackets called, split packet will never complete\n");
#endif
		}
		else
			i++;
	}
}

//-------------------------------------------------------------------------------------------------------
// Creates a copy of the specified internal packet with data copied from the original starting at dataByteOffset for dataByteLength bytes.
// Does not copy any split data parameters as that information is always generated does not have any reason to be copied
//-------------------------------------------------------------------------------------------------------
InternalPacket * ReliabilityLayer::CreateInternalPacketCopy( InternalPacket *original, int dataByteOffset, int dataByteLength, RakNetTimeNS time )
{
	InternalPacket * copy = internalPacketPool.Allocate();
#ifdef _DEBUG
	// Remove accessing undefined memory error
	memset( copy, 255, sizeof( InternalPacket ) );
#endif
	// Copy over our chunk of data

	if ( dataByteLength > 0 )
	{
		copy->data = (unsigned char*) rakMalloc( BITS_TO_BYTES(dataByteLength ) );
		memcpy( copy->data, original->data + dataByteOffset, dataByteLength );
	}
	else
		copy->data = 0;

	copy->dataBitLength = dataByteLength << 3;
	copy->creationTime = time;
	copy->nextActionTime = 0;
	copy->orderingIndex = original->orderingIndex;
	copy->orderingChannel = original->orderingChannel;
	copy->messageNumber = original->messageNumber;
	copy->priority = original->priority;
	copy->reliability = original->reliability;

	// REMOVEME
//	if (copy->messageNumber > 30000)
//	{
//		int a=5;
//	}

	return copy;
}

//-------------------------------------------------------------------------------------------------------
// Get the specified ordering list
//-------------------------------------------------------------------------------------------------------
DataStructures::LinkedList<InternalPacket*> *ReliabilityLayer::GetOrderingListAtOrderingStream( unsigned char orderingChannel )
{
	if ( orderingChannel >= orderingList.Size() )
		return 0;

	return orderingList[ orderingChannel ];
}

//-------------------------------------------------------------------------------------------------------
// Add the internal packet to the ordering list in order based on order index
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::AddToOrderingList( InternalPacket * internalPacket )
{
#ifdef _DEBUG
	assert( internalPacket->orderingChannel < NUMBER_OF_ORDERED_STREAMS );
#endif

	if ( internalPacket->orderingChannel >= NUMBER_OF_ORDERED_STREAMS )
	{
		return;
	}

	DataStructures::LinkedList<InternalPacket*> *theList;

	if ( internalPacket->orderingChannel >= orderingList.Size() || orderingList[ internalPacket->orderingChannel ] == 0 )
	{
		// Need a linked list in this index
		orderingList.Replace( new DataStructures::LinkedList<InternalPacket*>, 0, internalPacket->orderingChannel );
		theList=orderingList[ internalPacket->orderingChannel ];
	}
	else
	{
		// Have a linked list in this index
		if ( orderingList[ internalPacket->orderingChannel ]->Size() == 0 )
		{
			theList=orderingList[ internalPacket->orderingChannel ];
		}
		else
		{
			theList = GetOrderingListAtOrderingStream( internalPacket->orderingChannel );
		}
	}

	theList->End();
	theList->Add(internalPacket);
}

//-------------------------------------------------------------------------------------------------------
// Inserts a packet into the resend list in order
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::InsertPacketIntoResendList( InternalPacket *internalPacket, RakNetTimeNS time, bool makeCopyOfInternalPacket, bool firstResend )
{
	// lastAckTime is the time we last got an acknowledgment - however we also initialize the value if this is the first resend and
	// either we never got an ack before or we are inserting into an empty resend queue
	if ( firstResend && (lastAckTime == 0 || resendList.IsEmpty()))
	{
		lastAckTime = time; // Start the timer for the ack of this packet if we aren't already waiting for an ack
	}

	if (makeCopyOfInternalPacket)
	{
		InternalPacket *pool=internalPacketPool.Allocate();
		//printf("Adding %i\n", internalPacket->data);
		memcpy(pool, internalPacket, sizeof(InternalPacket));
		resendQueue.Push( pool );
	}
	else
	{
		resendQueue.Push( internalPacket );
	}

	RakAssert(internalPacket->nextActionTime!=0);

}

//-------------------------------------------------------------------------------------------------------
// If Read returns -1 and this returns true then a modified packet was detected
//-------------------------------------------------------------------------------------------------------
bool ReliabilityLayer::IsCheater( void ) const
{
	return cheater;
}

//-------------------------------------------------------------------------------------------------------
//  Were you ever unable to deliver a packet despite retries?
//-------------------------------------------------------------------------------------------------------
bool ReliabilityLayer::IsDeadConnection( void ) const
{
	return deadConnection;
}

//-------------------------------------------------------------------------------------------------------
//  Causes IsDeadConnection to return true
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::KillConnection( void )
{
	deadConnection=true;
}


//-------------------------------------------------------------------------------------------------------
// Statistics
//-------------------------------------------------------------------------------------------------------
RakNetStatistics * const ReliabilityLayer::GetStatistics( void )
{
	unsigned i;

	for ( i = 0; i < NUMBER_OF_PRIORITIES; i++ )
	{
		statistics.messageSendBuffer[i] = sendPacketSet[i].Size();
	}

	statistics.acknowlegementsPending = acknowlegements.Size();
	if (lastUpdateTime>histogramStart && histogramBitsSent >0)
		statistics.bitsPerSecond=(double)((long double)histogramBitsSent / ((long double) (lastUpdateTime-histogramStart) / 1000000.0f));
	else
		statistics.bitsPerSecond=0.0;
	statistics.messagesWaitingForReassembly = 0;
	for (i=0; i < splitPacketChannelList.Size(); i++)
		statistics.messagesWaitingForReassembly+=splitPacketChannelList[i]->splitPacketList.Size();
	statistics.internalOutputQueueSize = outputQueue.Size();
	//statistics.lossySize = lossyWindowSize == MAXIMUM_WINDOW_SIZE + 1 ? 0 : lossyWindowSize;
	//	statistics.lossySize=0;
	// The connection is full if we are continuously sending data and we had to throttle back recently.
	statistics.bandwidthExceeded = continuousSend && (lastUpdateTime-lastTimeBetweenPacketsIncrease) > (RakNetTimeNS) 1000000;
	statistics.messagesOnResendQueue = GetResendListDataSize();

	return &statistics;
}

//-------------------------------------------------------------------------------------------------------
// Returns the number of packets in the resend queue, not counting holes
//-------------------------------------------------------------------------------------------------------
unsigned int ReliabilityLayer::GetResendListDataSize(void) const
{
	/*
	unsigned int i, count;
	for (count=0, i=0; i < resendList.Size(); i++)
	if (resendList[i]!=0)
	count++;
	return count;
	*/

	// Not accurate but thread-safe.  The commented version might crash if the queue is cleared while we loop through it
	return resendList.Size();
}

//-------------------------------------------------------------------------------------------------------
// Process threaded commands
//-------------------------------------------------------------------------------------------------------
void ReliabilityLayer::UpdateThreadedMemory(void)
{
	if ( freeThreadedMemoryOnNextUpdate )
	{
		freeThreadedMemoryOnNextUpdate = false;
		FreeThreadedMemory();
	}
}
//-------------------------------------------------------------------------------------------------------
bool ReliabilityLayer::AckTimeout(RakNetTimeNS curTime)
{
	return curTime > lastAckTime && lastAckTime && curTime - lastAckTime > (RakNetTimeNS)timeoutTime*1000;
}
//-------------------------------------------------------------------------------------------------------
RakNetTimeNS ReliabilityLayer::GetNextSendTime(void) const
{
	return nextSendTime;
}
//-------------------------------------------------------------------------------------------------------
RakNetTimeNS ReliabilityLayer::GetTimeBetweenPackets(void) const
{
	return timeBetweenPackets;
}
//-------------------------------------------------------------------------------------------------------
RakNetTimeNS ReliabilityLayer::GetLastTimeBetweenPacketsDecrease(void) const
{
	return lastTimeBetweenPacketsDecrease;
}
//-------------------------------------------------------------------------------------------------------
RakNetTimeNS ReliabilityLayer::GetLastTimeBetweenPacketsIncrease(void) const
{
	return lastTimeBetweenPacketsIncrease;
}
//-------------------------------------------------------------------------------------------------------
RakNetTimeNS ReliabilityLayer::GetAckPing(void) const
{
	return ackPing;
}
//-------------------------------------------------------------------------------------------------------
/*
int InternalPacketCompByInternalOrderIndex( InternalPacket * const &key, InternalPacket * const &data )
{
	int64_t holeCount = (key->messageInternalOrder-data->messageInternalOrder);
	int64_t absHoleCount = holeCount>0 ? holeCount : -holeCount;
	MessageNumberType typeRange = (MessageNumberType)-1;
	if (absHoleCount > typeRange/2)
	{
		if (holeCount>0)
			return -1;
		else if (holeCount<0)
			return 1;
	}
	else
	{
		if (holeCount>0)
			return 1;
		else if (holeCount<0)
			return -1;
	}
	if (key->splitPacketIndex>data->splitPacketIndex)
		return 1;
	if (key->splitPacketIndex<data->splitPacketIndex)
		return -1;
	return 0;
}
void ReliabilityLayer::GetUndeliveredMessages(RakNet::BitStream *messages, int MTUSize)
{
	DataStructures::OrderedList<InternalPacket*, InternalPacket*, InternalPacketCompByInternalOrderIndex> undeliveredMessages;
	unsigned int i,j,k;
	for (i=0; i < resendQueue.Size(); i++)
	{
		if (resendQueue[i]->nextActionTime==0)
			continue;
		undeliveredMessages.Insert(resendQueue[i],resendQueue[i],true);
	}

	for ( i = 0; i < NUMBER_OF_PRIORITIES; i++ )
	{
		for (j=0; j < sendPacketSet[ i ].Size(); j++)
			undeliveredMessages.Insert( sendPacketSet[ i ][ j ],sendPacketSet[ i ][ j ],true);
	}

	unsigned char zeroBlock[MAXIMUM_MTU_SIZE];
	memset(zeroBlock,0,MAXIMUM_MTU_SIZE);
	unsigned int messagesConcatenated=0;
	uint64_t totalBitLength;
	BitSize_t oldWriteOffset;
	unsigned int lastSplitPacketIndex, lastSplitPacketId;
	messages->Reset();
	messages->Write((unsigned int) undeliveredMessages.Size());
	i=0;
	while (i < undeliveredMessages.Size())
	{
		// Write message size
		if (undeliveredMessages[i]->splitPacketCount>0)
		{
			lastSplitPacketId=undeliveredMessages[i]->splitPacketId;
			lastSplitPacketIndex=0;
			j=i;
			totalBitLength=0;
			unsigned int headerLength = (unsigned int) BITS_TO_BYTES( GetBitStreamHeaderLength( undeliveredMessages[i] ) );
			unsigned int maxDataSize = MTUSize - UDP_HEADER_SIZE;
			// How much to send in the largest block
			unsigned int maximumSendBlock = maxDataSize - headerLength;
			// Calculate the total bit length of all the messages, using the split MTU if not known
			while (j < undeliveredMessages.Size() && lastSplitPacketId==undeliveredMessages[j]->splitPacketId && undeliveredMessages[j]->splitPacketCount>0)
			{
				for (k=lastSplitPacketIndex; k < undeliveredMessages[j]->splitPacketIndex; k++)
					totalBitLength+=BYTES_TO_BITS(maximumSendBlock);
				totalBitLength+=undeliveredMessages[j]->dataBitLength;
				lastSplitPacketIndex=undeliveredMessages[j]->splitPacketIndex+1;
				j++;
			}
			messages->Write((unsigned int) BITS_TO_BYTES(totalBitLength));
			while (i < undeliveredMessages.Size() && lastSplitPacketId==undeliveredMessages[i]->splitPacketId && undeliveredMessages[i]->splitPacketCount>0)
			{
				// Write blocks of 0 for unknown data
				for (j=lastSplitPacketIndex; j < undeliveredMessages[i]->splitPacketIndex; j++)
					messages->WriteAlignedBytes(zeroBlock,BITS_TO_BYTES(undeliveredMessages[i]->dataBitLength));
				// Write the data
				messages->WriteAlignedBytes(undeliveredMessages[i]->data, BITS_TO_BYTES(undeliveredMessages[i]->dataBitLength));
				lastSplitPacketIndex=undeliveredMessages[i]->splitPacketIndex+1;
				messagesConcatenated++;
				i++;
			}
		}
		else
		{
			messages->Write((unsigned int) BITS_TO_BYTES(undeliveredMessages[i]->dataBitLength));
			// Write the data
			messages->WriteAlignedBytes(undeliveredMessages[i]->data, BITS_TO_BYTES(undeliveredMessages[i]->dataBitLength));
			i++;
		}
	}


	oldWriteOffset=messages->GetWriteOffset();
	messages->SetWriteOffset(0);
	messages->Write((unsigned int) undeliveredMessages.Size()-messagesConcatenated);
	messages->SetWriteOffset(oldWriteOffset);

}
*/

//-------------------------------------------------------------------------------------------------------
/*
void ReliabilityLayer::SetDoFastThroughputReactions(bool fast)
{
	if (fast)
		timeBetweenPacketsIncreaseMultiplier=TIME_BETWEEN_PACKETS_INCREASE_MULTIPLIER_FAST;
	else
		timeBetweenPacketsIncreaseMultiplier=TIME_BETWEEN_PACKETS_INCREASE_MULTIPLIER_DEFAULT;

}
*/
//-------------------------------------------------------------------------------------------------------
#ifdef _MSC_VER
#pragma warning( pop )
#endif
