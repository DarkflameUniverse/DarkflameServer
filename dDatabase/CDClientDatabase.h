#pragma once

// C++
#include <string>

// SQLite
#include "CppSQLite3.h"

/*
 * Optimization settings
 */

#include <sstream>
#include <iostream>

 // Enable this to cache all entries in each table for fast access, comes with more memory cost
 //#define CDCLIENT_CACHE_ALL

 // Enable this to skip some unused columns in some tables
#define UNUSED(v)

/*!
  \file CDClientDatabase.hpp
  \brief An interface between the CDClient.sqlite file and the server
 */

 //! The CDClient Database namespace
namespace CDClientDatabase {

	//! Opens a connection with the CDClient
	/*!
	  \param filename The filename
	 */
	void Connect(const std::string& filename);

	//! Queries the CDClient
	/*!
	  \param query The query
	  \return The results of the query
	 */
	CppSQLite3Query ExecuteQuery(const std::string& query);

	//! Queries the CDClient and parses arguments
	/*!
	  \param query The query with formatted arguments
	  \return prepared SQLite Statement
	*/
	CppSQLite3Statement CreatePreppedStmt(const std::string& query);
};
