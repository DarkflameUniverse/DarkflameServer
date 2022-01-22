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

#ifndef ZTD_TEXT_DETAIL_SCHEME_HANDLER_HPP
#define ZTD_TEXT_DETAIL_SCHEME_HANDLER_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/decode_result.hpp>
#include <ztd/text/encode_result.hpp>

#include <ztd/idk/span.hpp>
#include <ztd/ranges/range.hpp>
#include <ztd/idk/reference_wrapper.hpp>

#include <type_traits>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {

		template <typename _Byte, typename _UInputRange, typename _UOutputRange, typename _ErrorHandler>
		class __scheme_handler {
		private:
			::ztd::reference_wrapper<_ErrorHandler> _M_handler;

			template <typename _Encoding, typename _Input, typename _Output, typename _State,
				typename _InputProgress, typename _OutputProgress>
			static constexpr bool __decode_call_operator_noexcept() noexcept {
				if constexpr (::std::is_convertible_v<ranges::range_value_type_t<_InputProgress>, _Byte>) {
					if constexpr (::std::is_invocable_v<_ErrorHandler&, const _Encoding,
						              decode_result<_Input, _Output, _State>, const _InputProgress&,
						              const _OutputProgress&>) {
						return ::std::is_nothrow_invocable_v<_ErrorHandler&, const _Encoding&,
							decode_result<_Input, _Output, _State>, const _InputProgress&,
							const _OutputProgress&>;
					}
					else {
						return ::std::is_nothrow_invocable_v<_ErrorHandler&, const _Encoding&,
							decode_result<_Input, _Output, _State>, ::ztd::span<_Byte>, const _OutputProgress&>;
					}
				}
				else {
					return ::std::is_nothrow_invocable_v<_ErrorHandler&, const _Encoding&,
						decode_result<_Input, _Output, _State>, ::ztd::span<_Byte>, const _OutputProgress&>;
				}
			}

			template <typename _Encoding, typename _Input, typename _Output, typename _State,
				typename _InputProgress, typename _OutputProgress>
			static constexpr bool __encode_call_operator_noexcept() noexcept {
				if constexpr (::std::is_convertible_v<ranges::range_value_type_t<_OutputProgress>, _Byte>) {
					if constexpr (::std::is_invocable_v<_ErrorHandler&, const _Encoding,
						              encode_result<_Input, _Output, _State>, const _InputProgress&,
						              const _OutputProgress&>) {
						return ::std::is_nothrow_invocable_v<_ErrorHandler&, const _Encoding&,
							encode_result<_Input, _Output, _State>, const _InputProgress&,
							const _OutputProgress&>;
					}
					else {
						return ::std::is_nothrow_invocable_v<_ErrorHandler&, const _Encoding&,
							encode_result<_Input, _Output, _State>, const _InputProgress&, ::ztd::span<_Byte>>;
					}
				}
				else {
					return ::std::is_nothrow_invocable_v<_ErrorHandler&, const _Encoding&,
						encode_result<_Input, _Output, _State>, const _InputProgress&, ::ztd::span<_Byte>>;
				}
			}

			template <typename _Encoding, typename _Input, typename _Output, typename _State,
				typename _InputProgress, typename _OutputProgress>
			constexpr auto _M_basic_encode(const _Encoding& __encoding,
				encode_result<_Input, _Output, _State>&& __result, const _InputProgress& __input_progress,
				const _OutputProgress& __output_progress) const
				noexcept(::std::is_nothrow_invocable_v<_ErrorHandler&, const _Encoding&,
				     encode_result<_Input, _Output, _State>, const _InputProgress&, ::ztd::span<_Byte>>) {
				using _ProgressPointer = ranges::range_pointer_t<_OutputProgress>;
				using _ProgressWord    = ranges::range_value_type_t<_OutputProgress>;
				_Byte* __byte_progress_data
					= reinterpret_cast<_Byte*>(const_cast<_ProgressPointer>(__output_progress.data()));
				auto __byte_progress_size
					= (ranges::ranges_adl::adl_size(__output_progress) * sizeof(_ProgressWord)) / (sizeof(_Byte));
				::ztd::span<_Byte> __byte_progress(__byte_progress_data, __byte_progress_size);
				return this->_M_handler.get()(__encoding, ::std::move(__result), __input_progress, __byte_progress);
			}

			template <typename _Encoding, typename _Input, typename _Output, typename _State,
				typename _InputProgress, typename _OutputProgress>
			constexpr auto _M_basic_decode(const _Encoding& __encoding,
				decode_result<_Input, _Output, _State>&& __result, const _InputProgress& __input_progress,
				const _OutputProgress& __output_progress) const
				noexcept(::std::is_nothrow_invocable_v<_ErrorHandler&, const _Encoding&,
				     decode_result<_Input, _Output, _State>, ::ztd::span<_Byte>, const _OutputProgress&>) {
				using _ProgressPointer = ranges::range_pointer_t<_InputProgress>;
				using _ProgressWord    = ranges::range_value_type_t<_InputProgress>;
				_Byte* __byte_progress_data
					= reinterpret_cast<_Byte*>(const_cast<_ProgressPointer>(__input_progress.data()));
				auto __byte_progress_size
					= (ranges::ranges_adl::adl_size(__input_progress) * sizeof(_ProgressWord)) / (sizeof(_Byte));
				::ztd::span<_Byte> __byte_progress(__byte_progress_data, __byte_progress_size);
				return this->_M_handler.get()(
					__encoding, ::std::move(__result), __byte_progress, __output_progress);
			}

		public:
			constexpr __scheme_handler(_ErrorHandler& __handler) noexcept : _M_handler(__handler) {
			}

			template <typename _Encoding, typename _Input, typename _Output, typename _State,
				typename _InputProgress, typename _OutputProgress>
			constexpr auto operator()(const _Encoding& __encoding, decode_result<_Input, _Output, _State> __result,
				const _InputProgress& __input_progress, const _OutputProgress& __output_progress) const
				noexcept(__decode_call_operator_noexcept<_Encoding, _Input, _Output, _State, _InputProgress,
				     _OutputProgress>()) {
				if constexpr (::std::is_convertible_v<ranges::range_value_type_t<_InputProgress>, _Byte>) {
					if constexpr (::std::is_invocable_v<_ErrorHandler&, const _Encoding,
						              decode_result<_Input, _Output, _State>, const _InputProgress&,
						              const _OutputProgress&>) {
						return this->_M_handler.get()(
							__encoding, ::std::move(__result), __input_progress, __output_progress);
					}
					else {
						return this->_M_basic_decode(
							__encoding, ::std::move(__result), __input_progress, __output_progress);
					}
				}
				else {
					return this->_M_basic_decode(
						__encoding, ::std::move(__result), __input_progress, __output_progress);
				}
			}

			template <typename _Encoding, typename _Input, typename _Output, typename _State,
				typename _InputProgress, typename _OutputProgress>
			constexpr auto operator()(const _Encoding& __encoding, encode_result<_Input, _Output, _State> __result,
				const _InputProgress& __input_progress, const _OutputProgress& __output_progress) const
				noexcept(__encode_call_operator_noexcept<_Encoding, _Input, _Output, _State, _InputProgress,
				     _OutputProgress>()) {
				if constexpr (::std::is_convertible_v<ranges::range_value_type_t<_InputProgress>, _Byte>) {
					if constexpr (::std::is_invocable_v<_ErrorHandler&, const _Encoding,
						              encode_result<_Input, _Output, _State>, const _InputProgress&,
						              const _OutputProgress&>) {
						return this->_M_handler.get()(
							__encoding, ::std::move(__result), __input_progress, __output_progress);
					}
					else {
						return this->_M_basic_encode(
							__encoding, ::std::move(__result), __input_progress, __output_progress);
					}
				}
				else {
					return this->_M_basic_encode(
						__encoding, ::std::move(__result), __input_progress, __output_progress);
				}
			}
		};
	} // namespace __txt_detail

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_SCHEME_HANDLER_HPP
