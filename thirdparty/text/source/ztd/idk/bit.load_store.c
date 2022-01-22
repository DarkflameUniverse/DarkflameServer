// =============================================================================
//
// ztd.idk
// Copyright © 2021 JeanHeyd "ThePhD" Meneide and Shepherd's Oasis, LLC
// Contact: opensource@soasis.org
//
// Commercial License Usage
// Licensees holding valid commercial ztd.idk licenses may use this file in
// accordance with the commercial license agreement provided with the
// Software or, alternatively, in accordance with the terms contained in
// a written agreement between you and Shepherd's Oasis, LLC.
// For licensing terms and conditions see your agreement. For
// further information contact opensource@soasis.org.
//
// Apache License Version 2 Usage
// Alternatively, this file may be used under the terms of Apache License
// Version 2.0 (the "License") for non-commercial use; you may not use this
// file except in compliance with the License. You may obtain a copy of the
// License at
//
//		http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// ============================================================================>

#include <ztd/idk/version.h>

#include <ztd/idk/bit.h>

#include <ztd/idk/endian.h>
#include <ztd/idk/extent.h>
#include <ztd/idk/static_assert.h>
#include <ztd/idk/assume_aligned.hpp>

#if ZTD_IS_ON(ZTD_C_I_)
#include <string.h>
#else
#include <cstring>
#endif

#define ZTDC_GENERATE_LOAD8_BODY_LE(_N, _IS_SIGNED, _SIGNED_PREFIX)                                                    \
	typedef _SIGNED_PREFIX##int_least##_N##_t __type;                                                                 \
	const size_t __idx_limit = (_N)-8;                                                                                \
	const size_t __sign_idx  = (_N)-8;                                                                                \
	__type __value           = { 0 };                                                                                 \
	for (size_t __idx = 0, __ptr_idx = 0; __idx < (_N); __idx += 8, __ptr_idx += 8) {                                 \
		size_t __value_idx               = __idx % CHAR_BIT;                                                         \
		const unsigned char __byte8_mask = (unsigned char)(((unsigned char)(((_IS_SIGNED) && (__idx == __sign_idx)   \
		                                                                         && (__value_idx == (CHAR_BIT - 8))) \
		                                                             ? 0x7F                                          \
		                                                             : 0xFF))                                        \
		     << __value_idx);                                                                                        \
		__value |= (__type)((__type)(__ptr[__ptr_idx / CHAR_BIT] & __byte8_mask) << __idx);                          \
	}                                                                                                                 \
	if ((_IS_SIGNED) && ((__ptr[__idx_limit / CHAR_BIT] & 0x80) != (unsigned char)(0))) {                             \
		__value |= (__type)((__type)(0x1) << (sizeof(__type) * CHAR_BIT - 1));                                       \
	}                                                                                                                 \
	return __value

#define ZTDC_GENERATE_STORE8_BODY_LE(_N, _IS_SIGNED)                                                    \
	memset(__ptr, 0, ((_N) / CHAR_BIT));                                                               \
	const size_t __idx_limit = (_N)-8;                                                                 \
	const size_t __sign_idx  = (_N)-8;                                                                 \
	for (size_t __idx = 0, __ptr_idx = 0; __idx < (_N); __idx += 8, __ptr_idx += 8) {                  \
		size_t __value_idx = __idx % CHAR_BIT;                                                        \
		const unsigned char __byte8_mask                                                              \
		     = (unsigned char)(((_IS_SIGNED) && (__idx == (_N)-8) && (__value_idx == (CHAR_BIT - 8))) \
		               ? (unsigned char)0x7F                                                          \
		               : (unsigned char)0xFF);                                                        \
		__ptr[__ptr_idx / CHAR_BIT]                                                                   \
		     |= (unsigned char)((((unsigned char)(__value >> __idx)) & __byte8_mask) << __value_idx); \
	}                                                                                                  \
	if ((_IS_SIGNED) && __value < 0) {                                                                 \
		__ptr[__idx_limit / CHAR_BIT] |= (unsigned char)((unsigned char)(0x1) << (CHAR_BIT - 1));     \
	}                                                                                                  \
	ztd_static_assert(1, "👍")

