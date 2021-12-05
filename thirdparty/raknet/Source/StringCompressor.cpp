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

#include "StringCompressor.h"
#include "DS_HuffmanEncodingTree.h"
#include "BitStream.h"
#include "RakString.h"
#include <assert.h>
#include <string.h>
#include <memory.h>
#if defined(_PS3)
#include "Console2Includes.h"
#endif

using namespace RakNet;

StringCompressor* StringCompressor::instance=0;
int StringCompressor::referenceCount=0;

void StringCompressor::AddReference(void)
{
	if (++referenceCount==1)
	{
		instance = new StringCompressor;
	}
}
void StringCompressor::RemoveReference(void)
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

StringCompressor* StringCompressor::Instance(void)
{
	return instance;
}

unsigned int englishCharacterFrequencies[ 256 ] =
{
	0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		722,
		0,
		0,
		2,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		11084,
		58,
		63,
		1,
		0,
		31,
		0,
		317,
		64,
		64,
		44,
		0,
		695,
		62,
		980,
		266,
		69,
		67,
		56,
		7,
		73,
		3,
		14,
		2,
		69,
		1,
		167,
		9,
		1,
		2,
		25,
		94,
		0,
		195,
		139,
		34,
		96,
		48,
		103,
		56,
		125,
		653,
		21,
		5,
		23,
		64,
		85,
		44,
		34,
		7,
		92,
		76,
		147,
		12,
		14,
		57,
		15,
		39,
		15,
		1,
		1,
		1,
		2,
		3,
		0,
		3611,
		845,
		1077,
		1884,
		5870,
		841,
		1057,
		2501,
		3212,
		164,
		531,
		2019,
		1330,
		3056,
		4037,
		848,
		47,
		2586,
		2919,
		4771,
		1707,
		535,
		1106,
		152,
		1243,
		100,
		0,
		2,
		0,
		10,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0
};

StringCompressor::StringCompressor()
{
	DataStructures::Map<int, HuffmanEncodingTree *>::IMPLEMENT_DEFAULT_COMPARISON();

	// Make a default tree immediately, since this is used for RPC possibly from multiple threads at the same time
	HuffmanEncodingTree *huffmanEncodingTree = new HuffmanEncodingTree;
	huffmanEncodingTree->GenerateFromFrequencyTable( englishCharacterFrequencies );

	huffmanEncodingTrees.Set(0, huffmanEncodingTree);
}
void StringCompressor::GenerateTreeFromStrings( unsigned char *input, unsigned inputLength, int languageID )
{
	HuffmanEncodingTree *huffmanEncodingTree;
	if (huffmanEncodingTrees.Has(languageID))
	{
		huffmanEncodingTree = huffmanEncodingTrees.Get(languageID);
		delete huffmanEncodingTree;
	}

	unsigned index;
	unsigned int frequencyTable[ 256 ];

	if ( inputLength == 0 )
		return ;

	// Zero out the frequency table
	memset( frequencyTable, 0, sizeof( frequencyTable ) );

	// Generate the frequency table from the strings
	for ( index = 0; index < inputLength; index++ )
		frequencyTable[ input[ index ] ] ++;

	// Build the tree
	huffmanEncodingTree = new HuffmanEncodingTree;
	huffmanEncodingTree->GenerateFromFrequencyTable( frequencyTable );
	huffmanEncodingTrees.Set(languageID, huffmanEncodingTree);
}

StringCompressor::~StringCompressor()
{
	for (unsigned i=0; i < huffmanEncodingTrees.Size(); i++)
		delete huffmanEncodingTrees[i];
}

void StringCompressor::EncodeString( const char *input, int maxCharsToWrite, RakNet::BitStream *output, int languageID )
{
	HuffmanEncodingTree *huffmanEncodingTree;
	if (huffmanEncodingTrees.Has(languageID)==false)
		return;
	huffmanEncodingTree=huffmanEncodingTrees.Get(languageID);

	if ( input == 0 )
	{
		output->WriteCompressed( (unsigned int) 0 );
		return ;
	}

	RakNet::BitStream encodedBitStream;

	unsigned int stringBitLength;

	int charsToWrite;

	if ( maxCharsToWrite<=0 || ( int ) strlen( input ) < maxCharsToWrite )
		charsToWrite = ( int ) strlen( input );
	else
		charsToWrite = maxCharsToWrite - 1;

	huffmanEncodingTree->EncodeArray( ( unsigned char* ) input, charsToWrite, &encodedBitStream );

	stringBitLength = (unsigned int) encodedBitStream.GetNumberOfBitsUsed();

	output->WriteCompressed( stringBitLength );

	output->WriteBits( encodedBitStream.GetData(), stringBitLength );
}

