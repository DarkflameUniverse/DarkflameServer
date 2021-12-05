#include "LightweightDatabaseServer.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "StringCompressor.h"
#include "RakPeerInterface.h"
#include "TableSerializer.h"
#include "RakAssert.h"
#include "GetTime.h"
#include "Rand.h"

static const int SEND_PING_INTERVAL=15000;
static const int DROP_SERVER_INTERVAL=75000;

#ifdef _MSC_VER
#pragma warning( push )
#endif

int LightweightDatabaseServer::DatabaseTableComp( char* const &key1, char* const &key2 )
{
	return strcmp(key1, key2);
}

LightweightDatabaseServer::LightweightDatabaseServer()
{

}
LightweightDatabaseServer::~LightweightDatabaseServer()
{
	Clear();
}
DataStructures::Table *LightweightDatabaseServer::GetTable(char *tableName)
{
	if (database.Has(tableName))
		return &(database.Get(tableName)->table);
	return 0;
}
DataStructures::Page<unsigned, DataStructures::Table::Row*, _TABLE_BPLUS_TREE_ORDER> *LightweightDatabaseServer::GetTableRows(char *tableName)
{
	if (database.Has(tableName))
		database.Get(tableName)->table.GetRows().GetListHead();
	return 0;
}
DataStructures::Table* LightweightDatabaseServer::AddTable(char *tableName,
														   bool allowRemoteQuery,
														   bool allowRemoteUpdate,
														   bool allowRemoteRemove,
														   const char *queryPassword,
														   const char *updatePassword,
														   const char *removePassword,
														   bool oneRowPerSystemAddress,
														   bool onlyUpdateOwnRows,
														   bool removeRowOnPingFailure,
														   bool removeRowOnDisconnect,
														   bool autogenerateRowIDs)
{
	if (tableName==0 || tableName[0]==0)
		return 0;
	if (database.Has(tableName))
		return 0;
	DatabaseTable *databaseTable = new DatabaseTable;

	strncpy(databaseTable->tableName, tableName, _SIMPLE_DATABASE_TABLE_NAME_LENGTH-1);
	databaseTable->tableName[_SIMPLE_DATABASE_TABLE_NAME_LENGTH-1]=0;

	if (allowRemoteUpdate)
	{
		strncpy(databaseTable->updatePassword, updatePassword, _SIMPLE_DATABASE_PASSWORD_LENGTH-1);
		databaseTable->updatePassword[_SIMPLE_DATABASE_PASSWORD_LENGTH-1]=0;
	}
	else
		databaseTable->updatePassword[0]=0;

	if (allowRemoteQuery)
	{
		strncpy(databaseTable->queryPassword, queryPassword, _SIMPLE_DATABASE_PASSWORD_LENGTH-1);
		databaseTable->queryPassword[_SIMPLE_DATABASE_PASSWORD_LENGTH-1]=0;
	}
	else
		databaseTable->queryPassword[0]=0;	

	if (allowRemoteRemove)
	{
		strncpy(databaseTable->removePassword, removePassword, _SIMPLE_DATABASE_PASSWORD_LENGTH-1);
		databaseTable->removePassword[_SIMPLE_DATABASE_PASSWORD_LENGTH-1]=0;
	}
	else
		databaseTable->removePassword[0]=0;

	if (allowRemoteUpdate)
	{
		databaseTable->allowRemoteUpdate=true;
		databaseTable->oneRowPerSystemAddress=oneRowPerSystemAddress;
		databaseTable->onlyUpdateOwnRows=onlyUpdateOwnRows;
		databaseTable->removeRowOnPingFailure=removeRowOnPingFailure;
		databaseTable->removeRowOnDisconnect=removeRowOnDisconnect;
	}
	else
	{
		// All these parameters are related to IP tracking, which is not done if remote updates are not allowed
		databaseTable->allowRemoteUpdate=true;
		databaseTable->oneRowPerSystemAddress=false;
		databaseTable->onlyUpdateOwnRows=false;
		databaseTable->removeRowOnPingFailure=false;
		databaseTable->removeRowOnDisconnect=false;
	}
	
	databaseTable->nextRowId=0;
	databaseTable->nextRowPingCheck=0;
	
	databaseTable->autogenerateRowIDs=autogenerateRowIDs;
	databaseTable->allowRemoteRemove=allowRemoteRemove;
	databaseTable->allowRemoteQuery=allowRemoteQuery;
	
	database.SetNew(databaseTable->tableName, databaseTable);

	if ( oneRowPerSystemAddress || onlyUpdateOwnRows || removeRowOnPingFailure || removeRowOnDisconnect)
		databaseTable->SystemAddressColumnIndex=databaseTable->table.AddColumn(SYSTEM_ID_COLUMN_NAME, DataStructures::Table::BINARY);
	else
		databaseTable->SystemAddressColumnIndex=(unsigned) -1;
	if (databaseTable->removeRowOnPingFailure)
	{
		databaseTable->lastPingResponseColumnIndex=databaseTable->table.AddColumn(LAST_PING_RESPONSE_COLUMN_NAME, DataStructures::Table::NUMERIC);
		databaseTable->nextPingSendColumnIndex=databaseTable->table.AddColumn(NEXT_PING_SEND_COLUMN_NAME, DataStructures::Table::NUMERIC);
	}
	else
	{
		databaseTable->lastPingResponseColumnIndex=(unsigned) -1;
		databaseTable->nextPingSendColumnIndex=(unsigned) -1;
	}

	return &(databaseTable->table);
}
bool LightweightDatabaseServer::RemoveTable(char *tableName)
{
	LightweightDatabaseServer::DatabaseTable *databaseTable;
	databaseTable = database.Get(tableName);
	if (databaseTable==0)
		return false;
	// Be sure to call Delete on database before I do the actual pointer deletion since the key won't be valid after that time.
	database.Delete(tableName);
	databaseTable->table.Clear();
	delete databaseTable;
	return true;
}
void LightweightDatabaseServer::Clear(void)
{
	unsigned i;

	for (i=0; i < database.Size(); i++)
	{
		database[i]->table.Clear();
		delete database[i];
	}

	database.Clear();
}
unsigned LightweightDatabaseServer::GetAndIncrementRowID(char *tableName)
{
	LightweightDatabaseServer::DatabaseTable *databaseTable;
	databaseTable = database.Get(tableName);
	RakAssert(databaseTable);
	RakAssert(databaseTable->autogenerateRowIDs==true);
	return ++(databaseTable->nextRowId) - 1;
}

