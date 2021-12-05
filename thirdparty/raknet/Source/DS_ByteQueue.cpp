#include "DS_ByteQueue.h"
#include <string.h> // Memmove
#include <stdlib.h> // realloc
#include <stdio.h>


using namespace DataStructures;

ByteQueue::ByteQueue()
{
	readOffset=writeOffset=lengthAllocated=0;
	data=0;
}
ByteQueue::~ByteQueue()
{
	Clear();
	

}
void ByteQueue::WriteBytes(const char *in, unsigned length)
{
	unsigned bytesWritten;
	bytesWritten=GetBytesWritten();
	if (lengthAllocated==0 || length > lengthAllocated-bytesWritten-1)
	{
		unsigned oldLengthAllocated=lengthAllocated;
		unsigned newAmountToAllocate=length*2;
		if (newAmountToAllocate<256)
			newAmountToAllocate=256;
		lengthAllocated=lengthAllocated + newAmountToAllocate;
		data=(char*)rakRealloc(data, lengthAllocated);
		if (writeOffset < readOffset)
		{
			if (writeOffset <= newAmountToAllocate)
			{
				memcpy(data + oldLengthAllocated, data, writeOffset);
				writeOffset=readOffset+bytesWritten;
			}
			else
			{
				memcpy(data + oldLengthAllocated, data, newAmountToAllocate);
				memmove(data, data+newAmountToAllocate, writeOffset-newAmountToAllocate);
                writeOffset-=newAmountToAllocate;
			}
		}
	}

	if (length <= lengthAllocated-writeOffset)
		memcpy(data+writeOffset, in, length);
	else
	{
		// Wrap
		memcpy(data+writeOffset, in, lengthAllocated-writeOffset);
		memcpy(data, in+(lengthAllocated-writeOffset), length-(lengthAllocated-writeOffset));
	}
	writeOffset=(writeOffset+length) % lengthAllocated;
}
bool ByteQueue::ReadBytes(char *out, unsigned length, bool peek)
{
	if (GetBytesWritten() < length)
		return false;

	if (length <= lengthAllocated-readOffset)
		memcpy(out, data+readOffset, length);
	else
	{
		// Wrap
		memcpy(out, data+readOffset, lengthAllocated-readOffset);
		memcpy(out+(lengthAllocated-readOffset), data, length-(lengthAllocated-readOffset));
	}

	if (peek==false)
		IncrementReadOffset(length);
		
	return true;
}
void ByteQueue::Clear(void)
{
	if (lengthAllocated)
		rakFree(data);
	readOffset=writeOffset=lengthAllocated=0;
	data=0;
}
unsigned ByteQueue::GetBytesWritten(void) const
{
	if (writeOffset>=readOffset)
		return writeOffset-readOffset;
	else
		return (writeOffset-1)+(lengthAllocated-readOffset);
}
void ByteQueue::IncrementReadOffset(unsigned length)
{
	readOffset=(readOffset+length) % lengthAllocated;
}
void ByteQueue::Print(void)
{
	unsigned i;
	for (i=readOffset; i!=writeOffset; i++)
		printf("%i ", data[i]);
	printf("\n");
}
