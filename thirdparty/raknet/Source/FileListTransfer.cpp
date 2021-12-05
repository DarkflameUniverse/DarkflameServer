#include "FileListTransfer.h"
#include "DS_HuffmanEncodingTree.h"
#include "FileListTransferCBInterface.h"
#include "StringCompressor.h"
#include "FileList.h"
#include "DS_Queue.h"
#include "MessageIdentifiers.h"
#include "RakNetTypes.h"
#include "RakPeerInterface.h"
#include <assert.h>
#ifdef _PS3
#include <alloca.h>
#endif

#ifdef _MSC_VER
#pragma warning( push )
#endif

struct FileListTransfer::FileListReceiver
{
	FileListTransferCBInterface *downloadHandler;
	SystemAddress allowedSender;
    HuffmanEncodingTree tree;
	unsigned short setID;
	unsigned setCount;
	unsigned setTotalCompressedTransmissionLength;
	unsigned setTotalFinalLength;
	bool gotSetHeader;
	bool deleteDownloadHandler;
	bool isCompressed;
};

FileListTransfer::FileListTransfer()
{
	rakPeer=0;
	setId=0;
	callback=0;
	DataStructures::Map<unsigned short, FileListReceiver*>::IMPLEMENT_DEFAULT_COMPARISON();
}
FileListTransfer::~FileListTransfer()
{
	Clear();
}
unsigned short FileListTransfer::SetupReceive(FileListTransferCBInterface *handler, bool deleteHandler, SystemAddress allowedSender)
{
	if (rakPeer->IsConnected(allowedSender)==false)
		return (unsigned short)-1;
	FileListReceiver *receiver;

	if (fileListReceivers.Has(setId))
	{
		receiver=fileListReceivers.Get(setId);
		receiver->downloadHandler->OnDereference();
		if (receiver->deleteDownloadHandler)
			delete receiver->downloadHandler;
		delete receiver;
		fileListReceivers.Delete(setId);
	}

	unsigned short oldId;
	receiver = new FileListReceiver;
	receiver->downloadHandler=handler;
	receiver->allowedSender=allowedSender;
	receiver->gotSetHeader=false;
	receiver->deleteDownloadHandler=deleteHandler;
	fileListReceivers.Set(setId, receiver);
	oldId=setId;
	if (++setId==(unsigned short)-1)
		setId=0;
	return oldId;
}

