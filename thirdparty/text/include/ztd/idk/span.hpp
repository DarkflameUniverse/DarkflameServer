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

#pragma once

#ifndef ZTD_IDK_SPAN_HPP
#define ZTD_IDK_SPAN_HPP

#include <ztd/idk/version.hpp>

#include <type_traits>

#if ZTD_IS_ON(ZTD_STD_LIBRARY_SPAN_I_)

#include <span>

#include <ztd/prologue.hpp>

namespace ztd {
	ZTD_IDK_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @brief dynamic extent copycat
	///
	//////
	inline constexpr decltype(::std::dynamic_extent) dynamic_extent = ::std::dynamic_extent;

	using ::std::as_bytes;
	using ::std::as_writable_bytes;
	using ::std::span;

	ZTD_IDK_INLINE_ABI_NAMESPACE_CLOSE_I_
} // namespace ztd

#else

// Use home-grown span from Martin Moene
#include <ztd/idk/detail/span.implementation.hpp>

#include <ztd/prologue.hpp>

namespace ztd {
	ZTD_IDK_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @brief dynamic extent copycat
	///
	//////
	inline constexpr decltype(::nonstd::dynamic_extent) dynamic_extent = ::nonstd::dynamic_extent;

	using ::nonstd::as_bytes;
	using ::nonstd::as_writable_bytes;
	using ::nonstd::span;

	ZTD_IDK_INLINE_ABI_NAMESPACE_CLOSE_I_
} // namespace ztd

#if (ZTD_IS_ON(ZTD_LIBSTDCXX_I_) && ZTD_IS_ON(ZTD_STD_LIBRARY_CONCEPTS_I_)) \
     || (ZTD_IS_ON(ZTD_LIBVCXX_I_) && ZTD_IS_ON(ZTD_STD_LIBRARY_CONCEPTS_I_)) || ZTD_IS_ON(ZTD_STD_LIBRARY_RANGES_I_)

namespace std {

	template <typename _Ty, decltype(ztd::ranges::dynamic_extent) _Sen>
	inline constexpr bool enable_borrowed_range<::ztd::span<_Ty, _Extent>> = true;

} // namespace std
#endif

#endif

namespace ztd {
	ZTD_IDK_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @brief Determines whether or not a given type is a @c span of some kind.
	//////
	template <typename _Ty>
	class is_span : public ::std::false_type { };

	template <typename _Ty, decltype(::ztd::dynamic_extent) _N>
	class is_span<::ztd::span<_Ty, _N>> : public ::std::true_type { };

	//////
	/// @brief A @c _v alias for ztd::is_span.
	//////
	template <typename _Ty>
	inline constexpr bool is_span_v = is_span<_Ty>::value;

	ZTD_IDK_INLINE_ABI_NAMESPACE_CLOSE_I_
} // namespace ztd

#include <ztd/epilogue.hpp>

#endif // ZTD_IDK_SPAN_HPP
