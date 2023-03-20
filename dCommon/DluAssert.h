#ifndef __DLUASSERT__H__
#define __DLUASSERT__H__

#include <assert.h>

#define _DEBUG

#ifdef _DEBUG
#	define DluAssert(expression) assert(expression)
#else
#	define DluAssert(expression)
#endif

#endif  //!__DLUASSERT__H__