void LightweightDatabaseServer::OnAttach(RakPeerInterface *peer)
{
	(void) peer;
}
void LightweightDatabaseServer::Update(RakPeerInterface *peer)
{
	RakNetTime time=0;
	DatabaseTable *databaseTable;
	DataStructures::Page<unsigned, DataStructures::Table::Row*, _TABLE_BPLUS_TREE_ORDER> *cur;
	unsigned i,j;
	DataStructures::Table::Row* row;
	DataStructures::List<unsigned> removeList;
	SystemAddress systemAddress;

	// periodic ping if removing system that do not respond to pings.
	for (i=0; i < database.Size(); i++)
	{
		databaseTable=database[i];

		if (databaseTable->removeRowOnPingFailure)
		{
			// Reading the time is slow - only do it once if necessary.
			if (time==0)
				 time = RakNet::GetTime();

			if (databaseTable->nextRowPingCheck < time)
			{
				databaseTable->nextRowPingCheck=time+1000+(randomMT()%1000);
				DataStructures::BPlusTree<unsigned, DataStructures::Table::Row*, _TABLE_BPLUS_TREE_ORDER> &rows = databaseTable->table.GetRows();
				cur = rows.GetListHead();
				while (cur)
				{
					// Mark dropped entities
					for (j=0; j < (unsigned)cur->size; j++)
					{
						row = cur->data[j];
						if (time - (unsigned int) row->cells[databaseTable->lastPingResponseColumnIndex]->i > (unsigned int) DROP_SERVER_INTERVAL)
							removeList.Insert(cur->keys[j]);
					}
					cur=cur->next;
				}

				// Remove dropped entities
				for (j=0; j < removeList.Size(); j++)
					databaseTable->table.RemoveRow(removeList[i]);
				removeList.Clear(true);

				cur = rows.GetListHead();
				// Ping remaining entities if they are not connected.  If they are connected just increase the ping interval.
				while (cur)
				{
					for (j=0; j < (unsigned)cur->size; j++)
					{
						row = cur->data[j];
						if (row->cells[databaseTable->nextPingSendColumnIndex]->i < (int) time)
						{
							row->cells[databaseTable->SystemAddressColumnIndex]->Get((char*)&systemAddress, 0);
							if (peer->IsConnected(systemAddress)==false)
							{
								peer->Ping(systemAddress.ToString(false), systemAddress.port, false);
							}
							else
							{
								// Consider the fact that they are connected to be a ping response
								row->cells[databaseTable->lastPingResponseColumnIndex]->i=time;
							}
							
							row->cells[databaseTable->nextPingSendColumnIndex]->i=time+SEND_PING_INTERVAL+(randomMT()%1000);
						}
					}
					cur=cur->next;
				}
			}
		}
	}
}
PluginReceiveResult LightweightDatabaseServer::OnReceive(RakPeerInterface *peer, Packet *packet)
{
	switch (packet->data[0]) 
	{
	case ID_DATABASE_QUERY_REQUEST:
		OnQueryRequest(peer, packet);
		return RR_STOP_PROCESSING_AND_DEALLOCATE;
	case ID_DATABASE_UPDATE_ROW:
		OnUpdateRow(peer, packet);
		return RR_STOP_PROCESSING_AND_DEALLOCATE;
	case ID_DATABASE_REMOVE_ROW:
		OnRemoveRow(peer, packet);
		return RR_STOP_PROCESSING_AND_DEALLOCATE;
	case ID_DISCONNECTION_NOTIFICATION:
	case ID_CONNECTION_LOST:
		RemoveRowsFromIP(packet->systemAddress);
		return RR_CONTINUE_PROCESSING;
	case ID_PONG:
		OnPong(peer, packet);
		return RR_CONTINUE_PROCESSING;
	}
	return RR_CONTINUE_PROCESSING;
}
void LightweightDatabaseServer::OnShutdown(RakPeerInterface *peer)
{
	(void) peer;
}
void LightweightDatabaseServer::OnCloseConnection(RakPeerInterface *peer, SystemAddress systemAddress)
{
	(void) peer;
	RemoveRowsFromIP(systemAddress);
}
void LightweightDatabaseServer::OnQueryRequest(RakPeerInterface *peer, Packet *packet)
{
	RakNet::BitStream inBitstream(packet->data, packet->length, false);
	LightweightDatabaseServer::DatabaseTable *databaseTable = DeserializeClientHeader(&inBitstream, peer, packet, 0);
	if (databaseTable==0)
		return;
	if (databaseTable->allowRemoteQuery==false)
		return;
	unsigned char numColumnSubset;
	RakNet::BitStream outBitstream;
	unsigned i;
	if (inBitstream.Read(numColumnSubset)==false)
		return;
	unsigned char columnName[256];
	unsigned columnIndicesSubset[256];
	unsigned columnIndicesCount;
	for (i=0,columnIndicesCount=0; i < numColumnSubset; i++)
	{
		stringCompressor->DecodeString((char*)columnName, 256, &inBitstream);
		unsigned colIndex = databaseTable->table.ColumnIndex((char*)columnName);
		if (colIndex!=(unsigned)-1)
			columnIndicesSubset[columnIndicesCount++]=colIndex;
	}
	unsigned char numNetworkedFilters;
	if (inBitstream.Read(numNetworkedFilters)==false)
		return;
	DatabaseFilter networkedFilters[256];
	for (i=0; i < numNetworkedFilters; i++)
	{
		if (networkedFilters[i].Deserialize(&inBitstream)==false)
			return;
	}

	unsigned rowIds[256];
	unsigned char numRowIDs;
	if (inBitstream.Read(numRowIDs)==false)
		return;
	for (i=0; i < numRowIDs; i++)
		inBitstream.Read(rowIds[i]);

	// Convert the safer and more robust networked database filter to the more efficient form the table actually uses.
	DataStructures::Table::FilterQuery tableFilters[256];
	unsigned numTableFilters=0;
	for (i=0; i < numNetworkedFilters; i++)
	{	
		tableFilters[numTableFilters].columnIndex=databaseTable->table.ColumnIndex(networkedFilters[i].columnName);
		if (tableFilters[numTableFilters].columnIndex==(unsigned)-1)
			continue;
		if (networkedFilters[i].columnType!=databaseTable->table.GetColumns()[tableFilters[numTableFilters].columnIndex].columnType)
			continue;
		tableFilters[numTableFilters].operation=networkedFilters[i].operation;
		// It's important that I store a pointer to the class here or the destructor of the class will deallocate the cell twice
		tableFilters[numTableFilters++].cellValue=&(networkedFilters[i].cellValue);
	}
    
	DataStructures::Table queryResult;
	databaseTable->table.QueryTable(columnIndicesSubset, columnIndicesCount, tableFilters, numTableFilters, rowIds, numRowIDs, &queryResult);
	outBitstream.Write((MessageID)ID_DATABASE_QUERY_REPLY);
	TableSerializer::SerializeTable(&queryResult, &outBitstream);
	peer->Send(&outBitstream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);			

}
void LightweightDatabaseServer::OnUpdateRow(RakPeerInterface *peer, Packet *packet)
{
	RakNet::BitStream inBitstream(packet->data, packet->length, false);
	LightweightDatabaseServer::DatabaseTable *databaseTable = DeserializeClientHeader(&inBitstream, peer, packet, 1);
	if (databaseTable==0)
	{
		printf("ERROR: LightweightDatabaseServer::OnUpdateRow databaseTable==0\n");
		return;
	}
	if (databaseTable->allowRemoteUpdate==false)
	{
		printf("Warning: LightweightDatabaseServer::OnUpdateRow databaseTable->allowRemoteUpdate==false\n");
		return;
	}
	unsigned char updateMode;
	bool hasRowId=false;
	unsigned rowId;
	unsigned i;
	DataStructures::Table::Row *row;
	inBitstream.Read(updateMode);
	inBitstream.Read(hasRowId);
	if (hasRowId)
		inBitstream.Read(rowId);
	else
		rowId=(unsigned) -1; // Not used here but remove the debugging check
	unsigned char numCellUpdates;
	if (inBitstream.Read(numCellUpdates)==false)
		return;
	// Read the updates for the row
	DatabaseCellUpdate cellUpdates[256];
	for (i=0; i < numCellUpdates; i++)
	{
		if (cellUpdates[i].Deserialize(&inBitstream)==false)
		{
			printf("ERROR: LightweightDatabaseServer::OnUpdateRow cellUpdates deserialize failed i=%i numCellUpdates=%i\n",i,numCellUpdates);
			return;
		}
	}

	if ((RowUpdateMode)updateMode==RUM_UPDATE_EXISTING_ROW)
	{
		if (hasRowId==false)
		{
			unsigned rowKey;
			row = GetRowFromIP(databaseTable, packet->systemAddress, &rowKey);
			if (row==0)
				printf("ERROR: LightweightDatabaseServer::OnUpdateRow updateMode==RUM_UPDATE_EXISTING_ROW hasRowId==false");
		}
		else
		{
	
			row = databaseTable->table.GetRowByID(rowId);
			if (row==0 || (databaseTable->onlyUpdateOwnRows && RowHasIP(row, packet->systemAddress, databaseTable->SystemAddressColumnIndex)==false))
			{
				if (row==0)
					printf("ERROR: LightweightDatabaseServer::OnUpdateRow row = databaseTable->table.GetRowByID(rowId); row==0\n");
				else
					printf("ERROR: LightweightDatabaseServer::OnUpdateRow row = databaseTable->table.GetRowByID(rowId); databaseTable->onlyUpdateOwnRows && RowHasIP\n");

				return; // You can't update some other system's row
			}
		}
	}
	else if ((RowUpdateMode)updateMode==RUM_UPDATE_OR_ADD_ROW)
	{
		if (hasRowId)
			row = databaseTable->table.GetRowByID(rowId);
		else
		{
			unsigned rowKey;
			row = GetRowFromIP(databaseTable, packet->systemAddress, &rowKey);
		}

		if (row==0)
		{
			row=AddRow(databaseTable, packet->systemAddress, hasRowId, rowId);
			if (row==0)
			{
				printf("ERROR: LightweightDatabaseServer::OnUpdateRow updateMode==RUM_UPDATE_OR_ADD_ROW; row=AddRow; row==0\n");
				return;
			}
		}
		else
		{
			// Existing row
			if (databaseTable->onlyUpdateOwnRows && RowHasIP(row, packet->systemAddress, databaseTable->SystemAddressColumnIndex)==false)
			{
				SystemAddress sysAddr;
				memcpy(&sysAddr, row->cells[databaseTable->SystemAddressColumnIndex]->c, sizeof(SystemAddress));

				printf("ERROR: LightweightDatabaseServer::OnUpdateRow updateMode==RUM_UPDATE_OR_ADD_ROW; databaseTable->onlyUpdateOwnRows && RowHasIP. packet->systemAddress=%s sysAddr=%s\n",
					packet->systemAddress.ToString(true), sysAddr.ToString(true));

				return; // You can't update some other system's row
			}
		}	
	}
	else
	{
		RakAssert((RowUpdateMode)updateMode==RUM_ADD_NEW_ROW);

		row=AddRow(databaseTable, packet->systemAddress, hasRowId, rowId);
		if (row==0)
		{
			printf("ERROR: LightweightDatabaseServer::OnUpdateRow updateMode==RUM_ADD_NEW_ROW; row==0\n");
			return;
		}
	}

	unsigned columnIndex;
	for (i=0; i < numCellUpdates; i++)
	{
		columnIndex=databaseTable->table.ColumnIndex(cellUpdates[i].columnName);
		RakAssert(columnIndex!=(unsigned)-1); // Unknown column name
		if (columnIndex!=(unsigned)-1 && columnIndex!=databaseTable->lastPingResponseColumnIndex && columnIndex!=databaseTable->nextPingSendColumnIndex && columnIndex!=databaseTable->SystemAddressColumnIndex)
		{
			if (cellUpdates[i].cellValue.isEmpty)
				row->cells[columnIndex]->Clear();
			else if (cellUpdates[i].columnType==databaseTable->table.GetColumnType(columnIndex))
			{
				if (cellUpdates[i].columnType==DataStructures::Table::NUMERIC)
				{
					row->UpdateCell(columnIndex, cellUpdates[i].cellValue.i);
				}
				else if (cellUpdates[i].columnType==DataStructures::Table::BINARY)
				{
					row->UpdateCell(columnIndex, cellUpdates[i].cellValue.i, cellUpdates[i].cellValue.c);
				}
				else
				{
					RakAssert(cellUpdates[i].columnType==DataStructures::Table::STRING);
					row->UpdateCell(columnIndex, cellUpdates[i].cellValue.c);
				}
			}
		}
	}
}
void LightweightDatabaseServer::OnRemoveRow(RakPeerInterface *peer, Packet *packet)
{
	RakNet::BitStream inBitstream(packet->data, packet->length, false);
	LightweightDatabaseServer::DatabaseTable *databaseTable = DeserializeClientHeader(&inBitstream, peer, packet, 0);
	if (databaseTable==0)
		return;
	if (databaseTable->allowRemoteRemove==false)
		return;
	unsigned rowId;
	inBitstream.Read(rowId);
	databaseTable->table.RemoveRow(rowId);
}
void LightweightDatabaseServer::OnPong(RakPeerInterface *peer, Packet *packet)
{
	(void) peer;

	unsigned databaseIndex;
	DatabaseTable *databaseTable;
	unsigned curIndex;
	SystemAddress systemAddress;
	RakNetTime time=0;
	for (databaseIndex=0; databaseIndex < database.Size(); databaseIndex++)
	{
		databaseTable=database[databaseIndex];
		if (databaseTable->removeRowOnPingFailure)
		{
			if (time==0)
				time=RakNet::GetTime();

			DataStructures::BPlusTree<unsigned, DataStructures::Table::Row*, _TABLE_BPLUS_TREE_ORDER> &rows = databaseTable->table.GetRows();
			DataStructures::Page<unsigned, DataStructures::Table::Row*, _TABLE_BPLUS_TREE_ORDER> *cur = rows.GetListHead();
			
			while (cur)
			{
				for (curIndex=0; curIndex < (unsigned) cur->size; curIndex++)
				{
					cur->data[curIndex]->cells[databaseTable->SystemAddressColumnIndex]->Get((char*)&systemAddress,0);
					if (systemAddress==packet->systemAddress)
					{
						cur->data[curIndex]->cells[databaseTable->lastPingResponseColumnIndex]->i=time;
					}
				}
				cur=cur->next;
			}
		}
	}
}

