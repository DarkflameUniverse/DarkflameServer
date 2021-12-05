#include "FileList.h"
#include <stdio.h> // printf
#include <assert.h>
#if defined(_WIN32) || defined(__CYGWIN__)
	#include <io.h>
#elif !defined ( __APPLE__ ) && !defined ( __APPLE_CC__ ) && !defined ( __PPC__ ) && !defined ( __FreeBSD__ )
	#include <sys/io.h>
#endif
#include "DS_Queue.h"
#ifdef _WIN32 
// For mkdir
#include <direct.h>
#else
#include <sys/stat.h>
#endif
//#include "SHA1.h"
#include "StringCompressor.h"
#include "BitStream.h"
#include "FileOperations.h"
#include "SuperFastHash.h"
#include "RakAssert.h"
#include "LinuxStrings.h"

#define MAX_FILENAME_LENGTH 512
static const unsigned HASH_LENGTH=sizeof(unsigned int);

// alloca
#ifdef _XBOX360
#elif defined(_WIN32)
#include <malloc.h>
#else
#if !defined ( __FreeBSD__ )
#include <alloca.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "_FindFirst.h"
#include <stdint.h> //defines intptr_t
#endif

//int RAK_DLL_EXPORT FileListNodeComp( char * const &key, const FileListNode &data )
//{
//	return strcmp(key, data.filename);
//}


#ifdef _MSC_VER
#pragma warning( push )
#endif

/// First callback called when FileList::AddFilesFromDirectory() starts
void FLP_Printf::OnAddFilesFromDirectoryStarted(FileList *fileList, char *dir) {
	(void) fileList;
	printf("Adding files from directory %s\n",dir);}

/// Called for each directory, when that directory begins processing
void FLP_Printf::OnDirectory(FileList *fileList, char *dir, unsigned int directoriesRemaining) {
	(void) fileList;
	printf("Adding %s. %i remaining.\n", dir, directoriesRemaining);}	

