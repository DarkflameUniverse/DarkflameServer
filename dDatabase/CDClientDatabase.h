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
      \return the results of the query
    */
    // Due to the template, implementation must be in the header.
    template <typename... Args>
    CppSQLite3Query ExecuteQueryWithArgs(const std::string& query, Args... args) {
        CppSQLite3Buffer sqlBuf;
        sqlBuf.format(query.c_str(), args...);

        std::string safe_query = (const char *) sqlBuf;
        return ExecuteQuery(safe_query);
    }
};
