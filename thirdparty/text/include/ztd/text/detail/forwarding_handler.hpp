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

#ifndef ZTD_TEXT_DETAIL_FORWARDING_HANDLER_HPP
#define ZTD_TEXT_DETAIL_FORWARDING_HANDLER_HPP

#include <ztd/text/version.hpp>

#include <ztd/idk/ebco.hpp>

#include <utility>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {

		template <typename _Encoding, typename _ErrorHandler>
		class __forwarding_handler : private ebco<_Encoding&, 0>, private ebco<_ErrorHandler&, 1> {
		private:
			using __encoding_base_t      = ebco<_Encoding&, 0>;
			using __error_handler_base_t = ebco<_ErrorHandler&, 1>;

		public:
			constexpr __forwarding_handler(_Encoding& __encoding, _ErrorHandler& __error_handler) noexcept
			: __encoding_base_t(__encoding), __error_handler_base_t(__error_handler) {
			}

			template <typename _UnderlyingEncoding, typename _Result, typename _InputProgress,
				typename _OutputProgress>
			constexpr auto operator()(_UnderlyingEncoding&&, _Result&& __result, _InputProgress&& __input_progress,
				_OutputProgress&& __output_progress) const& // clang-format hack
				noexcept(noexcept(this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
				     ::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
				     ::std::forward<_OutputProgress>(__output_progress)))) {
				return this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
					::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
					::std::forward<_OutputProgress>(__output_progress));
			}

			template <typename _UnderlyingEncoding, typename _Result, typename _InputProgress,
				typename _OutputProgress>
			constexpr auto operator()(_UnderlyingEncoding&&, _Result&& __result, _InputProgress&& __input_progress,
				_OutputProgress&& __output_progress) & // clang-format hack
				noexcept(noexcept(this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
				     ::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
				     ::std::forward<_OutputProgress>(__output_progress)))) {
				return this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
					::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
					::std::forward<_OutputProgress>(__output_progress));
			}

			template <typename _UnderlyingEncoding, typename _Result, typename _InputProgress,
				typename _OutputProgress>
			constexpr auto operator()(_UnderlyingEncoding&&, _Result&& __result, _InputProgress&& __input_progress,
				_OutputProgress&& __output_progress) && // clang-format hack
				noexcept(noexcept(this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
				     ::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
				     ::std::forward<_OutputProgress>(__output_progress)))) {
				return this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
					::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
					::std::forward<_OutputProgress>(__output_progress));
			}
		};

	} // namespace __txt_detail

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text


#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_FORWARDING_HANDLER_HPP