LightweightDatabaseServer::DatabaseTable * LightweightDatabaseServer::DeserializeClientHeader(RakNet::BitStream *inBitstream, RakPeerInterface *peer, Packet *packet, int mode)
{
	RakNet::BitStream outBitstream;
	bool hasPassword=false;
	char password[_SIMPLE_DATABASE_PASSWORD_LENGTH];
	inBitstream->IgnoreBits(8);
	char tableName[_SIMPLE_DATABASE_TABLE_NAME_LENGTH];
	stringCompressor->DecodeString(tableName, _SIMPLE_DATABASE_TABLE_NAME_LENGTH, inBitstream);
	DatabaseTable *databaseTable = database.Get(tableName);
	if (databaseTable==0)
	{
		outBitstream.Write((MessageID)ID_DATABASE_UNKNOWN_TABLE);
		peer->Send(&outBitstream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
		return 0;
	}
	const char *dbPass;
	if (mode==0)
		dbPass=databaseTable->queryPassword;
	else if (mode==1)
		dbPass=databaseTable->updatePassword;
	else
		dbPass=databaseTable->removePassword;

	inBitstream->Read(hasPassword);
	if (hasPassword)
	{
		if (stringCompressor->DecodeString(password, _SIMPLE_DATABASE_PASSWORD_LENGTH, inBitstream)==false)
			return 0;
		if (databaseTable->queryPassword[0] && strcmp(password, dbPass)!=0)
		{
			outBitstream.Write((MessageID)ID_DATABASE_INCORRECT_PASSWORD);
			peer->Send(&outBitstream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
			// Short ban to prevent brute force password attempts
			peer->AddToBanList(packet->systemAddress.ToString(false), 1000);
			// Don't send a disconnection notification so it closes the connection right away.
			peer->CloseConnection(packet->systemAddress, false, 0);			
			return 0;
		}
	}
	else if (dbPass[0])
	{
		outBitstream.Write((MessageID)ID_DATABASE_INCORRECT_PASSWORD);
		peer->Send(&outBitstream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
		return 0;
	}

	return databaseTable;
}

DataStructures::Table::Row * LightweightDatabaseServer::GetRowFromIP(DatabaseTable *databaseTable, SystemAddress systemAddress, unsigned *rowKey)
{
	DataStructures::BPlusTree<unsigned, DataStructures::Table::Row*, _TABLE_BPLUS_TREE_ORDER> &rows = databaseTable->table.GetRows();
	DataStructures::Page<unsigned, DataStructures::Table::Row*, _TABLE_BPLUS_TREE_ORDER> *cur = rows.GetListHead();
	DataStructures::Table::Row* row;
	unsigned i;
	while (cur)
	{
		for (i=0; i < (unsigned)cur->size; i++)
		{
			row = cur->data[i];
			if (RowHasIP(row, systemAddress, databaseTable->SystemAddressColumnIndex ))
			{
				if (rowKey)
					*rowKey=cur->keys[i];
				return row;
			}
		}
		cur=cur->next;
	}
	return 0;
}
bool LightweightDatabaseServer::RowHasIP(DataStructures::Table::Row *row, SystemAddress systemAddress, unsigned SystemAddressColumnIndex)
{
	SystemAddress sysAddr;
	memcpy(&sysAddr, row->cells[SystemAddressColumnIndex]->c, sizeof(SystemAddress));
	return sysAddr==systemAddress;

	// Doesn't work in release for some reason
	//RakAssert(row->cells[SystemAddressColumnIndex]->isEmpty==false);
	//if (memcmp(row->cells[SystemAddressColumnIndex]->c, &systemAddress, sizeof(SystemAddress))==0)
	//	return true;
	// return false;
}
DataStructures::Table::Row * LightweightDatabaseServer::AddRow(LightweightDatabaseServer::DatabaseTable *databaseTable, SystemAddress systemAddress, bool hasRowId, unsigned rowId)
{
	DataStructures::Table::Row *row;
	if (databaseTable->oneRowPerSystemAddress && GetRowFromIP(databaseTable, systemAddress, 0))
		return 0; // This system already has a row.

	if (databaseTable->autogenerateRowIDs==false)
	{
		// For a new row:
		// rowID required but not specified OR
		// rowId specified but already in the table
		// Then exit
		if (hasRowId==false || databaseTable->table.GetRowByID(rowId))
			return 0; 
	}
	else
		rowId=databaseTable->nextRowId++;

	// Add new row
	row = databaseTable->table.AddRow(rowId);

	// Set IP and last update time
	if ( databaseTable->oneRowPerSystemAddress || databaseTable->onlyUpdateOwnRows || databaseTable->removeRowOnPingFailure || databaseTable->removeRowOnDisconnect)
		row->cells[databaseTable->SystemAddressColumnIndex]->Set((char*)&systemAddress, sizeof(SystemAddress));
	if (databaseTable->removeRowOnPingFailure)
	{
		RakNetTime time = RakNet::GetTime();
		row->cells[databaseTable->lastPingResponseColumnIndex]->Set(time);
		row->cells[databaseTable->nextPingSendColumnIndex]->Set(time+SEND_PING_INTERVAL);
	}

	return row;
}
void LightweightDatabaseServer::RemoveRowsFromIP(SystemAddress systemAddress)
{
	// Remove rows for tables that do so on a system disconnect
	DatabaseTable *databaseTable;
	DataStructures::List<unsigned> removeList;
	DataStructures::Page<unsigned, DataStructures::Table::Row*, _TABLE_BPLUS_TREE_ORDER> *cur;
	unsigned i,j;
	for (i=0; i < database.Size(); i++)
	{
		databaseTable=database[i];
		if (databaseTable->removeRowOnDisconnect)
		{
			DataStructures::BPlusTree<unsigned, DataStructures::Table::Row*, _TABLE_BPLUS_TREE_ORDER> &rows = databaseTable->table.GetRows();
			cur = rows.GetListHead();
			while (cur)
			{
				// Mark dropped entities
				for (j=0; j < (unsigned)cur->size; j++)
				{
					if (RowHasIP(cur->data[j], systemAddress, databaseTable->SystemAddressColumnIndex))
						removeList.Insert(cur->keys[j]);
				}
				cur=cur->next;
			}

			for (j=0; j < removeList.Size(); j++)
				databaseTable->table.RemoveRow(removeList[j]);
			removeList.Clear(true);
		}
	}	
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif
