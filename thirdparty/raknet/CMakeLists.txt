project(RakNetStaticLib)

SET(RAKNET_SOURCES 

Source/AsynchronousFileIO.cpp      Source/FormatString.cpp               Source/RakNetTypes.cpp
Source/AutoRPC.cpp                 Source/FullyConnectedMesh.cpp         Source/RakNetworkFactory.cpp
Source/BitStream.cpp               Source/FunctionThread.cpp             Source/RakPeer.cpp
Source/BitStream_NoTemplate.cpp    Source/Gen_RPC8.cpp                   Source/RakSleep.cpp
Source/CheckSum.cpp                Source/GetTime.cpp                    Source/RakString.cpp
Source/CommandParserInterface.cpp  Source/GridSectorizer.cpp             Source/Rand.cpp
Source/ConnectionGraph.cpp         Source/LightweightDatabaseClient.cpp  Source/ReadyEvent.cpp
Source/ConsoleServer.cpp           Source/LightweightDatabaseCommon.cpp  Source/ReliabilityLayer.cpp
Source/DataBlockEncryptor.cpp      Source/LightweightDatabaseServer.cpp  Source/ReplicaManager2.cpp
Source/DataCompressor.cpp          Source/LinuxStrings.cpp               Source/ReplicaManager.cpp
Source/DirectoryDeltaTransfer.cpp  Source/LogCommandParser.cpp           Source/rijndael.cpp
Source/DS_BytePool.cpp             Source/MessageFilter.cpp              Source/Router.cpp
Source/DS_ByteQueue.cpp            Source/NatPunchthrough.cpp            Source/RPCMap.cpp
Source/DS_HuffmanEncodingTree.cpp  Source/NetworkIDManager.cpp           Source/SHA1.cpp
Source/DS_Table.cpp                Source/NetworkIDObject.cpp            Source/SimpleMutex.cpp
Source/EmailSender.cpp             Source/PacketConsoleLogger.cpp        Source/SocketLayer.cpp
Source/EncodeClassName.cpp         Source/PacketFileLogger.cpp           Source/StringCompressor.cpp
Source/EpochTimeToString.cpp       Source/PacketLogger.cpp               Source/StringTable.cpp
Source/ExtendedOverlappedPool.cpp  Source/PluginInterface.cpp            Source/SystemAddressList.cpp
Source/FileList.cpp                Source/RakMemoryOverride.cpp          Source/TableSerializer.cpp
Source/FileListTransfer.cpp        Source/RakNetCommandParser.cpp        Source/TCPInterface.cpp
Source/FileOperations.cpp          Source/RakNetStatistics.cpp           Source/TelnetTransport.cpp
Source/_FindFirst.cpp              Source/RakNetTransport.cpp            Source/ThreadsafePacketLogger.cpp
Source/RakThread.cpp               Source/SuperFastHash.cpp              Source/Itoa.cpp
Source/HTTPConnection.cpp
)

set(RAKNET_HEADERS

Source/AsynchronousFileIO.h              Source/Export.h                       Source/RakNetTypes.h
Source/AutopatcherPatchContext.h         Source/ExtendedOverlappedPool.h       Source/RakNetVersion.h
Source/AutopatcherRepositoryInterface.h  Source/FileList.h                     Source/RakNetworkFactory.h
Source/AutoRPC.h                         Source/FileListTransferCBInterface.h  Source/RakPeer.h
Source/BigTypes.h                        Source/FileListTransfer.h             Source/RakPeerInterface.h
Source/BitStream.h                       Source/FileOperations.h               Source/RakSleep.h
Source/BitStream_NoTemplate.h            Source/_FindFirst.h                   Source/RakString.h
Source/CheckSum.h                        Source/FormatString.h                 Source/Rand.h
Source/ClientContextStruct.h             Source/FullyConnectedMesh.h           Source/ReadyEvent.h
Source/CommandParserInterface.h          Source/FunctionThread.h               Source/RefCountedObj.h
Source/ConnectionGraph.h                 Source/Gen_RPC8.h                     Source/ReliabilityLayer.h
Source/ConsoleServer.h                   Source/GetTime.h                      Source/ReplicaEnums.h
Source/DataBlockEncryptor.h              Source/GridSectorizer.h               Source/Replica.h
Source/DataCompressor.h                  Source/InternalPacket.h               Source/ReplicaManager2.h
Source/DirectoryDeltaTransfer.h          Source/LightweightDatabaseClient.h    Source/ReplicaManager.h
Source/DS_BinarySearchTree.h             Source/LightweightDatabaseCommon.h    Source/Rijndael-Boxes.h
Source/DS_BPlusTree.h                    Source/LightweightDatabaseServer.h    Source/Rijndael.h
Source/DS_BytePool.h                     Source/LinuxStrings.h                 Source/Router.h
Source/DS_ByteQueue.h                    Source/LogCommandParser.h             Source/RouterInterface.h
Source/DS_Heap.h                         Source/MessageFilter.h                Source/RPCMap.h
Source/DS_HuffmanEncodingTreeFactory.h   Source/MessageIdentifiers.h           Source/RPCNode.h
Source/DS_HuffmanEncodingTree.h          Source/MTUSize.h                      Source/RSACrypt.h
Source/DS_HuffmanEncodingTreeNode.h      Source/NatPunchthrough.h              Source/SHA1.h
Source/DS_LinkedList.h                   Source/NetworkIDManager.h             Source/SimpleMutex.h
Source/DS_List.h                         Source/NetworkIDObject.h              Source/SimpleTCPServer.h
Source/DS_Map.h                          Source/PacketConsoleLogger.h          Source/SingleProducerConsumer.h
Source/DS_MemoryPool.h                   Source/PacketFileLogger.h             Source/SocketLayer.h
Source/DS_OrderedChannelHeap.h           Source/PacketLogger.h                 Source/StringCompressor.h
Source/DS_OrderedList.h                  Source/PacketPool.h                   Source/StringTable.h
Source/DS_Queue.h                        Source/PacketPriority.h               Source/SystemAddressList.h
Source/DS_QueueLinkedList.h              Source/PluginInterface.h              Source/TableSerializer.h
Source/DS_RangeList.h                    Source/RakAssert.h                    Source/TCPInterface.h
Source/DS_Table.h                        Source/RakMemoryOverride.h            Source/TelnetTransport.h
Source/DS_Tree.h                         Source/RakNetCommandParser.h          Source/ThreadPool.h
Source/DS_WeightedGraph.h                Source/RakNetDefines.h                Source/ThreadsafePacketLogger.h
Source/EmailSender.h                     Source/RakNetStatistics.h             Source/TransportInterface.h
Source/EpochTimeToString.h               Source/RakNetTransport.h              Source/Types.h
Source/RakThread.h                       Source/SuperFastHash.h                Source/Itoa.h
Source/HTTPConnection.h                  Kbhit.h
)


add_library(raknet STATIC ${RAKNET_SOURCES})
target_compile_options(raknet PRIVATE
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -w>
     $<$<CXX_COMPILER_ID:MSVC>:
        /w>)

if(WIN32)
        # Link Win Sockets 2 to RakNet
	target_link_libraries(raknet ws2_32)
endif()

install(TARGETS raknet
        DESTINATION lib)

install(FILES ${RAKNET_HEADERS}
        DESTINATION include/raknet)
