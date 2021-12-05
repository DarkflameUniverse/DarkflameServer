#include "DirectoryDeltaTransfer.h"
#include "FileList.h"
#include "StringCompressor.h"
#include "RakPeerInterface.h"
#include "FileListTransfer.h"
#include "FileListTransferCBInterface.h"
#include "BitStream.h"
#include "MessageIdentifiers.h"
#include "FileOperations.h"

#ifdef _MSC_VER
#pragma warning( push )
#endif

class DDTCallback : public FileListTransferCBInterface
{
public:
	unsigned subdirLen;
	char outputSubdir[512];
	FileListTransferCBInterface *onFileCallback;

	DDTCallback() {}
	virtual ~DDTCallback() {}

	virtual bool OnFile(OnFileStruct *onFileStruct)
	{
		char fullPathToDir[1024];

		if (onFileStruct->fileName && onFileStruct->fileData && subdirLen < strlen(onFileStruct->fileName))
		{
			strcpy(fullPathToDir, outputSubdir);
			strcat(fullPathToDir, onFileStruct->fileName+subdirLen);
			WriteFileWithDirectories(fullPathToDir, (char*)onFileStruct->fileData, (unsigned int ) onFileStruct->finalDataLength);
		}
		else
			fullPathToDir[0]=0;

		return onFileCallback->OnFile(onFileStruct);
	}

	virtual void OnFileProgress(OnFileStruct *onFileStruct,unsigned int partCount,unsigned int partTotal,unsigned int partLength, char *firstDataChunk)
	{
		char fullPathToDir[1024];

		if (onFileStruct->fileName && subdirLen < strlen(onFileStruct->fileName))
		{
			strcpy(fullPathToDir, outputSubdir);
			strcat(fullPathToDir, onFileStruct->fileName+subdirLen);
		}
		else
			fullPathToDir[0]=0;

		onFileCallback->OnFileProgress(onFileStruct, partCount, partTotal, partLength, firstDataChunk);
	}
	virtual bool OnDownloadComplete(void)
	{
		return onFileCallback->OnDownloadComplete();
	}
};

