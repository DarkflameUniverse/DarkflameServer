#pragma once

// Custom Classes
#include "CDTable.h"
#include <map>

/*!
 \file CDEmoteTable.hpp
 \brief Contains data for the CDEmoteTable table
 */

 //! CDEmoteEntry Struct
struct CDEmoteTable {
	CDEmoteTable() {
		ID = -1;
		animationName = "";
		iconFilename = "";
		locState = -1;
		channel = -1;
		locked = false;
		localize = false;
		gateVersion = -1;
	}

	int ID;
	std::string animationName;
	std::string iconFilename;
	int locState;
	int channel;
	bool locked;
	bool localize;
	int gateVersion;
};

//! CDEmoteTable table
class CDEmoteTableTable : public CDTable {
private:
	std::map<int, CDEmoteTable*> entries;

public:

	//! Constructor
	CDEmoteTableTable(void);

	//! Destructor
	~CDEmoteTableTable(void);

	//! Returns the table's name
	/*!
	 \return The table name
	 */
	std::string GetName(void) const override;

	//! Returns an emote by ID
	CDEmoteTable* GetEmote(int id);
};
