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

#ifndef ZTD_TEXT_THROW_HANDLER_HPP
#define ZTD_TEXT_THROW_HANDLER_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/decode_result.hpp>
#include <ztd/text/encode_result.hpp>
#include <ztd/text/encoding_error.hpp>

#include <exception>
#include <system_error>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @brief An error handler that throws on any encode operation failure.
	///
	/// @remarks This class absolutely should not be used unless the user is prepared to handle spurious failure,
	/// especially for text processing that deals with input vectors. This can result in many exceptions being thrown,
	/// which for resource-intensive applications could cause issues and result in Denial of Service by way of
	/// repeated, unhandled, and unexpected failure.
	//////
	class throw_handler_t {
	public:
		//////
		/// @brief Throws a ztd::text::encoding_error as an exception on an encode failure.
		template <typename _Encoding, typename _InputRange, typename _OutputRange, typename _State,
			typename _InputProgress, typename _OutputProgress>
		constexpr encode_result<_InputRange, _OutputRange, _State> operator()(const _Encoding&,
			encode_result<_InputRange, _OutputRange, _State> __result, const _InputProgress&,
			const _OutputProgress&) const noexcept(false) {
			throw ::std::system_error(static_cast<int>(__result.error_code), ::ztd::text::encoding_category());
		}

		//////
		/// @brief Throws a ztd::text::encoding_error code as an exception on a decode failure.
		template <typename _Encoding, typename _InputRange, typename _OutputRange, typename _State,
			typename _InputProgress, typename _OutputProgress>
		constexpr decode_result<_InputRange, _OutputRange, _State> operator()(const _Encoding&,
			decode_result<_InputRange, _OutputRange, _State> __result, const _InputProgress&,
			const _OutputProgress&) const noexcept(false) {
			throw ::std::system_error(static_cast<int>(__result.error_code), ::ztd::text::encoding_category());
		}
	};

	//////
	/// @brief An instance of throw_handler_t for ease of use.
	inline constexpr throw_handler_t throw_handler = {};

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text


#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_THROW_HANDLER_HPP