bool StringCompressor::DecodeString( char *output, int maxCharsToWrite, RakNet::BitStream *input, int languageID )
{
	HuffmanEncodingTree *huffmanEncodingTree;
	if (huffmanEncodingTrees.Has(languageID)==false)
		return false;
	if (maxCharsToWrite<=0)
		return false;
	huffmanEncodingTree=huffmanEncodingTrees.Get(languageID);

	unsigned int stringBitLength;
	int bytesInStream;

	output[ 0 ] = 0;

	if ( input->ReadCompressed( stringBitLength ) == false )
		return false;

	if ( (unsigned) input->GetNumberOfUnreadBits() < stringBitLength )
		return false;

	bytesInStream = huffmanEncodingTree->DecodeArray( input, stringBitLength, maxCharsToWrite, ( unsigned char* ) output );

	if ( bytesInStream < maxCharsToWrite )
		output[ bytesInStream ] = 0;
	else
		output[ maxCharsToWrite - 1 ] = 0;

	return true;
}
#ifdef _CSTRING_COMPRESSOR
void StringCompressor::EncodeString( const CString &input, int maxCharsToWrite, RakNet::BitStream *output )
{
	LPTSTR p = input;
	EncodeString(p, maxCharsToWrite*sizeof(TCHAR), output, languageID);
}
bool StringCompressor::DecodeString( CString &output, int maxCharsToWrite, RakNet::BitStream *input, int languageID )
{
	LPSTR p = output.GetBuffer(maxCharsToWrite*sizeof(TCHAR));
	DecodeString(p,maxCharsToWrite*sizeof(TCHAR), input, languageID);
	output.ReleaseBuffer(0)

}
#endif
#ifdef _STD_STRING_COMPRESSOR
void StringCompressor::EncodeString( const std::string &input, int maxCharsToWrite, RakNet::BitStream *output, int languageID )
{
	EncodeString(input.c_str(), maxCharsToWrite, output, languageID);
}
bool StringCompressor::DecodeString( std::string *output, int maxCharsToWrite, RakNet::BitStream *input, int languageID )
{
	if (maxCharsToWrite <= 0)
	{
		output->clear();
		return true;
	}

	char *destinationBlock;
	bool out;

#ifndef _XBOX360
	if (maxCharsToWrite < MAX_ALLOCA_STACK_ALLOCATION)
	{
		destinationBlock = (char*) alloca(maxCharsToWrite);
		out=DecodeString(destinationBlock, maxCharsToWrite, input, languageID);
		*output=destinationBlock;
	}
	else
#endif
	{
		destinationBlock = (char*) rakMalloc( maxCharsToWrite );
		out=DecodeString(destinationBlock, maxCharsToWrite, input, languageID);
		*output=destinationBlock;
		rakFree(destinationBlock);
	}

	return out;
}
#endif
void StringCompressor::EncodeString( const RakString *input, int maxCharsToWrite, RakNet::BitStream *output, int languageID )
{
	EncodeString(input->C_String(), maxCharsToWrite, output, languageID);
}
bool StringCompressor::DecodeString( RakString *output, int maxCharsToWrite, RakNet::BitStream *input, int languageID )
{
	if (maxCharsToWrite <= 0)
	{
		output->Clear();
		return true;
	}

	char *destinationBlock;
	bool out;

#ifndef _XBOX360
	if (maxCharsToWrite < MAX_ALLOCA_STACK_ALLOCATION)
	{
		destinationBlock = (char*) alloca(maxCharsToWrite);
		out=DecodeString(destinationBlock, maxCharsToWrite, input, languageID);
		*output=destinationBlock;
	}
	else
#endif
	{
		destinationBlock = (char*) rakMalloc( maxCharsToWrite );
		out=DecodeString(destinationBlock, maxCharsToWrite, input, languageID);
		*output=destinationBlock;
		rakFree(destinationBlock);
	}

	return out;
}