void FileListTransfer::Send(FileList *fileList, RakPeerInterface *rakPeer, SystemAddress recipient, unsigned short setID, PacketPriority priority, char orderingChannel, bool compressData)
{
	RakNet::BitStream outBitstream, encodedData;
	HuffmanEncodingTree tree;
	unsigned int frequencyTable[ 256 ];
	unsigned int i,j;
	unsigned totalCompressedLength, totalLength;
	DataStructures::Queue<FileListNode> compressedFiles;
	FileListNode n;

	if (callback)
		fileList->SetCallback(callback);

	totalCompressedLength=totalLength=0;
	if (compressData)
	{
		memset(frequencyTable,0,256*sizeof(unsigned int));

		for (i=0; i < fileList->fileList.Size(); i++)
		{
			const FileListNode &fileListNode = fileList->fileList[i];
			for (j=0; j < fileListNode.dataLengthBytes; j++)
			{
				++frequencyTable[(unsigned char)(fileListNode.data[j])];
			}
		}

		tree.GenerateFromFrequencyTable(frequencyTable);

		// Compress all the files, so we know the total compressed size to be sent

		for (i=0; i < fileList->fileList.Size(); i++)
		{
			encodedData.Reset();
			// Why send compressed chunks if we are not sending the whole file?
			assert(fileList->fileList[i].fileLengthBytes==fileList->fileList[i].fileLengthBytes);
			tree.EncodeArray((unsigned char*)fileList->fileList[i].data, (size_t) fileList->fileList[i].dataLengthBytes, &encodedData);
			n.dataLengthBytes=encodedData.GetNumberOfBitsUsed(); // Temporarily actually storing bits
			totalCompressedLength+=(unsigned int) BITS_TO_BYTES(n.dataLengthBytes);
			totalLength+=fileList->fileList[i].fileLengthBytes;
			n.data = (char*) rakMalloc( (size_t) BITS_TO_BYTES(n.dataLengthBytes) );
			memcpy(n.data, encodedData.GetData(), (size_t) BITS_TO_BYTES(n.dataLengthBytes));
			compressedFiles.Push(n);
		}
	}
	else
	{
		for (i=0; i < fileList->fileList.Size(); i++)
		{
			const FileListNode &fileListNode = fileList->fileList[i];
			totalLength+=fileListNode.fileLengthBytes;
		}
	}

	// Write the chunk header, which contains the frequency table, the total number of files, and the total number of bytes
	bool anythingToWrite;
	outBitstream.Write((MessageID)ID_FILE_LIST_TRANSFER_HEADER);
	outBitstream.Write(setID);
	anythingToWrite=fileList->fileList.Size()>0;
	outBitstream.Write(anythingToWrite);
	if (anythingToWrite)
	{
		if (compressData)
		{
			outBitstream.Write(true);
			for (i=0; i < 256; i++)
				outBitstream.WriteCompressed(frequencyTable[i]);
			outBitstream.WriteCompressed(fileList->fileList.Size()); // SetCount
			outBitstream.WriteCompressed(totalLength);
			outBitstream.WriteCompressed(totalCompressedLength);
		}
		else
		{
			outBitstream.Write(false);
			outBitstream.WriteCompressed(fileList->fileList.Size());
			outBitstream.WriteCompressed(totalLength);
		}
		
		rakPeer->Send(&outBitstream, priority, RELIABLE_ORDERED, orderingChannel, recipient, false);

		// Next part arrives at FileListTransfer::DecodeFile
		if (compressData)
		{
			// Send each possibly compressed file
			for (i=0; i < compressedFiles.Size(); i++)
			{
				outBitstream.Reset();
				outBitstream.Write((MessageID)ID_FILE_LIST_TRANSFER_FILE);
				outBitstream.Write(fileList->fileList[i].context);
				outBitstream.Write(setID);
				outBitstream.WriteCompressed(i);
				outBitstream.WriteCompressed(fileList->fileList[i].dataLengthBytes); // Original length
				outBitstream.WriteCompressed(compressedFiles[i].dataLengthBytes); // Compressed bitlength (yes, not bytes)
				stringCompressor->EncodeString(fileList->fileList[i].filename, 512, &outBitstream);
				outBitstream.WriteBits((const unsigned char*)compressedFiles[i].data, compressedFiles[i].dataLengthBytes);
//				rakFree(compressedFiles[i].data);

//				rakPeer->Send(&outBitstream, priority, RELIABLE_ORDERED, orderingChannel, recipient, false);

				// This is much more efficient than the commented code, because it doesn't require writing the file to the bitstream just to pass to Send
				char *dataBlocks[2];
				int lengths[2];
				dataBlocks[0]=(char*) outBitstream.GetData();
				lengths[0]=outBitstream.GetNumberOfBytesUsed();
				dataBlocks[1]=compressedFiles[i].data;
				lengths[1]=BITS_TO_BYTES(compressedFiles[i].dataLengthBytes);
				rakPeer->SendList(dataBlocks,lengths,2,priority, RELIABLE_ORDERED, orderingChannel, recipient, false);
				rakFree(compressedFiles[i].data);
			}
		}
		else
		{
			for (i=0; i < fileList->fileList.Size(); i++)
			{
				outBitstream.Reset();
				outBitstream.Write((MessageID)ID_FILE_LIST_TRANSFER_FILE);
				outBitstream.Write(fileList->fileList[i].context);
				outBitstream.Write(setID);
				outBitstream.WriteCompressed(i);
				outBitstream.WriteCompressed(fileList->fileList[i].dataLengthBytes); // Original length in bytes
				stringCompressor->EncodeString(fileList->fileList[i].filename, 512, &outBitstream);
				outBitstream.AlignWriteToByteBoundary();
	//			outBitstream.Write((const char*) fileList->fileList[i].data, fileList->fileList[i].dataLengthBytes);

	//			rakPeer->Send(&outBitstream, priority, RELIABLE_ORDERED, orderingChannel, recipient, false);

				// This is much more efficient than the commented code, because it doesn't require writing the file to the bitstream just to pass to Send
				char *dataBlocks[2];
				int lengths[2];
				dataBlocks[0]=(char*) outBitstream.GetData();
				lengths[0]=outBitstream.GetNumberOfBytesUsed();
				dataBlocks[1]=fileList->fileList[i].data;
				lengths[1]=fileList->fileList[i].dataLengthBytes;
				rakPeer->SendList(dataBlocks,lengths,2,priority, RELIABLE_ORDERED, orderingChannel, recipient, false);
			}

		}
	}
	else
		rakPeer->Send(&outBitstream, priority, RELIABLE_ORDERED, orderingChannel, recipient, false);
}

