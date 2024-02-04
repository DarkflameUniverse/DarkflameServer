#ifndef __DATTRIBUTES_H__
#define __DATTRIBUTES_H__

/**
 * Attribute tag documentation for each compiler:
 * MSVC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 * CLANG: https://clang.llvm.org/docs/AttributeReference.html
 * GNU: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
*/

// Attribute for when the function's purpose is to be a cast expression
#ifdef __has_cpp_attribute
	#if __has_cpp_attribute(msvc::intrinsic)
		#define DCAST_ALIAS msvc::intrinsic
	#elif __has_cpp_attribute(clang::builtin_alias)
		#define DCAST_ALIAS clang::builtin_alias
	#elif __has_cpp_attribute(gnu::always_inline)
		#define DCAST_ALIAS gnu::always_inline // GNU doesn't have an exact equivalent, so use always_inline
	#else
		#define DCAST_ALIAS
	#endif
#else
	#define DCAST_ALIAS
#endif

// Always inline
#ifdef __has_cpp_attribute
	#if __has_cpp_attribute(msvc::forceinline)
		#define DALWAYS_INLINE msvc::forceinline
	#elif __has_cpp_attribute(clang::always_inline)
		#define DALWAYS_INLINE clang::always_inline
	#elif __has_cpp_attribute(gnu::always_inline)
		#define DALWAYS_INLINE gnu::always_inline
	#else
		#define DALWAYS_INLINE
	#endif
#else
	#define DALWAYS_INLINE
#endif

// No inline
#ifdef __has_cpp_attribute
	#if __has_cpp_attribute(msvc::noinline)
		#define DNO_INLINE msvc::noinline
	#elif __has_cpp_attribute(clang::noinline)
		#define DNO_INLINE clang::noinline
	#elif __has_cpp_attribute(gnu::no_inline)
		#define DNO_INLINE gnu::no_inline
	#else
		#define DNO_INLINE
	#endif
#else
	#define DNO_INLINE
#endif

#endif // !__DATTRIBUTES_H__
