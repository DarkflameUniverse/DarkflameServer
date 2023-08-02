#pragma once

#include "CDAbstractProvider.h"

/**
 * Shared memory provider with CDAbstractSharedMemoryProvider<key, value>
 * 
 * Depends on the boost::interprocess library — header only
 * 
 * Requires that CD_PROVIDER_MEMORY is defined and CD_PROVIDER_SQLITE is not defined
 */
#if defined(CD_PROVIDER_MEMORY) && !defined(CD_PROVIDER_SQLITE)

#include "CDAbstractSharedMemoryProvider.h"

#define CD_PROVIDER(provider, key, value) CDAbstractSharedMemoryProvider<key, value>* provider; typedef key CD_KEY; typedef value CD_VALUE
#define NEW_CD_PROVIDER(provider, name, parser, size_calculator, cache) provider = new CDAbstractSharedMemoryProvider<CD_KEY, CD_VALUE>(name, parser, size_calculator, cache)

template<typename KeyType, typename MappedType>
using CDProvider = CDAbstractSharedMemoryProvider<KeyType, MappedType>;

#endif

/**
 * SQLite provider with CDAbstractSqliteProvider<key, value>
 * 
 * No extra dependencies
 * 
 * Requires that CD_PROVIDER_SQLITE or CD_PROVIDER_MEMORY is not defined — the default option
 */
#if defined(CD_PROVIDER_SQLITE) || !defined(CD_PROVIDER_MEMORY)

#include "CDAbstractSqliteProvider.h"

#define CD_PROVIDER(provider, key, value) CDAbstractSqliteProvider<key, value>* provider; typedef key CD_KEY; typedef value CD_VALUE
#define NEW_CD_PROVIDER(provider, name, parser, size_calculator, cache) provider = new CDAbstractSqliteProvider<CD_KEY, CD_VALUE>(name, parser, cache)

template<typename KeyType, typename MappedType>
using CDProvider = CDAbstractSqliteProvider<KeyType, MappedType>;

#endif
