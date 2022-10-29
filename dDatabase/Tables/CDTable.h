#pragma once

// Custom Classes
#include "../CDClientDatabase.h"

// C++
#include <functional>
#include <string>
#include <vector>
#include <map>

// CPPLinq
#ifdef _WIN32
#define NOMINMAX
// windows.h has min and max macros that breaks cpplinq
#endif
#include "cpplinq.hpp"

#pragma warning (disable : 4244) //Disable double to float conversion warnings
#pragma warning (disable : 4715) //Disable "not all control paths return a value"

#if defined(__unix) || defined(__APPLE__)
//For Linux:
typedef __int64_t __int64;
#endif

/*!
  \file CDTable.hpp
  \brief A virtual class for CDClient Tables
 */

 //! The base class for all CD tables
class CDTable {
public:
    
    //! Returns the table's name
    /*!
      \return The table name
     */
    virtual std::string GetName() const = 0;

    //! Loads the table into shared memory
    virtual void LoadHost() {};

    //! Initalizes the table services
    static void Initalize();

    //! Initalizes the table services as host
    static void InitalizeHost();

    //! Get a string from a handle
    /*!
      \param handle The handle to get the string from
      \return The string
     */
    static std::string GetString(size_t handle);

    //! Set a string
    /*!
      \param value The string to set
      \return The handle to the string
     */
    static size_t SetString(std::string value);
};
