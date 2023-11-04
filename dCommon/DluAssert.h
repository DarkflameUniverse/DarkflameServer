#ifndef __DLUASSERT__H__
#define __DLUASSERT__H__

#include <assert.h>

#ifdef _DEBUG
#	define DluAssert(expression) assert(expression)
#else
#	define DluAssert(expression)
#endif

#endif  //!__DLUASSERT__H__