bool FileListTransfer::DecodeSetHeader(Packet *packet)
{
	unsigned i;
	unsigned int frequencyTable[ 256 ];
	bool anythingToWrite=false;
	unsigned short setID;
	RakNet::BitStream inBitStream(packet->data, packet->length, false);
	inBitStream.IgnoreBits(8);
	inBitStream.Read(setID);
	FileListReceiver *fileListReceiver;
	if (fileListReceivers.Has(setID)==false)
	{
#ifdef _DEBUG
		assert(0);
#endif
		return false;
	}
	fileListReceiver=fileListReceivers.Get(setID);
	if (fileListReceiver->allowedSender!=packet->systemAddress)
	{
#ifdef _DEBUG
		assert(0);
#endif
		return false;
	}

#ifdef _DEBUG
	assert(fileListReceiver->gotSetHeader==false);
#endif

	inBitStream.Read(anythingToWrite);
	if (anythingToWrite)
	{
		inBitStream.Read(fileListReceiver->isCompressed);
		if (fileListReceiver->isCompressed)
		{
			for (i=0; i < 256; i++)
				inBitStream.ReadCompressed(frequencyTable[i]);
			fileListReceiver->tree.GenerateFromFrequencyTable(frequencyTable);
			inBitStream.ReadCompressed(fileListReceiver->setCount);
			inBitStream.ReadCompressed(fileListReceiver->setTotalFinalLength);
			if (inBitStream.ReadCompressed(fileListReceiver->setTotalCompressedTransmissionLength))
			{
				fileListReceiver->gotSetHeader=true;
				return true;
			}
		}
		else
		{
			inBitStream.ReadCompressed(fileListReceiver->setCount);
			if (inBitStream.ReadCompressed(fileListReceiver->setTotalFinalLength))
			{
				fileListReceiver->setTotalCompressedTransmissionLength=fileListReceiver->setTotalFinalLength;
				fileListReceiver->gotSetHeader=true;
				return true;
			}
		}
	}
	else
	{
		FileListTransferCBInterface::OnFileStruct s;
		memset(&s,0,sizeof(FileListTransferCBInterface::OnFileStruct));
		s.setID=setID;
		fileListReceiver->downloadHandler->OnFile(&s);
		return true;
	}

	return false;
}

