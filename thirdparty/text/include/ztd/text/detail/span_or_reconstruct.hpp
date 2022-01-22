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

#ifndef ZTD_TEXT_SPAN_OR_RECONSTRUCT_HPP
#define ZTD_TEXT_SPAN_OR_RECONSTRUCT_HPP

#include <ztd/text/version.hpp>


#include <ztd/idk/span.hpp>
#include <ztd/ranges/adl.hpp>
#include <ztd/ranges/reconstruct.hpp>

#include <string_view>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {
		template <typename _Input>
		constexpr bool __span_or_reconstruct_noexcept() {
			using _CVInput = ::std::remove_reference_t<_Input>;
			using _UInput  = remove_cvref_t<_Input>;
			if constexpr (is_specialization_of_v<_UInput, ::std::basic_string_view> || ::ztd::is_span_v<_UInput>) {
				return true;
			}
			else if constexpr (ranges::is_iterator_contiguous_iterator_v<ranges::range_iterator_t<_CVInput>>) {
				using _Ty  = ::ztd::ranges::range_value_type_t<_UInput>;
				using _Tag = ::std::in_place_type_t<::ztd::span<_Ty>>;
				return ::ztd::ranges::__rng_detail::__is_cascade_range_reconstruct_noexcept<_Tag, _Input>();
			}
			else {
				using _Tag = ::std::in_place_type_t<_UInput>;
				return ::ztd::ranges::__rng_detail::__is_cascade_range_reconstruct_noexcept<_Tag, _Input>();
			}
		}

		template <typename _Input>
		constexpr bool __string_view_or_span_or_reconstruct_noexcept() {
			using _CVInput = ::std::remove_reference_t<_Input>;
			using _UInput  = remove_cvref_t<_Input>;
			if constexpr (
				::std::is_array_v<
				     _UInput> && ::std::is_const_v<::std::remove_extent_t<_CVInput>> && ::std::is_lvalue_reference_v<_Input>) {
				using _CharTy = ::std::remove_extent_t<_UInput>;
				if constexpr (is_char_traitable_v<_CharTy>) {
					return ::std::is_nothrow_constructible_v<::std::basic_string_view<_CharTy>, _Input>;
				}
				else {
					using _Ty  = ::std::remove_extent_t<_CVInput>;
					using _Tag = ::std::in_place_type_t<::ztd::span<const _Ty>>;
					using _It  = ranges::range_iterator_t<_Input>;
					using _Sen = ranges::range_iterator_t<_Input>;
					return ::ztd::ranges::__rng_detail::__is_cascade_reconstruct_noexcept<_Tag, _It, _Sen>();
				}
			}
			else {
				return __span_or_reconstruct_noexcept<_Input>();
			}
		}

		template <typename _Input>
		constexpr auto __span_or_reconstruct(_Input&& __input) noexcept(__span_or_reconstruct_noexcept<_Input>()) {
			using _VInput = ::std::remove_reference_t<_Input>;
			using _UInput = remove_cvref_t<_Input>;
			if constexpr (is_specialization_of_v<_UInput, ::std::basic_string_view> || ::ztd::is_span_v<_UInput>) {
				return __input;
			}
			else if constexpr (ranges::is_iterator_contiguous_iterator_v<ranges::range_iterator_t<_VInput>>) {
				using _Ty = ::std::remove_reference_t<::ztd::ranges::range_reference_t<_VInput>>;
				return ::ztd::ranges::reconstruct(
					::std::in_place_type<::ztd::span<_Ty>>, ::std::forward<_Input>(__input));
			}
			else {
				return ::ztd::ranges::reconstruct(::std::in_place_type<_UInput>, ::std::forward<_Input>(__input));
			}
		}

		template <typename _Input>
		constexpr auto __string_view_or_span_or_reconstruct(_Input&& __input) noexcept(
			__string_view_or_span_or_reconstruct_noexcept<_Input>()) {
			using _CVInput = ::std::remove_reference_t<_Input>;
			using _UInput  = remove_cvref_t<_Input>;
			// try to catch string literal_ts / arrays
			if constexpr (
				::std::is_array_v<
				     _UInput> && ::std::is_const_v<::std::remove_extent_t<_CVInput>> && ::std::is_lvalue_reference_v<_Input>) {
				using _CharTy = ::std::remove_extent_t<_UInput>;
				if constexpr (is_char_traitable_v<_CharTy>) {
					return ::std::basic_string_view<_CharTy>(::std::forward<_Input>(__input));
				}
				else {
					using _Ty = ::std::remove_extent_t<_CVInput>;
					return ::ztd::ranges::reconstruct(::std::in_place_type<::ztd::span<const _Ty>>,
						ranges::ranges_adl::adl_begin(__input), ranges::ranges_adl::adl_end(__input));
				}
			}
			else {
				return __span_or_reconstruct(::std::forward<_Input>(__input));
			}
		}

		template <typename _Input>
		using __span_or_reconstruct_t = decltype(__span_or_reconstruct(::std::declval<_Input>()));

		template <typename _Input>
		using __string_view_or_span_or_reconstruct_t
			= decltype(__string_view_or_span_or_reconstruct(::std::declval<_Input>()));
	} // namespace __txt_detail

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_SPAN_OR_RECONSTRUCT_HPP
