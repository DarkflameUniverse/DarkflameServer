// =============================================================================
//
// ztd.text
// Copyright © 2021 JeanHeyd "ThePhD" Meneide and Shepherd's Oasis, LLC
// Contact: opensource@soasis.org
//
// Commercial License Usage
// Licensees holding valid commercial ztd.text licenses may use this file in
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

#ifndef ZTD_TEXT_DETAIL_ENCODING_ITERATOR_STORAGE_HPP
#define ZTD_TEXT_DETAIL_ENCODING_ITERATOR_STORAGE_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/state.hpp>
#include <ztd/text/encoding_error.hpp>


#include <ztd/idk/ebco.hpp>
#include <ztd/idk/detail/math.hpp>
#include <ztd/ranges/range.hpp>

#include <cstddef>
#include <climits>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_
	namespace __txt_detail {

		inline constexpr ::std::size_t _CursorlessSizeSentinel = 1;

		template <typename _Encoding, typename _EncodingState, ::std::size_t _Id = 0>
		class __state_storage : private ebco<remove_cvref_t<unwrap_t<_EncodingState>>, _Id> {
		private:
			using _UEncoding      = remove_cvref_t<unwrap_t<_Encoding>>;
			using _UEncodingState = remove_cvref_t<unwrap_t<_EncodingState>>;
			using __state_base_t  = ebco<remove_cvref_t<unwrap_t<_EncodingState>>, _Id>;

		public:
			template <typename _ArgEncoding = _UEncoding,
				::std::enable_if_t<
				     !is_state_independent_v<remove_cvref_t<_ArgEncoding>,
				          _UEncodingState> && !::std::is_same_v<remove_cvref_t<_ArgEncoding>, __state_storage>>* = nullptr>
			constexpr __state_storage(_ArgEncoding& __encoding) noexcept(
				::std::is_nothrow_constructible_v<__state_base_t, _Encoding&>)
			: __state_base_t(::std::forward<_ArgEncoding>(__encoding)) {
			}
			template <typename _ArgEncoding = _UEncoding,
				::std::enable_if_t<
				     is_state_independent_v<remove_cvref_t<_ArgEncoding>,
				          _UEncodingState> && !::std::is_same_v<remove_cvref_t<_ArgEncoding>, __state_storage>>* = nullptr>
			constexpr __state_storage(_ArgEncoding&) noexcept(
				::std::is_nothrow_default_constructible_v<__state_base_t>)
			: __state_base_t() {
			}
			constexpr __state_storage(_Encoding&, const _UEncodingState& __state) noexcept(
				::std::is_nothrow_constructible_v<__state_base_t, const _UEncodingState&>)
			: __state_base_t(__state) {
			}
			constexpr __state_storage(_Encoding&, _UEncodingState&& __state) noexcept(
				::std::is_nothrow_constructible_v<__state_base_t, _UEncodingState&&>)
			: __state_base_t(::std::move(__state)) {
			}
			constexpr __state_storage(const __state_storage&) = default;
			constexpr __state_storage(__state_storage&&)      = default;
			constexpr __state_storage& operator=(const __state_storage&) = default;
			constexpr __state_storage& operator=(__state_storage&&) = default;

			constexpr ::std::add_lvalue_reference_t<_UEncodingState> _M_get_state() noexcept {
				return this->__state_base_t::get_value();
			}

			constexpr ::std::add_const_t<::std::add_lvalue_reference_t<_UEncodingState>>
			_M_get_state() const noexcept {
				return this->__state_base_t::get_value();
			}
		};

		template <::std::size_t _MaxN, bool __is_input_or_output_iterator>
		class __cursor_cache {
		public:
			// clang-format off
			using _SizeType = ::std::conditional_t<(_MaxN <= UCHAR_MAX), unsigned char,
				::std::conditional_t<(_MaxN <= USHRT_MAX), unsigned short, 
					::std::conditional_t<(_MaxN <= UINT_MAX), unsigned int,
						::std::conditional_t<(_MaxN <= ULONG_MAX), unsigned long,
							::std::conditional_t<(_MaxN <= ULLONG_MAX), unsigned long long, ::std::size_t>
						>
					>
				>
			>;
			// clang-format on

			_SizeType _M_size     = static_cast<_SizeType>(0);
			_SizeType _M_position = static_cast<_SizeType>(0);
		};

		template <>
		class __cursor_cache<1, true> {
		public:
			using _SizeType = unsigned char;

			_SizeType _M_size = static_cast<_SizeType>(0);
		};

		template <>
		class __cursor_cache<1, false> {
		public:
			using _SizeType = unsigned char;

			_SizeType _M_size = static_cast<_SizeType>(0);
		};

		template <bool>
		class __error_cache {
		public:
			encoding_error _M_error_code = encoding_error::ok;
		};

		template <>
		class __error_cache<true> { };

	} // namespace __txt_detail
	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_ENCODING_ITERATOR_STORAGE_HPP
