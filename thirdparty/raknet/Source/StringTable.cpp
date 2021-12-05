#include "StringTable.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "BitStream.h"
#include "StringCompressor.h"
using namespace RakNet;

StringTable* StringTable::instance=0;
int StringTable::referenceCount=0;


int StrAndBoolComp( char *const &key, const StrAndBool &data )
{
	return strcmp(key,(const char*)data.str);
}

StringTable::StringTable()
{
	
}

StringTable::~StringTable()
{
	unsigned i;
	for (i=0; i < orderedStringList.Size(); i++)
	{
		if (orderedStringList[i].b)
			rakFree(orderedStringList[i].str);
	}
}

void StringTable::AddReference(void)
{
	if (++referenceCount==1)
	{
		instance = new StringTable;
	}
}
void StringTable::RemoveReference(void)
{
	assert(referenceCount > 0);

	if (referenceCount > 0)
	{
		if (--referenceCount==0)
		{
			delete instance;
			instance=0;
		}
	}
}

StringTable* StringTable::Instance(void)
{
	return instance;
}

void StringTable::AddString(const char *str, bool copyString)
{
	StrAndBool sab;
	sab.b=copyString;
	if (copyString)
	{
		sab.str = (char*) rakMalloc( strlen(str)+1 );
		strcpy(sab.str, str);
	}
	else
	{
		sab.str=(char*)str;
	}

	// If it asserts inside here you are adding duplicate strings.
	if (orderedStringList.Insert(sab.str,sab, true)!=(unsigned)-1)
	{
		if (copyString)
			delete sab.str;
	}

	// If this assert hits you need to increase the range of StringTableType
	assert(orderedStringList.Size() < (StringTableType)-1);	
	
}
void StringTable::EncodeString( const char *input, int maxCharsToWrite, RakNet::BitStream *output )
{
	unsigned index;
	bool objectExists;
	// This is fast because the list is kept ordered.
	index=orderedStringList.GetIndexFromKey((char*)input, &objectExists);
	if (objectExists)
	{
		output->Write(true);
		output->Write((StringTableType)index);
	}
	else
	{
		LogStringNotFound(input);
		output->Write(false);
		stringCompressor->EncodeString(input, maxCharsToWrite, output);
	}
}

bool StringTable::DecodeString( char *output, int maxCharsToWrite, RakNet::BitStream *input )
{
	bool hasIndex;
	assert(maxCharsToWrite>0);

	if (maxCharsToWrite==0)
		return false;
	if (!input->Read(hasIndex))
		return false;
	if (hasIndex==false)
	{
		stringCompressor->DecodeString(output, maxCharsToWrite, input);
	}
	else
	{
		StringTableType index;
		if (!input->Read(index))
			return false;
		if (index >= orderedStringList.Size())
		{
#ifdef _DEBUG
			// Critical error - got a string index out of range, which means AddString was called more times on the remote system than on this system.
			// All systems must call AddString the same number of types, with the same strings in the same order.
			assert(0);
#endif
			return false;
		}
		
		strncpy(output, orderedStringList[index].str, maxCharsToWrite);
		output[maxCharsToWrite-1]=0;
	}

	return true;
}
void StringTable::LogStringNotFound(const char *strName)
{
	(void) strName;

#ifdef _DEBUG
	printf("Efficiency Warning! Unregistered String %s sent to StringTable.\n", strName);
#endif
}