#define ZTDC_GENERATE_LOAD8_BODY_BE(_N, _IS_SIGNED, _SIGNED_PREFIX)                                                    \
	typedef _SIGNED_PREFIX##int_least##_N##_t __type;                                                                 \
	const size_t __idx_limit = 0;                                                                                     \
	__type __value           = { 0 };                                                                                 \
	for (size_t __idx = (_N), __ptr_idx = 0; __idx > __idx_limit; __ptr_idx += 8) {                                   \
		__idx -= 8;                                                                                                  \
		size_t __value_idx               = __idx % CHAR_BIT;                                                         \
		const unsigned char __byte8_mask = (unsigned char)(((unsigned char)(((_IS_SIGNED) && (__ptr_idx == 0)        \
		                                                                         && (__value_idx == (CHAR_BIT - 8))) \
		                                                             ? 0x7F                                          \
		                                                             : 0xFF))                                        \
		     << __value_idx);                                                                                        \
		__value |= (__type)(((__type)(__ptr[__ptr_idx / CHAR_BIT] & __byte8_mask)) << __idx);                        \
	}                                                                                                                 \
	if ((_IS_SIGNED)                                                                                                  \
	     && ((__ptr[0] & ((unsigned char)((unsigned char)0x1 << (CHAR_BIT - 1)))) != (unsigned char)(0))) {           \
		__value |= (__type)((__type)(0x1) << (sizeof(__type) * CHAR_BIT - 1));                                       \
	}                                                                                                                 \
	return __value

#define ZTDC_GENERATE_STORE8_BODY_BE(_N, _IS_SIGNED)                                                                   \
	memset(__ptr, 0, ((_N) / CHAR_BIT));                                                                              \
	const size_t __idx_limit = 0;                                                                                     \
	const size_t __sign_idx  = ((_N)-8);                                                                              \
	for (size_t __idx = (_N), __ptr_idx = 0; __idx > __idx_limit; __ptr_idx += 8) {                                   \
		__idx -= 8;                                                                                                  \
		size_t __value_idx = __idx % CHAR_BIT;                                                                       \
		const unsigned char __byte8_mask                                                                             \
		     = (unsigned char)(((_IS_SIGNED) && (__idx == __sign_idx)) ? (unsigned char)0x7F : (unsigned char)0xFF); \
		__ptr[__ptr_idx / CHAR_BIT]                                                                                  \
		     |= (unsigned char)((((unsigned char)(__value >> __idx)) & __byte8_mask) << __value_idx);                \
	}                                                                                                                 \
	if ((_IS_SIGNED) && __value < 0) {                                                                                \
		__ptr[0] |= (unsigned char)((unsigned char)0x1 << (CHAR_BIT - 1));                                           \
	}                                                                                                                 \
	ztd_static_assert(1, "👍")

