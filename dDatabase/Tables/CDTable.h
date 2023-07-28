#pragma once

#include "CDClientDatabase.h"
#include "Singleton.h"
#include "DluAssert.h"

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

// Used for legacy
#define UNUSED(x)

// Enable this to skip some unused columns in some tables
#define UNUSED_COLUMN(v)

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
template<class Table>
class CDTable : public Singleton<Table> {
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

protected:
	virtual ~CDTable() = default;
};

template<class T>
class LookupResult {
	typedef std::pair<T, bool> DataType;
public:
	LookupResult() { m_data.first = T(); m_data.second = false; };
	LookupResult(T& data) { m_data.first = data; m_data.second = true; };
	inline const T& Data() { return m_data.first; };
	inline const bool& FoundData() { return m_data.second; };
private:
	DataType m_data;
};
