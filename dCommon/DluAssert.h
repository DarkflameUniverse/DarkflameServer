#ifndef DLUASSERT_H
#define DLUASSERT_H

#include <assert.h>

#ifdef _DEBUG
#	define DluAssert(expression) assert(expression)
#else
#	define DluAssert(expression)
#endif

#endif  //!DLUASSERT_H