bool FileListTransfer::DecodeFile(Packet *packet, bool fullFile)
{
	FileListTransferCBInterface::OnFileStruct onFileStruct;
	BitSize_t bitLength=0;
	RakNet::BitStream inBitStream(packet->data, packet->length, false);
	inBitStream.IgnoreBits(8);

	unsigned int partCount=0;
	unsigned int partTotal=0;
	unsigned int partLength=0;
	onFileStruct.fileData=0;
	if (fullFile==false)
	{
		// Disable endian swapping on reading this, as it's generated locally in ReliabilityLayer.cpp
		inBitStream.ReadBits( (unsigned char* ) &partCount, BYTES_TO_BITS(sizeof(partCount)), true );
		inBitStream.ReadBits( (unsigned char* ) &partTotal, BYTES_TO_BITS(sizeof(partTotal)), true );
		inBitStream.ReadBits( (unsigned char* ) &partLength, BYTES_TO_BITS(sizeof(partLength)), true );
		inBitStream.IgnoreBits(8);
		// The header is appended to every chunk, which we continue to read after this statement block
	}
	inBitStream.Read(onFileStruct.context);
	inBitStream.Read(onFileStruct.setID);
	FileListReceiver *fileListReceiver;
	if (fileListReceivers.Has(onFileStruct.setID)==false)
	{
		return false;
	}
	fileListReceiver=fileListReceivers.Get(onFileStruct.setID);
	if (fileListReceiver->allowedSender!=packet->systemAddress)
	{
#ifdef _DEBUG
		assert(0);
#endif
		return false;
	}

#ifdef _DEBUG
	assert(fileListReceiver->gotSetHeader==true);
#endif

	inBitStream.ReadCompressed(onFileStruct.fileIndex);
	inBitStream.ReadCompressed(onFileStruct.finalDataLength);
	if (fileListReceiver->isCompressed)
	{
		inBitStream.ReadCompressed(bitLength);
		onFileStruct.compressedTransmissionLength=(unsigned int) BITS_TO_BYTES(bitLength);
	}
	else
	{
		// Read header uncompressed so the data is byte aligned, for speed
		onFileStruct.compressedTransmissionLength=(unsigned int) onFileStruct.finalDataLength;
	}

	if (stringCompressor->DecodeString(onFileStruct.fileName, 512, &inBitStream)==false)
	{
#ifdef _DEBUG
		assert(0);
#endif
		return false;
	}

	if (fullFile)
	{
		// Support SendLists
		inBitStream.AlignReadToByteBoundary();

		onFileStruct.fileData = (char*) rakMalloc( (size_t) onFileStruct.finalDataLength );

		if (fileListReceiver->isCompressed)
		{
			BitSize_t len=fileListReceiver->tree.DecodeArray(&inBitStream, bitLength, (size_t) onFileStruct.finalDataLength, (unsigned char*) onFileStruct.fileData);
			if (len!=onFileStruct.finalDataLength)
			{
#ifdef _DEBUG
				assert(0);
#endif
				rakFree(onFileStruct.fileData);
				return false;
			}
		}
		else
		{
			inBitStream.Read((char*)onFileStruct.fileData, onFileStruct.finalDataLength);
		}
	}
	

	onFileStruct.setCount=fileListReceiver->setCount;
	onFileStruct.setTotalCompressedTransmissionLength=fileListReceiver->setTotalCompressedTransmissionLength;
	onFileStruct.setTotalFinalLength=fileListReceiver->setTotalFinalLength;

	// User callback for this file.
	if (fullFile)
	{
		if (fileListReceiver->downloadHandler->OnFile(&onFileStruct))
			rakFree(onFileStruct.fileData);

		// If this set is done, free the memory for it.
		if (fileListReceiver->setCount==onFileStruct.fileIndex+1)
		{
			if (fileListReceiver->downloadHandler->OnDownloadComplete()==false)
			{
				fileListReceiver->downloadHandler->OnDereference();
				if (fileListReceiver->deleteDownloadHandler)
					delete fileListReceiver->downloadHandler;
				fileListReceivers.Delete(onFileStruct.setID);
				delete fileListReceiver;
			}
		}

	}
	else
	{
		inBitStream.AlignReadToByteBoundary();

		bool usedAlloca=false;
		char *firstDataChunk;
		unsigned int unreadBits = inBitStream.GetNumberOfUnreadBits();
		unsigned int unreadBytes = BITS_TO_BYTES(unreadBits);
		if (unreadBytes>0)
		{
#if !defined(_XBOX360)
			if (unreadBits < MAX_ALLOCA_STACK_ALLOCATION)
			{
				firstDataChunk = ( char* ) alloca( unreadBytes );
				usedAlloca=true;
			}
			else
#endif
				firstDataChunk = (char*) rakMalloc( unreadBytes );

			// Read partLength bytes, reported to OnFileProgress

			if (fileListReceiver->isCompressed)
				fileListReceiver->tree.DecodeArray(&inBitStream, unreadBits, partLength, (unsigned char*) firstDataChunk);
			else
				inBitStream.Read((char*)firstDataChunk, unreadBytes );

		}
		else
			firstDataChunk=0;

		fileListReceiver->downloadHandler->OnFileProgress(&onFileStruct, partCount, partTotal, unreadBytes, firstDataChunk);

		if (usedAlloca==false)
			delete [] firstDataChunk;
	}

	return true;
}
PluginReceiveResult FileListTransfer::OnReceive(RakPeerInterface *peer, Packet *packet)
{
	(void) peer;
	switch (packet->data[0]) 
	{
	case ID_CONNECTION_LOST:
	case ID_DISCONNECTION_NOTIFICATION:
		RemoveReceiver(packet->systemAddress);
	break;
	case ID_FILE_LIST_TRANSFER_HEADER:
		DecodeSetHeader(packet);
		return RR_STOP_PROCESSING_AND_DEALLOCATE;
	case ID_FILE_LIST_TRANSFER_FILE:
		DecodeFile(packet, true);
		return RR_STOP_PROCESSING_AND_DEALLOCATE;
	case ID_DOWNLOAD_PROGRESS:
		if (packet->length>sizeof(MessageID)+sizeof(unsigned int)*3 &&
			packet->data[sizeof(MessageID)+sizeof(unsigned int)*3]==ID_FILE_LIST_TRANSFER_FILE)
		{
			DecodeFile(packet, false);
			return RR_STOP_PROCESSING_AND_DEALLOCATE;
		}
		break;
	}

	return RR_CONTINUE_PROCESSING;
}
void FileListTransfer::OnShutdown(RakPeerInterface *peer)
{
	(void) peer;
	Clear();	
}
void FileListTransfer::Clear(void)
{
	unsigned i;
	for (i=0; i < fileListReceivers.Size(); i++)
	{
		fileListReceivers[i]->downloadHandler->OnDereference();
		if (fileListReceivers[i]->deleteDownloadHandler)
		{
			delete fileListReceivers[i]->downloadHandler;
		}
		delete fileListReceivers[i];
	}
	fileListReceivers.Clear();
}
void FileListTransfer::OnCloseConnection(RakPeerInterface *peer, SystemAddress systemAddress)
{
	(void) peer;
	RemoveReceiver(systemAddress);
}
void FileListTransfer::CancelReceive(unsigned short setId)
{
	if (fileListReceivers.Has(setId)==false)
	{
#ifdef _DEBUG
		assert(0);
#endif
		return;
	}
	FileListReceiver *fileListReceiver=fileListReceivers.Get(setId);
	fileListReceiver->downloadHandler->OnDereference();
	if (fileListReceiver->deleteDownloadHandler)
		delete fileListReceiver->downloadHandler;
	delete fileListReceiver;
	fileListReceivers.Delete(setId);
}
void FileListTransfer::RemoveReceiver(SystemAddress systemAddress)
{
	unsigned i;
	i=0;
	while (i < fileListReceivers.Size())
	{
		if (fileListReceivers[i]->allowedSender==systemAddress)
		{
			fileListReceivers[i]->downloadHandler->OnDereference();
			if (fileListReceivers[i]->deleteDownloadHandler)
			{
				delete fileListReceivers[i]->downloadHandler;
			}
			delete fileListReceivers[i];
			fileListReceivers.RemoveAtIndex(i);
		}
		else
			i++;
	}
}
bool FileListTransfer::IsHandlerActive(unsigned short setId)
{
	return fileListReceivers.Has(setId);
}
void FileListTransfer::SetCallback(FileListProgress *cb)
{
	callback=cb;
}
FileListProgress *FileListTransfer::GetCallback(void) const
{
	return callback;
}
void FileListTransfer::OnAttach(RakPeerInterface *peer)
{
	rakPeer=peer;
}
void FileListTransfer::Update(RakPeerInterface *peer)
{
	(void) peer;

	unsigned i;
	i=0;
	while (i < fileListReceivers.Size())
	{
		if (fileListReceivers[i]->downloadHandler->Update()==false)
		{
			fileListReceivers[i]->downloadHandler->OnDereference();
			if (fileListReceivers[i]->deleteDownloadHandler)
				delete fileListReceivers[i]->downloadHandler;
			delete fileListReceivers[i];
			fileListReceivers.RemoveAtIndex(i);
		}
		else
			i++;
	}
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