FileList::FileList()
{
	callback=0;
}
FileList::~FileList()
{
	Clear();
}
void FileList::AddFile(const char *filepath, const char *filename, unsigned char context)
{
	if (filepath==0 || filename==0)
		return;

	char *data;
	//std::fstream file;
	//file.open(filename, std::ios::in | std::ios::binary);

	FILE *fp = fopen(filepath, "rb");
	if (fp==0)
		return;
	fseek(fp, 0, SEEK_END);
	int length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (length > (int) ((unsigned int)-1 / 8))
	{
		// If this assert hits, split up your file. You could also change BitSize_t in RakNetTypes.h to unsigned long long but this is not recommended for performance reasons
		assert("Cannot add files over 536 MB" && 0);
		fclose(fp);
		return;
	}


#if !defined(_XBOX360)
	bool usedAlloca=false;
	if (length < MAX_ALLOCA_STACK_ALLOCATION)
	{
		data = ( char* ) alloca( length );
		usedAlloca=true;
	}
	else
#endif
	{
		data = (char*) rakMalloc( length );
	}

	fread(data, 1, length, fp);
	AddFile(filename, data, length, length, context);
	fclose(fp);

#if !defined(_XBOX360)
	if (usedAlloca==false)
#endif
		rakFree(data);

}
void FileList::AddFile(const char *filename, const char *data, const unsigned dataLength, const unsigned fileLength, unsigned char context)
{
	if (filename==0)
		return;
	if (strlen(filename)>MAX_FILENAME_LENGTH)
	{
		// Should be enough for anyone
		assert(0);
		return;
	}
	// Avoid duplicate insertions unless the data is different, in which case overwrite the old data
	unsigned i;
	for (i=0; i<fileList.Size();i++)
	{
		if (strcmp(fileList[i].filename, filename)==0)
		{
			if (fileList[i].fileLengthBytes==fileLength && fileList[i].dataLengthBytes==dataLength &&
				(dataLength==0 || memcmp(fileList[i].data, data, dataLength)==0))
				// Exact same file already here
				return;

			// File of the same name, but different contents, so overwrite
			rakFree(fileList[i].data);
			rakFree(fileList[i].filename);
			fileList.RemoveAtIndex(i);
			break;
		}
	}

	FileListNode n;
	n.filename=(char*) rakMalloc( strlen(filename)+1 );
	if (dataLength)
	{
		n.data=(char*) rakMalloc( dataLength );
		memcpy(n.data, data, dataLength);
	}
	else
		n.data=0;
	n.dataLengthBytes=dataLength;
	n.fileLengthBytes=fileLength;
	n.context=context;
	strcpy(n.filename, filename);
		
	fileList.Insert(n);
}
void FileList::AddFilesFromDirectory(const char *applicationDirectory, const char *subDirectory, bool writeHash, bool writeData, bool recursive, unsigned char context)
{
	DataStructures::Queue<char*> dirList;
	char root[260];
	char fullPath[520];
	_finddata_t fileInfo;
	intptr_t dir;
	FILE *fp;
	char *dirSoFar, *fileData;
	dirSoFar=(char*) rakMalloc( 520 );

	if (applicationDirectory)
		strcpy(root, applicationDirectory);
	else
		root[0]=0;

	int rootLen=(int)strlen(root);
	if (rootLen)
	{
		strcpy(dirSoFar, root);
		if (FixEndingSlash(dirSoFar))
			rootLen++;
	}
	else
		dirSoFar[0]=0;
	
	if (subDirectory)
	{
		strcat(dirSoFar, subDirectory);
		FixEndingSlash(dirSoFar);
	}
	if (callback)
		callback->OnAddFilesFromDirectoryStarted(this, dirSoFar);
	// printf("Adding files from directory %s\n",dirSoFar);
	dirList.Push(dirSoFar);
	while (dirList.Size())
	{
		dirSoFar=dirList.Pop();
		strcpy(fullPath, dirSoFar);
		// Changed from *.* to * for Linux compatibility
		strcat(fullPath, "*");


                dir=_findfirst(fullPath, &fileInfo );
		if (dir==-1)
		{
			_findclose(dir);
			rakFree(dirSoFar);
			unsigned i;
			for (i=0; i < dirList.Size(); i++)
				rakFree(dirList[i]);
			return;
		}

//		printf("Adding %s. %i remaining.\n", fullPath, dirList.Size());
		if (callback)
			callback->OnDirectory(this, fullPath, dirList.Size());

                do
		{
                    // no guarantee these entries are first...
                    if (strcmp("." , fileInfo.name) == 0 ||
                        strcmp("..", fileInfo.name) == 0)
                    {
                        continue;
                    }
                    
			if ((fileInfo.attrib & (_A_HIDDEN | _A_SUBDIR | _A_SYSTEM))==0)
			{
				strcpy(fullPath, dirSoFar);
				strcat(fullPath, fileInfo.name);
				fileData=0;

				if (callback)
					callback->OnFile(this, dirSoFar, fileInfo.name, fileInfo.size);

				if (writeData && writeHash)
				{
					fileData= (char*) rakMalloc( fileInfo.size+HASH_LENGTH );
					fp = fopen(fullPath, "rb");
					fread(fileData+HASH_LENGTH, fileInfo.size, 1, fp);
					fclose(fp);

					unsigned int hash = SuperFastHash(fileData+HASH_LENGTH, fileInfo.size);
					memcpy(fileData, &hash, HASH_LENGTH);

//					sha1.Reset();
//					sha1.Update( ( unsigned char* ) fileData+HASH_LENGTH, fileInfo.size );
//					sha1.Final();
//					memcpy(fileData, sha1.GetHash(), HASH_LENGTH);
					// File data and hash
					AddFile((const char*)fullPath+rootLen, fileData, fileInfo.size+HASH_LENGTH, fileInfo.size, context);
				}
				else if (writeHash)
				{
//					sha1.Reset();
//					sha1.HashFile((char*)fullPath);
//					sha1.Final();

					unsigned int hash = SuperFastHashFile(fullPath);

					// Hash only
				//	AddFile((const char*)fullPath+rootLen, (const char*)sha1.GetHash(), HASH_LENGTH, fileInfo.size, context);
					AddFile((const char*)fullPath+rootLen, (const char*)&hash, HASH_LENGTH, fileInfo.size, context);
				}
				else if (writeData)
				{
					fileData= (char*) rakMalloc( fileInfo.size );
					fp = fopen(fullPath, "rb");
					fread(fileData, fileInfo.size, 1, fp);
					fclose(fp);

					// File data only
					AddFile(fullPath+rootLen, fileData, fileInfo.size, fileInfo.size, context);
				}
				else
				{
					// Just the filename
					AddFile(fullPath+rootLen, 0, 0, fileInfo.size, context);
				}

				if (fileData)
					rakFree(fileData);
			}
			else if ((fileInfo.attrib & _A_SUBDIR) && (fileInfo.attrib & (_A_HIDDEN | _A_SYSTEM))==0 && recursive)
			{
				char *newDir=(char*) rakMalloc( 520 );
				strcpy(newDir, dirSoFar);
				strcat(newDir, fileInfo.name);
				strcat(newDir, "/");
				dirList.Push(newDir);
			}

		} while (_findnext(dir, &fileInfo ) != -1);

		_findclose(dir);
		rakFree(dirSoFar);
	}
}
void FileList::Clear(void)
{
	unsigned i;
	for (i=0; i<fileList.Size(); i++)
	{
		rakFree(fileList[i].data);
		rakFree(fileList[i].filename);
	}
	fileList.Clear();
}
void FileList::Serialize(RakNet::BitStream *outBitStream)
{
	outBitStream->WriteCompressed(fileList.Size());
	unsigned i;
	for (i=0; i < fileList.Size(); i++)
	{
		outBitStream->WriteCompressed(fileList[i].context);
		stringCompressor->EncodeString(fileList[i].filename, MAX_FILENAME_LENGTH, outBitStream);
		outBitStream->Write((bool)(fileList[i].dataLengthBytes>0==true));
		if (fileList[i].dataLengthBytes>0)
		{
			outBitStream->WriteCompressed(fileList[i].dataLengthBytes);
			outBitStream->Write(fileList[i].data, fileList[i].dataLengthBytes);
		}

		outBitStream->Write((bool)(fileList[i].fileLengthBytes==fileList[i].dataLengthBytes));
		if (fileList[i].fileLengthBytes!=fileList[i].dataLengthBytes)
			outBitStream->WriteCompressed(fileList[i].fileLengthBytes);
	}
}
bool FileList::Deserialize(RakNet::BitStream *inBitStream)
{
	bool b, dataLenNonZero=false, fileLenMatchesDataLen=false;
	char filename[512];
	unsigned int fileListSize;
	FileListNode n;
	b=inBitStream->ReadCompressed(fileListSize);
#ifdef _DEBUG
	assert(b);
	assert(fileListSize < 10000);
#endif
	if (b==false || fileListSize > 10000)
		return false; // Sanity check
	Clear();	
	unsigned i;
	for (i=0; i < fileListSize; i++)
	{
		inBitStream->ReadCompressed(n.context);
		stringCompressor->DecodeString((char*)filename, MAX_FILENAME_LENGTH, inBitStream);
		inBitStream->Read(dataLenNonZero);
		if (dataLenNonZero)
		{
			inBitStream->ReadCompressed(n.dataLengthBytes);
			// sanity check
			if (n.dataLengthBytes>2000000000)
			{
#ifdef _DEBUG
				assert(n.dataLengthBytes<=2000000000);
#endif
				return false;
			}
			n.data=(char*) rakMalloc( (size_t) n.dataLengthBytes );
			inBitStream->Read(n.data, n.dataLengthBytes);
		}
		else
		{
			n.dataLengthBytes=0;
			n.data=0;
		}
		
		b=inBitStream->Read(fileLenMatchesDataLen);
		if (fileLenMatchesDataLen)
			n.fileLengthBytes=(unsigned) n.dataLengthBytes;
		else
			b=inBitStream->ReadCompressed(n.fileLengthBytes);
#ifdef _DEBUG
		assert(b);
#endif
		if (b==0)
		{
			Clear();
			return false;
		}
		n.filename=(char*) rakMalloc( strlen(filename)+1 );
		strcpy(n.filename, filename);
		fileList.Insert(n);
	}

	return true;
}
void FileList::GetDeltaToCurrent(FileList *input, FileList *output, const char *dirSubset, const char *remoteSubdir)
{
	// For all files in this list that do not match the input list, write them to the output list.
	// dirSubset allows checking only a portion of the files in this list.
	unsigned thisIndex, inputIndex;
	unsigned dirSubsetLen, localPathLen, remoteSubdirLen;
	bool match;
	if (dirSubset)
		dirSubsetLen = (unsigned int) strlen(dirSubset);
	else
		dirSubsetLen = 0;
	if (remoteSubdir && remoteSubdir[0])
	{
		remoteSubdirLen=(unsigned int) strlen(remoteSubdir);
		if (IsSlash(remoteSubdir[remoteSubdirLen-1]))
			remoteSubdirLen--;
	}
	else
		remoteSubdirLen=0;

	for (thisIndex=0; thisIndex < fileList.Size(); thisIndex++)
	{
		localPathLen = (unsigned int) strlen(fileList[thisIndex].filename);
		while (localPathLen>0)
		{
			if (IsSlash(fileList[thisIndex].filename[localPathLen-1]))
			{
				localPathLen--;
				break;
			}
			localPathLen--;
		}

		// fileList[thisIndex].filename has to match dirSubset and be shorter or equal to it in length.
		if (dirSubsetLen>0 &&
			(localPathLen<dirSubsetLen ||
			_strnicmp(fileList[thisIndex].filename, dirSubset, dirSubsetLen)!=0 ||
			(localPathLen>dirSubsetLen && IsSlash(fileList[thisIndex].filename[dirSubsetLen])==false)))
			continue;

		match=false;
		for (inputIndex=0; inputIndex < input->fileList.Size(); inputIndex++)
		{
			// If the filenames, hashes, and lengths match then skip this element in fileList.  Otherwise write it to output
			if (_stricmp(input->fileList[inputIndex].filename+remoteSubdirLen,fileList[thisIndex].filename+dirSubsetLen)==0)
			{
				match=true;
				if (input->fileList[inputIndex].fileLengthBytes==fileList[thisIndex].fileLengthBytes &&
					input->fileList[inputIndex].dataLengthBytes==fileList[thisIndex].dataLengthBytes &&
					memcmp(input->fileList[inputIndex].data,fileList[thisIndex].data,(size_t) fileList[thisIndex].dataLengthBytes)==0)
				{
					// File exists on both machines and is the same.
					break;
				}
				else
				{
					// File exists on both machines and is not the same.
					output->AddFile(fileList[thisIndex].filename, 0,0, fileList[thisIndex].fileLengthBytes, 0);
					break;
				}
			}
		}
		if (match==false)
		{
            // Other system does not have the file at all
			output->AddFile(fileList[thisIndex].filename, 0,0, fileList[thisIndex].fileLengthBytes, 0);
		}
	}
}
void FileList::ListMissingOrChangedFiles(const char *applicationDirectory, FileList *missingOrChangedFiles, bool alwaysWriteHash, bool neverWriteHash)
{
	unsigned fileLength;
//	CSHA1 sha1;
	FILE *fp;
	char fullPath[512];
	unsigned i;
//	char *fileData;

	for (i=0; i < fileList.Size(); i++)
	{
		strcpy(fullPath, applicationDirectory);
		FixEndingSlash(fullPath);
		strcat(fullPath,fileList[i].filename);
		fp=fopen(fullPath, "rb");
		if (fp==0)
		{
			missingOrChangedFiles->AddFile(fileList[i].filename, 0, 0, 0, 0);
		}
		else
		{
			fseek(fp, 0, SEEK_END);
			fileLength = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			if (fileLength != fileList[i].fileLengthBytes && alwaysWriteHash==false)
			{
				missingOrChangedFiles->AddFile(fileList[i].filename, 0, 0, fileLength, 0);
			}
			else
			{

//				fileData= (char*) rakMalloc( fileLength );
//				fread(fileData, fileLength, 1, fp);

//				sha1.Reset();
//				sha1.Update( ( unsigned char* ) fileData, fileLength );
//				sha1.Final();

//				rakFree(fileData);

				unsigned int hash = SuperFastHashFilePtr(fp);

				//if (fileLength != fileList[i].fileLength || memcmp( sha1.GetHash(), fileList[i].data, HASH_LENGTH)!=0)
				if (fileLength != fileList[i].fileLengthBytes || memcmp( &hash, fileList[i].data, HASH_LENGTH)!=0)
				{
					if (neverWriteHash==false)
					//	missingOrChangedFiles->AddFile((const char*)fileList[i].filename, (const char*)sha1.GetHash(), HASH_LENGTH, fileLength, 0);
						missingOrChangedFiles->AddFile((const char*)fileList[i].filename, (const char *) &hash, HASH_LENGTH, fileLength, 0);
					else
						missingOrChangedFiles->AddFile((const char*)fileList[i].filename, 0, 0, fileLength, 0);
				}
			}
			fclose(fp);
		}
	}
}
void FileList::PopulateDataFromDisk(const char *applicationDirectory, bool writeFileData, bool writeFileHash, bool removeUnknownFiles)
{
	FILE *fp;
	char fullPath[512];
	unsigned i;
//	CSHA1 sha1;

	i=0;
	while (i < fileList.Size())
	{
		rakFree(fileList[i].data);
		strcpy(fullPath, applicationDirectory);
		FixEndingSlash(fullPath);
		strcat(fullPath,fileList[i].filename);
		fp=fopen(fullPath, "rb");
		if (fp)
		{
			if (writeFileHash || writeFileData)
			{
				fseek(fp, 0, SEEK_END);
				fileList[i].fileLengthBytes = ftell(fp);
				fseek(fp, 0, SEEK_SET);
				if (writeFileHash)
				{
					if (writeFileData)
					{
						// Hash + data so offset the data by HASH_LENGTH
						fileList[i].data=(char*) rakMalloc( fileList[i].fileLengthBytes+HASH_LENGTH );
						fread(fileList[i].data+HASH_LENGTH, fileList[i].fileLengthBytes, 1, fp);
//						sha1.Reset();
//						sha1.Update((unsigned char*)fileList[i].data+HASH_LENGTH, fileList[i].fileLength);
//						sha1.Final();
						unsigned int hash = SuperFastHash(fileList[i].data+HASH_LENGTH, fileList[i].fileLengthBytes);
//						memcpy(fileList[i].data, sha1.GetHash(), HASH_LENGTH);
						memcpy(fileList[i].data, &hash, HASH_LENGTH);
					}
					else
					{
						// Hash only
						fileList[i].dataLengthBytes=HASH_LENGTH;
						if (fileList[i].fileLengthBytes < HASH_LENGTH)
							fileList[i].data=(char*) rakMalloc( HASH_LENGTH );
						else
							fileList[i].data=(char*) rakMalloc( fileList[i].fileLengthBytes );
						fread(fileList[i].data, fileList[i].fileLengthBytes, 1, fp);
				//		sha1.Reset();
				//		sha1.Update((unsigned char*)fileList[i].data, fileList[i].fileLength);
				//		sha1.Final();
						unsigned int hash = SuperFastHash(fileList[i].data, fileList[i].fileLengthBytes);
						// memcpy(fileList[i].data, sha1.GetHash(), HASH_LENGTH);
						memcpy(fileList[i].data, &hash, HASH_LENGTH);
					}
				}
				else
				{
					// Data only
					fileList[i].dataLengthBytes=fileList[i].fileLengthBytes;
					fileList[i].data=(char*) rakMalloc( fileList[i].fileLengthBytes );
					fread(fileList[i].data, fileList[i].fileLengthBytes, 1, fp);
				}

				fclose(fp);
				i++;
			}
			else
			{
				fileList[i].data=0;
				fileList[i].dataLengthBytes=0;
			}
		}
		else
		{
			if (removeUnknownFiles)
			{
				rakFree(fileList[i].filename);
				fileList.RemoveAtIndex(i);
			}
			else
				i++;			
		}
	}
}
void FileList::WriteDataToDisk(const char *applicationDirectory)
{
	char fullPath[512];
	unsigned i,j;

	for (i=0; i < fileList.Size(); i++)
	{
		strcpy(fullPath, applicationDirectory);
		FixEndingSlash(fullPath);
		strcat(fullPath,fileList[i].filename);
		
		// Security - Don't allow .. in the filename anywhere so you can't write outside of the root directory
		for (j=1; j < strlen(fileList[i].filename); j++)
		{
			if (fileList[i].filename[j]=='.' && fileList[i].filename[j-1]=='.')
			{
#ifdef _DEBUG
				assert(0);
#endif
				// Just cancel the write entirely
				return;
			}
		}

		WriteFileWithDirectories(fullPath, fileList[i].data, (unsigned int) fileList[i].dataLengthBytes);
	}
}

