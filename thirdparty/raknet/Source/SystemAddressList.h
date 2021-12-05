/// \file
/// \brief Just a class to hold a list of systems
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

#ifndef __SYSTEM_ID_LIST_H
#define __SYSTEM_ID_LIST_H

#include "RakNetTypes.h"
#include "DS_OrderedList.h"

class SystemAddressList : public RakNet::RakMemoryOverride
{
public:
	SystemAddressList();
	SystemAddressList(SystemAddress system);
	void AddSystem(SystemAddress system);
	void RandomizeOrder(void);
	void Serialize(RakNet::BitStream *out);
	bool Deserialize(RakNet::BitStream *in);
	bool Save(const char *filename);
	bool Load(const char *filename);
	void RemoveSystem(SystemAddress system);
	unsigned Size(void) const;
	SystemAddress& operator[] ( const unsigned int position ) const;
	void Clear(void);

	DataStructures::List<SystemAddress> * GetList(void);

protected:
	DataStructures::List<SystemAddress> systemList;
};

#endif
