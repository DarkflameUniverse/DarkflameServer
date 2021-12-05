#include "TelnetTransport.h"
#include "TCPInterface.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "LinuxStrings.h"

// #define _PRINTF_DEBUG

#define ECHO_INPUT

#ifdef _MSC_VER
#pragma warning( push )
#endif

TelnetTransport::TelnetTransport()
{
	tcpInterface=0;
	sendSuffix=0;
	sendPrefix=0;
}
TelnetTransport::~TelnetTransport()
{
	Stop();
	if (sendSuffix)
		rakFree(sendSuffix);
	if (sendPrefix)
		rakFree(sendPrefix);
}
bool TelnetTransport::Start(unsigned short port, bool serverMode)
{
    AutoAllocate();
	assert(serverMode);
	return tcpInterface->Start(port, 64);
}
void TelnetTransport::Stop(void)
{
	if (tcpInterface==0) return;
	tcpInterface->Stop();
	unsigned i;
	for (i=0; i < remoteClients.Size(); i++)
		delete remoteClients[i];
	remoteClients.Clear();
}
void TelnetTransport::Send(  SystemAddress systemAddress, const char *data,... )
{
	if (tcpInterface==0) return;

	char text[REMOTE_MAX_TEXT_INPUT];
	size_t prefixLength;
	if (sendPrefix)
	{
		strcpy(text, sendPrefix);
		prefixLength = strlen(sendPrefix);
	}
	else
	{
		text[0]=0;
		prefixLength=0;
	}
	va_list ap;
	va_start(ap, data);
	_vsnprintf(text+prefixLength, REMOTE_MAX_TEXT_INPUT-prefixLength, data, ap);
	va_end(ap);
	text[REMOTE_MAX_TEXT_INPUT-1]=0;

	if (sendSuffix)
	{
		size_t length = strlen(text);
		size_t availableChars = REMOTE_MAX_TEXT_INPUT-length-1;
		strncat(text, sendSuffix, availableChars);
	}

	tcpInterface->Send(text, (unsigned int) strlen(text), systemAddress);
}
void TelnetTransport::CloseConnection( SystemAddress systemAddress )
{
	tcpInterface->CloseConnection(systemAddress);
}
Packet* TelnetTransport::Receive( void )
{
	if (tcpInterface==0) return 0;
	Packet *p = tcpInterface->Receive();
	if (p==0)
		return 0;

	/*
	if (p->data[0]==255)
	{
		unsigned i;
		for (i=0; i < p->length; i++)
		{
			printf("%i ", p->data[i]);
		}
		printf("\n");
		tcpInterface->DeallocatePacket(p);
		return 0;
	}
	*/

	// 127 is delete - ignore that
	// 9 is tab
	// 27 is escape
	if (p->data[0]>=127 || p->data[0]==9 || p->data[0]==27)
	{
		tcpInterface->DeallocatePacket(p);
		return 0;
	}

	// Hack - I don't know what the hell this is about but cursor keys send 3 characters at a time.  I can block these
	//Up=27,91,65
	//Down=27,91,66
	//Right=27,91,67
	//Left=27,91,68
	if (p->length==3 && p->data[0]==27 && p->data[1]==91 && p->data[2]>=65 && p->data[2]<=68)
	{
		tcpInterface->DeallocatePacket(p);
		return 0;
	}

	// Get this guy's cursor buffer.  This is real bullcrap that I have to do this.
	unsigned i;
	TelnetClient *remoteClient=0;
	for (i=0; i < remoteClients.Size(); i++)
	{
		if (remoteClients[i]->systemAddress==p->systemAddress)
			remoteClient=remoteClients[i];
	}
	assert(remoteClient);
	if (remoteClient==0)
	{
		tcpInterface->DeallocatePacket(p);
		return 0;
	}


	// Echo
#ifdef ECHO_INPUT
	tcpInterface->Send((const char *)p->data, p->length, p->systemAddress);
#endif

	bool gotLine;
	// Process each character in turn
	for (i=0; i < p->length; i++)
	{

#ifdef ECHO_INPUT
		if (p->data[i]==8)
		{
			char spaceThenBack[2];
			spaceThenBack[0]=' ';
			spaceThenBack[1]=8;
			tcpInterface->Send((const char *)spaceThenBack, 2, p->systemAddress);
		}
#endif

		gotLine=ReassembleLine(remoteClient, p->data[i]);
		if (gotLine && remoteClient->textInput[0])
		{
			Packet *reassembledLine = (Packet*) rakMalloc(sizeof(Packet));
			reassembledLine->length=(unsigned int) strlen(remoteClient->textInput);
			assert(reassembledLine->length < REMOTE_MAX_TEXT_INPUT);
			reassembledLine->data= (unsigned char*) rakMalloc( reassembledLine->length+1 );
			memcpy(reassembledLine->data, remoteClient->textInput, reassembledLine->length);
#ifdef _PRINTF_DEBUG
			memset(remoteClient->textInput, 0, REMOTE_MAX_TEXT_INPUT);
#endif
			reassembledLine->data[reassembledLine->length]=0;
			reassembledLine->systemAddress=p->systemAddress;
			tcpInterface->DeallocatePacket(p);
			return reassembledLine;
		}
	}

	tcpInterface->DeallocatePacket(p);
	return 0;
}
void TelnetTransport::DeallocatePacket( Packet *packet )
{
	if (tcpInterface==0) return;
	rakFree(packet->data);
	rakFree(packet);
}
SystemAddress TelnetTransport::HasNewConnection(void)
{
	unsigned i;
	SystemAddress newConnection;
	newConnection = tcpInterface->HasNewConnection();
	// 03/16/06 Can't force the stupid windows telnet to use line mode or local echo so now I have to track all the remote players and their
	// input buffer
	if (newConnection != UNASSIGNED_SYSTEM_ADDRESS)
	{
		unsigned char command[10];
		// http://www.pcmicro.com/netfoss/RFC857.html
		// IAC WON'T ECHO
		command[0]=255; // IAC
		//command[1]=253; // WON'T
		command[1]=251; // WILL
		command[2]=1; // ECHO
		tcpInterface->Send((const char*)command, 3, newConnection);

		/*
		// Tell the other side to use line mode
		// http://www.faqs.org/rfcs/rfc1184.html
		// IAC DO LINEMODE
	//	command[0]=255; // IAC
	//	command[1]=252; // DO
	//	command[2]=34; // LINEMODE
	//	tcpInterface->Send((const char*)command, 3, newConnection);

	*/

		TelnetClient *remoteClient=0;
		for (i=0; i < remoteClients.Size(); i++)
		{
			if (remoteClients[i]->systemAddress==newConnection)
			{
				remoteClient=remoteClients[i];
				remoteClient->cursorPosition=0;
			}
		}

		if (remoteClient==0)
		{
			remoteClient=new TelnetClient;
			remoteClient->cursorPosition=0;
			remoteClient->systemAddress=newConnection;
#ifdef _PRINTF_DEBUG
			memset(remoteClient->textInput, 0, REMOTE_MAX_TEXT_INPUT);
#endif
		}

		remoteClients.Insert(remoteClient);
	}
	return newConnection;
}
SystemAddress TelnetTransport::HasLostConnection(void)
{
	SystemAddress systemAddress;
	unsigned i;
	systemAddress=tcpInterface->HasLostConnection();
	if (systemAddress!=UNASSIGNED_SYSTEM_ADDRESS)
	{
		for (i=0; i < remoteClients.Size(); i++)
		{
			if (remoteClients[i]->systemAddress==systemAddress)
			{
				delete remoteClients[i];
				remoteClients[i]=remoteClients[remoteClients.Size()-1];
				remoteClients.RemoveFromEnd();
			}
		}
	}
	return systemAddress;
}
CommandParserInterface* TelnetTransport::GetCommandParser(void)
{
	return 0;
}
void TelnetTransport::SetSendSuffix(const char *suffix)
{
	if (sendSuffix)
	{
		rakFree(sendSuffix);
		sendSuffix=0;
	}
	if (suffix)
	{
		sendSuffix = (char*) rakMalloc(strlen(suffix)+1);
		strcpy(sendSuffix, suffix);
	}
}
void TelnetTransport::SetSendPrefix(const char *prefix)
{
	if (sendPrefix)
	{
		rakFree(sendPrefix);
		sendPrefix=0;
	}
	if (prefix)
	{
		sendPrefix = (char*) rakMalloc(strlen(prefix)+1);
		strcpy(sendPrefix, prefix);
	}
}
void TelnetTransport::AutoAllocate(void)
{
	if (tcpInterface==0)
		tcpInterface=new TCPInterface;
}
bool TelnetTransport::ReassembleLine(TelnetTransport::TelnetClient* remoteClient, unsigned char c)
{
	if (c=='\n')
	{
		remoteClient->textInput[remoteClient->cursorPosition]=0;
		remoteClient->cursorPosition=0;
#ifdef _PRINTF_DEBUG
		printf("[Done] %s\n", remoteClient->textInput);
#endif
		return true;
	}
	else if (c==8) // backspace
	{
		if (remoteClient->cursorPosition>0)
		{
			remoteClient->textInput[--remoteClient->cursorPosition]=0;
#ifdef _PRINTF_DEBUG
			printf("[Back] %s\n", remoteClient->textInput);
#endif
		}
	}
	else if (c>=32 && c <127)
	{
		if (remoteClient->cursorPosition < REMOTE_MAX_TEXT_INPUT)
		{
			remoteClient->textInput[remoteClient->cursorPosition++]=c;
#ifdef _PRINTF_DEBUG
			printf("[Norm] %s\n", remoteClient->textInput);
#endif
		}
	}
	return false;
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
