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

#ifndef ZTD_TEXT_IS_LOSSLESS_HPP
#define ZTD_TEXT_IS_LOSSLESS_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/forward.hpp>
#include <ztd/text/is_full_range_representable.hpp>

#include <ztd/text/type_traits.hpp>

#include <type_traits>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {
		template <typename _ErrorHandler>
		class __is_careless_error_handler
		: public ::std::integral_constant<bool, ::std::is_same_v<remove_cvref_t<_ErrorHandler>, default_handler_t>> {
		};

		template <typename _Byte, typename _UInputRange, typename _UOutputRange, typename _ErrorHandler>
		class __is_careless_error_handler<__scheme_handler<_Byte, _UInputRange, _UOutputRange, _ErrorHandler>>
		: public ::std::integral_constant<bool, __is_careless_error_handler<_ErrorHandler>::value> { };

		template <typename _Encoding, typename _ErrorHandler>
		class __is_careless_error_handler<__forwarding_handler<_Encoding, _ErrorHandler>>
		: public ::std::integral_constant<bool, __is_careless_error_handler<_ErrorHandler>::value> { };

		template <typename _ErrorHandler>
		inline constexpr bool __is_careless_error_handler_v = __is_careless_error_handler<_ErrorHandler>::value;

		template <typename _Encoding, typename _ErrorHandler>
		class __is_encode_lossless_or_deliberate
		: public ::std::integral_constant<bool,
			  __txt_detail::__is_careless_error_handler_v<remove_cvref_t<_ErrorHandler>>
			       ? is_encode_injective_v<remove_cvref_t<_Encoding>>
			       : true> { };

		template <typename _Encoding, typename _ErrorHandler>
		inline constexpr bool __is_encode_lossless_or_deliberate_v
			= __is_encode_lossless_or_deliberate<_Encoding, _ErrorHandler>::value;

		template <typename _Encoding, typename _ErrorHandler>
		class __is_decode_lossless_or_deliberate
		: public ::std::integral_constant<bool,
			  __txt_detail::__is_careless_error_handler_v<remove_cvref_t<_ErrorHandler>>
			       ? is_decode_injective_v<remove_cvref_t<_Encoding>>
			       : true> { };

		template <typename _Encoding, typename _ErrorHandler>
		inline constexpr bool __is_decode_lossless_or_deliberate_v
			= __is_decode_lossless_or_deliberate<_Encoding, _ErrorHandler>::value;

		template <typename _FromEncoding, typename _ToEncoding, typename _FromErrorHandler, typename _ToErrorHandler>
		class __is_transcode_lossless_or_deliberate
		: public ::std::integral_constant<bool,
			  (__txt_detail::__is_careless_error_handler_v<remove_cvref_t<_FromErrorHandler>>
			            ? is_decode_injective_v<remove_cvref_t<_FromEncoding>>
			            : true)
			       && (__txt_detail::__is_careless_error_handler_v<remove_cvref_t<_ToErrorHandler>>
			                 ? is_encode_injective_v<remove_cvref_t<_ToEncoding>>
			                 : true)> { };

		template <typename _FromEncoding, typename _ToEncoding, typename _FromErrorHandler, typename _ToErrorHandler>
		inline constexpr bool __is_transcode_lossless_or_deliberate_v
			= __is_transcode_lossless_or_deliberate<_FromEncoding, _ToEncoding, _FromErrorHandler,
			     _ToErrorHandler>::value;
	} // namespace __txt_detail

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_IS_LOSSLESS_HPP