#ifdef _MSC_VER
#pragma warning( disable : 4966 ) // unlink declared depreciated by Microsoft in order to make it harder to be cross platform.  I don't agree it's depreciated.
#endif
void FileList::DeleteFiles(const char *applicationDirectory)
{
	char fullPath[512];
	unsigned i,j;

	for (i=0; i < fileList.Size(); i++)
	{
		// The filename should not have .. in the path - if it does ignore it
		for (j=1; j < strlen(fileList[i].filename); j++)
		{
			if (fileList[i].filename[j]=='.' && fileList[i].filename[j-1]=='.')
			{
#ifdef _DEBUG
				assert(0);
#endif
				// Just cancel the deletion entirely
				return;
			}
		}

		strcpy(fullPath, applicationDirectory);
		FixEndingSlash(fullPath);
		strcat(fullPath, fileList[i].filename);

#ifdef _MSC_VER
#pragma warning( disable : 4966 ) // unlink declared depreciated by Microsoft in order to make it harder to be cross platform.  I don't agree it's depreciated.
#endif
        int result = unlink(fullPath);
		if (result!=0)
		{
			printf("FileList::DeleteFiles: unlink (%s) failed.\n", fullPath);
		}
	}
}

void FileList::SetCallback(FileListProgress *cb)
{
	callback=cb;
}

bool FileList::FixEndingSlash(char *str)
{
#ifdef _WIN32
	if (str[strlen(str)-1]!='/' && str[strlen(str)-1]!='\\')
	{
		strcat(str, "\\"); // Only \ works with system commands, used by AutopatcherClient
		return true;
	}
#else
	if (str[strlen(str)-1]!='\\' && str[strlen(str)-1]!='/')
	{
		strcat(str, "/"); // Only / works with Linux
		return true;
	}
#endif

	return false;
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