#define ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(_N)                                                                     \
	uint_least##_N##_t ztdc_load8_leu##_N(const unsigned char __ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)])                   \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		ZTDC_GENERATE_LOAD8_BODY_LE(_N, 0, u);                                                                       \
	}                                                                                                                 \
	uint_least##_N##_t ztdc_load8_beu##_N(const unsigned char __ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)])                   \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		ZTDC_GENERATE_LOAD8_BODY_BE(_N, 0, u);                                                                       \
	}                                                                                                                 \
	uint_least##_N##_t ztdc_load8_aligned_leu##_N(const unsigned char __unaligned_ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)]) \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		const unsigned char* __ptr = ZTD_ASSUME_ALIGNED(((_N) / CHAR_BIT), __unaligned_ptr);                         \
		ZTDC_GENERATE_LOAD8_BODY_LE(_N, 0, u);                                                                       \
	}                                                                                                                 \
	uint_least##_N##_t ztdc_load8_aligned_beu##_N(const unsigned char __unaligned_ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)]) \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		const unsigned char* __ptr = ZTD_ASSUME_ALIGNED(((_N) / CHAR_BIT), __unaligned_ptr);                         \
		ZTDC_GENERATE_LOAD8_BODY_BE(_N, 0, u);                                                                       \
	}                                                                                                                 \
	int_least##_N##_t ztdc_load8_les##_N(const unsigned char __ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)])                    \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		ZTDC_GENERATE_LOAD8_BODY_LE(_N, 1, );                                                                        \
	}                                                                                                                 \
	int_least##_N##_t ztdc_load8_bes##_N(const unsigned char __ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)])                    \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		ZTDC_GENERATE_LOAD8_BODY_BE(_N, 1, );                                                                        \
	}                                                                                                                 \
	int_least##_N##_t ztdc_load8_aligned_les##_N(const unsigned char __unaligned_ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)])  \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		const unsigned char* __ptr = ZTD_ASSUME_ALIGNED(((_N) / CHAR_BIT), __unaligned_ptr);                         \
		ZTDC_GENERATE_LOAD8_BODY_LE(_N, 1, );                                                                        \
	}                                                                                                                 \
	int_least##_N##_t ztdc_load8_aligned_bes##_N(const unsigned char __unaligned_ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)])  \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		const unsigned char* __ptr = ZTD_ASSUME_ALIGNED(((_N) / CHAR_BIT), __unaligned_ptr);                         \
		ZTDC_GENERATE_LOAD8_BODY_BE(_N, 1, );                                                                        \
	}                                                                                                                 \
                                                                                                                       \
	void ztdc_store8_leu##_N(const uint_least##_N##_t __value, unsigned char __ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)])    \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		ZTDC_GENERATE_STORE8_BODY_LE(_N, 0);                                                                         \
	}                                                                                                                 \
	void ztdc_store8_beu##_N(const uint_least##_N##_t __value, unsigned char __ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)])    \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		ZTDC_GENERATE_STORE8_BODY_BE(_N, 0);                                                                         \
	}                                                                                                                 \
	void ztdc_store8_aligned_leu##_N(const uint_least##_N##_t __value,                                                \
	     unsigned char __unaligned_ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)]) ZTD_CXX_NOEXCEPT_I_ {                          \
		unsigned char* __ptr = ZTD_ASSUME_ALIGNED(((_N) / CHAR_BIT), __unaligned_ptr);                               \
		ZTDC_GENERATE_STORE8_BODY_LE(_N, 0);                                                                         \
	}                                                                                                                 \
	void ztdc_store8_aligned_beu##_N(const uint_least##_N##_t __value,                                                \
	     unsigned char __unaligned_ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)]) ZTD_CXX_NOEXCEPT_I_ {                          \
		unsigned char* __ptr = ZTD_ASSUME_ALIGNED(((_N) / CHAR_BIT), __unaligned_ptr);                               \
		ZTDC_GENERATE_STORE8_BODY_BE(_N, 0);                                                                         \
	}                                                                                                                 \
	void ztdc_store8_les##_N(const int_least##_N##_t __value, unsigned char __ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)])     \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		ZTDC_GENERATE_STORE8_BODY_LE(_N, 1);                                                                         \
	}                                                                                                                 \
	void ztdc_store8_bes##_N(const int_least##_N##_t __value, unsigned char __ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)])     \
	     ZTD_CXX_NOEXCEPT_I_ {                                                                                        \
		ZTDC_GENERATE_STORE8_BODY_BE(_N, 1);                                                                         \
	}                                                                                                                 \
	void ztdc_store8_aligned_les##_N(const int_least##_N##_t __value,                                                 \
	     unsigned char __unaligned_ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)]) ZTD_CXX_NOEXCEPT_I_ {                          \
		unsigned char* __ptr = ZTD_ASSUME_ALIGNED(((_N) / CHAR_BIT), __unaligned_ptr);                               \
		ZTDC_GENERATE_STORE8_BODY_LE(_N, 1);                                                                         \
	}                                                                                                                 \
	void ztdc_store8_aligned_bes##_N(const int_least##_N##_t __value,                                                 \
	     unsigned char __unaligned_ptr[ZTD_PTR_EXTENT(_N / CHAR_BIT)]) ZTD_CXX_NOEXCEPT_I_ {                          \
		unsigned char* __ptr = ZTD_ASSUME_ALIGNED(((_N) / CHAR_BIT), __unaligned_ptr);                               \
		ZTDC_GENERATE_STORE8_BODY_BE(_N, 1);                                                                         \
	}                                                                                                                 \
	ztd_static_assert(((_N % 8) == 0), "👍")


#if ZTD_IS_ON(ZTD_COMPILER_GCC_I_) || ZTD_IS_ON(ZTD_COMPILER_CLANG_I_)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif

#if ((CHAR_BIT % 8) == 0)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(8);
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(16);
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(64);
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(32);

#if defined(UINT_LEAST24_WIDTH)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(24);
#endif
#if defined(UINT_LEAST48_WIDTH)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(48);
#endif
#if defined(UINT_LEAST56_WIDTH)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(56);
#endif
#if defined(UINT_LEAST72_WIDTH)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(72);
#endif
#if defined(UINT_LEAST80_WIDTH)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(80);
#endif
#if defined(UINT_LEAST88_WIDTH)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(88);
#endif
#if defined(UINT_LEAST96_WIDTH)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(96);
#endif
#if defined(UINT_LEAST104_WIDTH)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(104);
#endif
#if defined(UINT_LEAST112_WIDTH)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(112);
#endif
#if defined(UINT_LEAST120_WIDTH)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(120);
#endif
#if defined(UINT_LEAST128_WIDTH)
ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS(128);
#endif
#endif

#if ZTD_IS_ON(ZTD_COMPILER_GCC_I_) || ZTD_IS_ON(ZTD_COMPILER_CLANG_I_)
#pragma GCC diagnostic pop
#endif


#undef ZTDC_GENERATE_LOAD8_STORE8_DEFINITIONS