DirectoryDeltaTransfer::DirectoryDeltaTransfer()
{
	applicationDirectory[0]=0;
	fileListTransfer=0;
	availableUploads = new FileList;
	rakPeer=0;
	priority=HIGH_PRIORITY;
	orderingChannel=0;
	compressOutgoingSends=false;
}
DirectoryDeltaTransfer::~DirectoryDeltaTransfer()
{
	delete availableUploads;
}
void DirectoryDeltaTransfer::SetFileListTransferPlugin(FileListTransfer *flt)
{
	fileListTransfer=flt;
	if (flt)
		availableUploads->SetCallback(flt->GetCallback());
	else
		availableUploads->SetCallback(0);
}
void DirectoryDeltaTransfer::SetApplicationDirectory(const char *pathToApplication)
{
	if (pathToApplication==0 || pathToApplication[0]==0)
		applicationDirectory[0]=0;
	else
	{
		strncpy(applicationDirectory, pathToApplication, 510);
		if (applicationDirectory[strlen(applicationDirectory)-1]!='/' && applicationDirectory[strlen(applicationDirectory)-1]!='\\')
			strcat(applicationDirectory, "/");
		applicationDirectory[511]=0;
	}
}
void DirectoryDeltaTransfer::SetUploadSendParameters(PacketPriority _priority, char _orderingChannel)
{
	priority=_priority;
	orderingChannel=_orderingChannel;
}
void DirectoryDeltaTransfer::AddUploadsFromSubdirectory(const char *subdir)
{
	availableUploads->AddFilesFromDirectory(applicationDirectory, subdir, true, false, true, 0);
}
unsigned short DirectoryDeltaTransfer::DownloadFromSubdirectory(const char *subdir, const char *outputSubdir, bool prependAppDirToOutputSubdir, SystemAddress host, FileListTransferCBInterface *onFileCallback, PacketPriority _priority, char _orderingChannel, FileListProgress *cb)
{
	if (rakPeer->IsConnected(host)==false)
		return (unsigned short) -1;

	DDTCallback *transferCallback;
	FileList localFiles;
	localFiles.SetCallback(cb);
	// Get a hash of all the files that we already have (if any)
	localFiles.AddFilesFromDirectory(prependAppDirToOutputSubdir ? applicationDirectory : 0, outputSubdir, true, false, true, 0);

	// Prepare the callback data
	transferCallback = new DDTCallback;
	if (subdir && subdir[0])
	{
		transferCallback->subdirLen=(unsigned int)strlen(subdir);
		if (subdir[transferCallback->subdirLen-1]!='/' && subdir[transferCallback->subdirLen-1]!='\\')
			transferCallback->subdirLen++;
	}
	else
		transferCallback->subdirLen=0;
	if (prependAppDirToOutputSubdir)
		strcpy(transferCallback->outputSubdir, applicationDirectory);
	else
		transferCallback->outputSubdir[0]=0;
	if (outputSubdir)
		strcat(transferCallback->outputSubdir, outputSubdir);
	if (transferCallback->outputSubdir[strlen(transferCallback->outputSubdir)-1]!='/' && transferCallback->outputSubdir[strlen(transferCallback->outputSubdir)-1]!='\\')
		strcat(transferCallback->outputSubdir, "/");
	transferCallback->onFileCallback=onFileCallback;

	// Setup the transfer plugin to get the response to this download request
	unsigned short setId = fileListTransfer->SetupReceive(transferCallback, true, host);

	// Send to the host, telling it to process this request
	RakNet::BitStream outBitstream;
	outBitstream.Write((MessageID)ID_DDT_DOWNLOAD_REQUEST);
	outBitstream.Write(setId);
	stringCompressor->EncodeString(subdir, 256, &outBitstream);
	stringCompressor->EncodeString(outputSubdir, 256, &outBitstream);
    localFiles.Serialize(&outBitstream);
	rakPeer->Send(&outBitstream, _priority, RELIABLE_ORDERED, _orderingChannel, host, false);

	return setId;
}
void DirectoryDeltaTransfer::ClearUploads(void)
{
	availableUploads->Clear();
}
void DirectoryDeltaTransfer::OnDownloadRequest(RakPeerInterface *peer, Packet *packet)
{
	(void) peer;

	char subdir[256];
	char remoteSubdir[256];
	RakNet::BitStream inBitstream(packet->data, packet->length, false);
	FileList remoteFileHash;
	FileList delta;
	unsigned short setId;
    inBitstream.IgnoreBits(8);
	inBitstream.Read(setId);
	stringCompressor->DecodeString(subdir, 256, &inBitstream);
	stringCompressor->DecodeString(remoteSubdir, 256, &inBitstream);
	if (remoteFileHash.Deserialize(&inBitstream)==false)
	{
#ifdef _DEBUG
		assert(0);
#endif
		return;
	}

	availableUploads->GetDeltaToCurrent(&remoteFileHash, &delta, subdir, remoteSubdir);
	delta.PopulateDataFromDisk(applicationDirectory, true, false, true);

	// This will call the ddtCallback interface that was passed to FileListTransfer::SetupReceive on the remote system
	fileListTransfer->Send(&delta, rakPeer, packet->systemAddress, setId, priority, orderingChannel, compressOutgoingSends);
}
void DirectoryDeltaTransfer::OnAttach(RakPeerInterface *peer)
{
	rakPeer=peer;
}
void DirectoryDeltaTransfer::Update(RakPeerInterface *peer)
{
	(void) peer;
}
PluginReceiveResult DirectoryDeltaTransfer::OnReceive(RakPeerInterface *peer, Packet *packet)
{
	switch (packet->data[0]) 
	{
	case ID_DDT_DOWNLOAD_REQUEST:
		OnDownloadRequest(peer, packet);
		return RR_STOP_PROCESSING_AND_DEALLOCATE;
	}

	return RR_CONTINUE_PROCESSING;
}
void DirectoryDeltaTransfer::OnShutdown(RakPeerInterface *peer)
{
	(void) peer;
}

unsigned DirectoryDeltaTransfer::GetNumberOfFilesForUpload(void) const
{
	return availableUploads->fileList.Size();
}
void DirectoryDeltaTransfer::SetCompressOutgoingSends(bool compress)
{
	compressOutgoingSends=compress;
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
