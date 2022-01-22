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

#ifndef ZTD_TEXT_ERROR_HANDLER_HPP
#define ZTD_TEXT_ERROR_HANDLER_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/code_point.hpp>
#include <ztd/text/code_unit.hpp>
#include <ztd/text/state.hpp>
#include <ztd/text/encode_result.hpp>
#include <ztd/text/decode_result.hpp>
#include <ztd/text/encoding_error.hpp>
#include <ztd/text/is_code_points_replaceable.hpp>
#include <ztd/text/is_code_units_replaceable.hpp>
#include <ztd/text/is_unicode_code_point.hpp>
#include <ztd/text/type_traits.hpp>
#include <ztd/text/assume_valid_handler.hpp>
#include <ztd/text/throw_handler.hpp>
#include <ztd/text/pass_handler.hpp>
#include <ztd/text/detail/pass_through_handler.hpp>
#include <ztd/text/detail/forwarding_handler.hpp>
#include <ztd/text/detail/transcode_one.hpp>

#include <ztd/ranges/range.hpp>
#include <ztd/ranges/reconstruct.hpp>
#include <ztd/idk/ebco.hpp>
#include <ztd/idk/detail/unicode.hpp>

#include <climits>
#include <cstddef>
#include <utility>
#include <array>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {

		template <typename _Encoding, typename _Input, typename _Result>
		constexpr _Result&& __write_direct(const _Encoding&, _Input&& __input, _Result&& __result) noexcept {
			using _UOutputRange = remove_cvref_t<decltype(__result.output)>;

			auto __outit   = ranges::ranges_adl::adl_begin(__result.output);
			auto __outlast = ranges::ranges_adl::adl_end(__result.output);
			if (__outit == __outlast) {
				// BAIL
				return ::std::forward<_Result>(__result);
			}

			if (ranges::ranges_adl::adl_empty(__input)) {
				// empty range, everything is okay
				__result.error_code = encoding_error::ok;
				return ::std::forward<_Result>(__result);
			}

			for (const auto& __element : ::std::forward<_Input>(__input)) {
				if (__outit == __outlast) {
					__result.output = ranges::reconstruct(
						::std::in_place_type<_UOutputRange>, ::std::move(__outit), ::std::move(__outlast));
					return ::std::forward<_Result>(__result);
				}
				(*__outit) = __element;
				++__outit;
			}

			__result.output = ranges::reconstruct(
				::std::in_place_type<_UOutputRange>, ::std::move(__outit), ::std::move(__outlast));
			__result.error_code = encoding_error::ok;
			return ::std::forward<_Result>(__result);
		}

		template <typename _Encoding, typename _Result>
		constexpr _Result&& __write_static_code_points_direct(
			const _Encoding& __encoding, _Result&& __result) noexcept {
			using _InputCodePoint = code_point_t<_Encoding>;
			if constexpr (is_code_points_replaceable_v<_Encoding, decltype(__result.state)&>) {
				return __txt_detail::__write_direct(__encoding, __encoding.replacement_code_points(__result.state),
					::std::forward<_Result>(__result));
			}
			else if constexpr (is_code_points_replaceable_v<_Encoding>) {
				return __txt_detail::__write_direct(
					__encoding, __encoding.replacement_code_points(), ::std::forward<_Result>(__result));
			}
			else if constexpr (is_code_points_maybe_replaceable_v<_Encoding, decltype(__result.state)&>) {
				decltype(auto) __maybe_code_points = __encoding.maybe_replacement_code_points(__result.state);
				if (__maybe_code_points) {
					return __txt_detail::__write_direct(__encoding,
						*::std::forward<decltype(__maybe_code_points)>(__maybe_code_points),
						::std::forward<_Result>(__result));
				}
				else {
					return ::std::forward<_Result>(__result);
				}
			}
			else if constexpr (is_code_points_maybe_replaceable_v<_Encoding>) {
				decltype(auto) __maybe_code_points = __encoding.maybe_replacement_code_points();
				if (__maybe_code_points) {
					return __txt_detail::__write_direct(__encoding,
						*::std::forward<decltype(__maybe_code_points)>(__maybe_code_points),
						::std::forward<_Result>(__result));
				}
				else {
					return ::std::forward<_Result>(__result);
				}
			}
			else if constexpr (is_unicode_code_point_v<_InputCodePoint>) {
				constexpr _InputCodePoint __replacements[1]
					= { static_cast<_InputCodePoint>(__ztd_idk_detail_replacement) };
				return __txt_detail::__write_direct(__encoding, __replacements, ::std::forward<_Result>(__result));
			}
			else {
				static_assert(always_false_v<_Encoding>,
					"There is no logical replacement code points to insert into the stream on failure for the "
					"specified encoding type.");
			}
		}

		template <typename _Encoding, typename _Result>
		constexpr _Result&& __write_static_code_units_direct(
			const _Encoding& __encoding, _Result&& __result) noexcept {
			using _InputCodeUnit = code_unit_t<_Encoding>;
			if constexpr (is_code_units_replaceable_v<_Encoding, decltype(__result.state)&>) {
				return __txt_detail::__write_direct(__encoding, __encoding.replacement_code_units(__result.state),
					::std::forward<_Result>(__result));
			}
			else if constexpr (is_code_units_replaceable_v<_Encoding>) {
				return __txt_detail::__write_direct(
					__encoding, __encoding.replacement_code_units(), ::std::forward<_Result>(__result));
			}
			else if constexpr (is_code_units_maybe_replaceable_v<_Encoding, decltype(__result.state)&>) {
				decltype(auto) __maybe_code_points = __encoding.maybe_replacement_code_units(__result.state);
				if (__maybe_code_points) {
					return __txt_detail::__write_direct(__encoding,
						*::std::forward<decltype(__maybe_code_points)>(__maybe_code_points),
						::std::forward<_Result>(__result));
				}
				else {
					return ::std::forward<_Result>(__result);
				}
			}
			else if constexpr (is_code_units_maybe_replaceable_v<_Encoding>) {
				decltype(auto) __maybe_code_points = __encoding.maybe_replacement_code_units();
				if (__maybe_code_points) {
					return __txt_detail::__write_direct(__encoding,
						*::std::forward<decltype(__maybe_code_points)>(__maybe_code_points),
						::std::forward<_Result>(__result));
				}
				else {
					return ::std::forward<_Result>(__result);
				}
			}
			else if constexpr (sizeof(_InputCodeUnit) >= sizeof(char)) {
				constexpr _InputCodeUnit __replacements[1]
					= { static_cast<_InputCodeUnit>(__ztd_idk_detail_ascii_replacement) };
				return __txt_detail::__write_direct(__encoding, __replacements, ::std::forward<_Result>(__result));
			}
			else {
				static_assert(always_false_v<_Encoding>,
					"There is no logical replacement code units to insert into the stream on failure for the "
					"specified encoding type.");
			}
		}

		template <typename _Encoding, typename _State>
		constexpr ::std::size_t __fill_replacement_code_point_static(const _Encoding& __encoding, _State& __state,
			code_point_t<_Encoding> (&__replacement_code_points)[max_code_points_v<_Encoding>]) {
			using _InputCodePoint = code_point_t<_Encoding>;
			if constexpr (is_code_points_replaceable_v<_Encoding, _State&>) {
				::std::size_t __replacement_index = 0;
				for (const auto& __element : __encoding.replacement_code_points(__state)) {
					__replacement_code_points[__replacement_index] = static_cast<_InputCodePoint>(__element);
					++__replacement_index;
				}
				return __replacement_index;
			}
			else if constexpr (is_code_points_replaceable_v<_Encoding>) {
				::std::size_t __replacement_index = 0;
				for (const auto& __element : __encoding.replacement_code_points()) {
					__replacement_code_points[__replacement_index] = static_cast<_InputCodePoint>(__element);
					++__replacement_index;
				}
				return __replacement_index;
			}
			else if constexpr (is_code_points_maybe_replaceable_v<_Encoding, _State&>) {
				::std::size_t __replacement_index  = 0;
				decltype(auto) __maybe_code_points = __encoding.maybe_replacement_code_points(__state);
				if (__maybe_code_points) {
					decltype(auto) __code_points
						= *::std::forward<decltype(__maybe_code_points)>(__maybe_code_points);
					for (const auto& __element : __code_points) {
						__replacement_code_points[__replacement_index] = static_cast<_InputCodePoint>(__element);
						++__replacement_index;
					}
				}
				return __replacement_index;
			}
			else if constexpr (is_code_points_maybe_replaceable_v<_Encoding>) {
				::std::size_t __replacement_index  = 0;
				decltype(auto) __maybe_code_points = __encoding.maybe_replacement_code_points();
				if (__maybe_code_points) {
					decltype(auto) __code_points
						= *::std::forward<decltype(__maybe_code_points)>(__maybe_code_points);
					for (const auto& __element : __code_points) {
						__replacement_code_points[__replacement_index] = static_cast<_InputCodePoint>(__element);
						++__replacement_index;
					}
				}
				return __replacement_index;
			}
			else if constexpr (is_unicode_code_point_v<_InputCodePoint>) {
				__replacement_code_points[0] = static_cast<_InputCodePoint>(__ztd_idk_detail_replacement);
				return 1;
			}
			else {
				static_assert(always_false_v<_Encoding>,
					"There is no logical replacement code points to insert into the stream on failure for the "
					"specified encoding type.");
			}
		}

		template <typename _Encoding, typename _State>
		constexpr ::std::size_t __fill_replacement_code_unit_static(const _Encoding& __encoding, _State& __state,
			code_unit_t<_Encoding> (&__replacement_code_units)[max_code_units_v<_Encoding>]) {
			using _InputCodeUnit = code_unit_t<_Encoding>;
			if constexpr (is_code_units_replaceable_v<_Encoding, _State&>) {
				::std::size_t __replacement_index = 0;
				for (const auto& __element : __encoding.replacement_code_units(__state)) {
					__replacement_code_units[__replacement_index] = static_cast<_InputCodeUnit>(__element);
					++__replacement_index;
				}
				return __replacement_index;
			}
			else if constexpr (is_code_units_replaceable_v<_Encoding>) {
				::std::size_t __replacement_index = 0;
				for (const auto& __element : __encoding.replacement_code_units()) {
					__replacement_code_units[__replacement_index] = static_cast<_InputCodeUnit>(__element);
					++__replacement_index;
				}
				return __replacement_index;
			}
			else if constexpr (is_code_units_maybe_replaceable_v<_Encoding, _State&>) {
				::std::size_t __replacement_index = 0;
				decltype(auto) __maybe_code_units = __encoding.maybe_replacement_code_units(__state);
				if (__maybe_code_units) {
					decltype(auto) __code_units
						= *::std::forward<decltype(__maybe_code_units)>(__maybe_code_units);
					for (const auto& __element : __code_units) {
						__replacement_code_units[__replacement_index] = static_cast<_InputCodeUnit>(__element);
						++__replacement_index;
					}
				}
				return __replacement_index;
			}
			else if constexpr (is_code_units_maybe_replaceable_v<_Encoding>) {
				::std::size_t __replacement_index = 0;
				decltype(auto) __maybe_code_units = __encoding.maybe_replacement_code_units();
				if (__maybe_code_units) {
					decltype(auto) __code_units
						= *::std::forward<decltype(__maybe_code_units)>(__maybe_code_units);
					for (const auto& __element : __code_units) {
						__replacement_code_units[__replacement_index] = static_cast<_InputCodeUnit>(__element);
						++__replacement_index;
					}
				}
				return __replacement_index;
			}
			else if constexpr (is_unicode_code_point_v<_InputCodeUnit>) {
				__replacement_code_units[0] = static_cast<_InputCodeUnit>(__ztd_idk_detail_replacement);
				return 1;
			}
			else if constexpr (sizeof(_InputCodeUnit) >= sizeof(char)) {
				__replacement_code_units[0] = static_cast<_InputCodeUnit>(__ztd_idk_detail_ascii_replacement);
				return 1;
			}
			else {
				static_assert(always_false_v<_Encoding>,
					"There is no logical replacement code units to insert into the stream on failure for the "
					"specified encoding type.");
			}
		}
	} // namespace __txt_detail

	//////
	/// @brief An error handler that replaces bad code points and code units with a chosen code point / code unit
	/// sequence.
	///
	/// @remarks This class hooks into the encodings passed as the first parameter to the error handling functions to
	/// see if they define either `replacement_code_points()` or `replacement_code_units()` function. If so, they
	/// will call them and use the returned contiguous range to isnert code points or code units into the function. If
	/// neither of these exist, then it checks for a definition of a `maybe_replacement_code_points()` or a @c
	/// maybe_replacement_code_units() function. If either is present, they are expected to return a `std::optional`
	/// of a contiguous range. If it is engaged (the `std::optional` is filled) it will be used. Otherwise, if it is
	/// not engaged, then it will explicitly fall back to attempt to insert the default replacement character `U`+FFFD
	/// (<tt>U'�'</tt>) or <tt>?</tt> character. If the output is out of room for the desired object, then nothing will
	/// be inserted at all.
	//////
	class replacement_handler_t {
	public:
		//////
		/// @brief The function call for inserting replacement code units at the point of failure, before returning
		/// flow back to the caller of the encode operation.
		///
		/// @param[in] __encoding The Encoding that experienced the error.
		/// @param[in] __result The current state of the encode operation.
		//////
		template <typename _Encoding, typename _InputRange, typename _OutputRange, typename _State,
			typename _InputProgress, typename _OutputProgress>
		constexpr auto operator()(const _Encoding& __encoding,
			encode_result<_InputRange, _OutputRange, _State> __result, const _InputProgress&,
			const _OutputProgress&) const noexcept {
			using _CodeUnit = code_unit_t<_Encoding>;
			if (__result.error_code == encoding_error::insufficient_output_space) {
				// BAIL
				return __result;
			}

			auto __outit   = ranges::ranges_adl::adl_begin(__result.output);
			auto __outlast = ranges::ranges_adl::adl_end(__result.output);
			if (__outit == __outlast) {
				// BAIL
				return __result;
			}

			if constexpr (is_code_units_replaceable_v<_Encoding, _State&>) {
				return __txt_detail::__write_direct(
					__encoding, __encoding.replacement_code_units(__result.state), ::std::move(__result));
			}
			else if constexpr (is_code_units_replaceable_v<_Encoding>) {
				return __txt_detail::__write_direct(
					__encoding, __encoding.replacement_code_units(), ::std::move(__result));
			}
			else if constexpr (
				!(is_code_points_maybe_replaceable_v<_Encoding,
				       _State&> || is_code_points_maybe_replaceable_v<_Encoding>)&&is_unicode_code_point_v<_CodeUnit>) {
				constexpr _CodeUnit __replacements[1] = { static_cast<_CodeUnit>(__ztd_idk_detail_replacement) };
				return __txt_detail::__write_direct(__encoding, __replacements, ::std::move(__result));
			}
			else {
				if constexpr (is_code_units_maybe_replaceable_v<_Encoding, _State&>) {
					auto __maybe_direct_replacement = __encoding.maybe_replacement_code_units(__result.state);
					if (__maybe_direct_replacement) {
						const auto& __direct_replacement = *__maybe_direct_replacement;
						return __txt_detail::__write_direct(
							__encoding, __direct_replacement, ::std::move(__result));
					}
				}
				else if constexpr (is_code_units_maybe_replaceable_v<_Encoding>) {
					auto __maybe_direct_replacement = __encoding.maybe_replacement_code_units();
					if (__maybe_direct_replacement) {
						const auto& __direct_replacement = *__maybe_direct_replacement;
						return __txt_detail::__write_direct(
							__encoding, __direct_replacement, ::std::move(__result));
					}
				}
				using _InputCodePoint = code_point_t<_Encoding>;
				_InputCodePoint __replacement[max_code_points_v<_Encoding>] {};
				::std::size_t __replacement_size = 0;
				if constexpr (is_code_points_replaceable_v<_Encoding, _State&>) {
					auto __replacement_code_units = __encoding.replacement_code_points(__result.state);
					for (const auto& __element : __replacement_code_units) {
						__replacement[__replacement_size] = __element;
						++__replacement_size;
					}
				}
				else if constexpr (is_code_points_replaceable_v<_Encoding>) {
					auto __replacement_code_units = __encoding.replacement_code_points();
					for (const auto& __element : __replacement_code_units) {
						__replacement[__replacement_size] = __element;
						++__replacement_size;
					}
				}
				else {
					__replacement_size = __txt_detail::__fill_replacement_code_point_static(
						__encoding, __result.state, __replacement);
				}

				const ::ztd::span<const _InputCodePoint> __replacement_range(__replacement, __replacement_size);

				__txt_detail::__pass_through_handler __handler {};
				encode_state_t<_Encoding> __state = copy_encode_state_with(__encoding, __result.state);
				auto __encresult                  = __txt_detail::__basic_encode_one<__txt_detail::__consume::__no>(
                         __replacement_range, __encoding, ::std::move(__result.output), __handler, __state);
				__result.output = ::std::move(__encresult.output);
				if (__encresult.error_code != encoding_error::ok) {
					// we can't even encode a single code unit
					// into the stream... report error and bail
					return __result;
				}

				__result.error_code = encoding_error::ok;

				return __result;
			}
		}

		//////
		/// @brief The function call for inserting replacement code points at the point of failure, before returning
		/// flow back to the caller of the decode operation.
		///
		/// @param[in] __encoding The Encoding that experienced the error.
		/// @param[in] __result The current state of the encode operation.
		//////
		template <typename _Encoding, typename _InputRange, typename _OutputRange, typename _State,
			typename _InputProgress, typename _OutputProgress>
		constexpr auto operator()(const _Encoding& __encoding,
			decode_result<_InputRange, _OutputRange, _State> __result, const _InputProgress&,
			const _OutputProgress&) const noexcept {
			using _CodePoint = code_point_t<_Encoding>;

			if (__result.error_code == encoding_error::insufficient_output_space) {
				// BAIL
				return __result;
			}
			auto __outit   = ranges::ranges_adl::adl_begin(__result.output);
			auto __outlast = ranges::ranges_adl::adl_end(__result.output);
			if (__outit == __outlast) {
				// BAIL
				return __result;
			}

			if constexpr (is_code_points_replaceable_v<_Encoding, _State&>) {
				return __txt_detail::__write_direct(
					__encoding, __encoding.replacement_code_points(__result.state), ::std::move(__result));
			}
			else if constexpr (is_code_points_replaceable_v<_Encoding>) {
				return __txt_detail::__write_direct(
					__encoding, __encoding.replacement_code_points(), ::std::move(__result));
			}
			else if constexpr (
				!(is_code_points_maybe_replaceable_v<_Encoding,
				       _State&> || is_code_points_maybe_replaceable_v<_Encoding>)&&is_unicode_code_point_v<_CodePoint>) {
				constexpr _CodePoint __replacements[1] = { static_cast<_CodePoint>(__ztd_idk_detail_replacement) };
				return __txt_detail::__write_direct(__encoding, __replacements, ::std::move(__result));
			}
			else {
				if constexpr (is_code_points_maybe_replaceable_v<_Encoding, _State&>) {
					auto __maybe_direct_replacement = __encoding.maybe_replacement_code_points(__result.state);
					if (__maybe_direct_replacement) {
						const auto& __direct_replacement = *__maybe_direct_replacement;
						return __txt_detail::__write_direct(
							__encoding, __direct_replacement, ::std::move(__result));
					}
				}
				else if constexpr (is_code_points_maybe_replaceable_v<_Encoding>) {
					auto __maybe_direct_replacement = __encoding.maybe_replacement_code_points();
					if (__maybe_direct_replacement) {
						const auto& __direct_replacement = *__maybe_direct_replacement;
						return __txt_detail::__write_direct(
							__encoding, __direct_replacement, ::std::move(__result));
					}
				}
				using _InputCodeUnit = code_unit_t<_Encoding>;
				_InputCodeUnit __replacement[max_code_units_v<_Encoding>] {};
				::std::size_t __replacement_size = 0;
				if constexpr (is_code_units_replaceable_v<_Encoding, _State&>) {
					auto __replacement_code_units = __encoding.replacement_code_units(__result.state);
					for (const auto& __element : __replacement_code_units) {
						__replacement[__replacement_size] = __element;
						++__replacement_size;
					}
				}
				else if constexpr (is_code_units_replaceable_v<_Encoding>) {
					auto __replacement_code_units = __encoding.replacement_code_units();
					for (const auto& __element : __replacement_code_units) {
						__replacement[__replacement_size] = __element;
						++__replacement_size;
					}
				}
				else {
					__replacement_size = __txt_detail::__fill_replacement_code_unit_static(
						__encoding, __result.state, __replacement);
				}

				const ::ztd::span<const _InputCodeUnit> __replacement_range(__replacement, __replacement_size);

				__txt_detail::__pass_through_handler __handler {};
				decode_state_t<_Encoding> __state = copy_decode_state_with(__encoding, __result.state);
				auto __decresult                  = __txt_detail::__basic_decode_one<__txt_detail::__consume::__no>(
                         __replacement_range, __encoding, ::std::move(__result.output), __handler, __state);
				__result.output = ::std::move(__decresult.output);
				if (__decresult.error_code != encoding_error::ok) {
					// we can't even decode a single code unit
					// into the stream... report error and bail
					return __result;
				}

				__result.error_code = encoding_error::ok;
				return __result;
			}
		}
	};

	//////
	/// @brief A convenience variable for passing the replacement_handler_t handler to functions.
	inline constexpr replacement_handler_t replacement_handler = {};

	//////
	/// @brief This handler detects if the error code is an incomplete seqence, and sets the error code to being
	/// okay before returning.
	///
	/// @tparam _Encoding The encoding type which dictates the `code_unit` and `code_point` buffers to store in
	/// the handler to catch unused input from the last parameter of error handler invocations by the encoding.
	/// @tparam _ErrorHandler An error handler to invoke if the encoding error code is @b NOT an incomplete
	/// sequence.
	///
	/// @remarks This type is often useful in conjunction with an accumulation state or buffer, which can be very
	/// handy for I/O (e.g., Networking) operations.
	//////
	template <typename _Encoding, typename _ErrorHandler = default_handler_t>
	class incomplete_handler : private ebco<_ErrorHandler> {
	private:
		using __error_handler_base_t = ebco<_ErrorHandler>;
		using _CodeUnit              = code_unit_t<_Encoding>;
		using _CodePoint             = code_point_t<_Encoding>;

	public:
		//////
		/// @brief The underlying error handler type.

		using error_handler = _ErrorHandler;

		//////
		/// @brief Constructs a ztd::text::incomplete_handler with a default-constructed internal error handler.

		constexpr incomplete_handler() noexcept(::std::is_nothrow_default_constructible_v<__error_handler_base_t>)
		: __error_handler_base_t(), _M_code_points_size(), _M_code_units_size() {
		}

		//////
		/// @brief Constructs a ztd::text::incomplete_handler with the provided internal error handler object.
		///
		/// @param __error_handler The provided error handler object to copy in and use when the error is not an
		/// incomplete error.
		//////
		constexpr incomplete_handler(const _ErrorHandler& __error_handler) noexcept(
			::std::is_nothrow_constructible_v<__error_handler_base_t, const _ErrorHandler&>)
		: __error_handler_base_t(__error_handler)
		, _M_code_units()
		, _M_code_units_size()
		, _M_code_points()
		, _M_code_points_size() {
		}

		//////
		/// @brief Constructs a ztd::text::incomplete_handler with the provided internal error handler object.
		///
		/// @param __error_handler The provided error handler object to move in and use when the error is not an
		/// incomplete error.
		//////
		constexpr incomplete_handler(_ErrorHandler&& __error_handler) noexcept(
			::std::is_nothrow_constructible_v<__error_handler_base_t, _ErrorHandler&&>)
		: __error_handler_base_t(::std::move(__error_handler))
		, _M_code_units()
		, _M_code_units_size()
		, _M_code_points()
		, _M_code_points_size() {
		}

		//////
		/// @brief Returns the base error handler that is called when a non-incomplete error occurs.

		constexpr _ErrorHandler& base() & noexcept {
			return this->__error_handler_base_t::get_value();
		}

		//////
		/// @brief Returns the base error handler that is called when a non-incomplete error occurs.

		constexpr const _ErrorHandler& base() const& noexcept {
			return this->__error_handler_base_t::get_value();
		}

		//////
		/// @brief Returns the base error handler that is called when a non-incomplete error occurs.

		constexpr _ErrorHandler&& base() && noexcept {
			return this->__error_handler_base_t::get_value();
		}

		//////
		/// @brief Checks if the __result.error_code is ztd::text::encoding_error::incomplete_sequence, it saves the
		/// values from `__progress` and returns. Otherwise, invokes the provided error handler this object was
		/// constructed with.
		///
		/// @param[in] __encoding The Encoding that experienced the error.
		/// @param[in] __result The current state of the encode operation.
		/// @param[in] __input_progress Any code units or code points that were read but not yet used before the
		/// failure occurred. These will be stored in this handler.
		/// @param[in] __output_progress Any code points or code units that have not yet been written before the
		/// failure occurred. These will be stored in this handler.
		//////
		template <typename _Result, typename _InputProgress, typename _OutputProgress>
		constexpr auto operator()(const _Encoding& __encoding, _Result __result,
			const _InputProgress& __input_progress,
			const _OutputProgress& __output_progress) const& // clang-format hack
			noexcept(::std::is_nothrow_invocable_v<_ErrorHandler, const _Encoding&, _Result&&, const _InputProgress&,
			     const _OutputProgress&>) {
			if (__result.error_code == encoding_error::incomplete_sequence) {
				// it's incomplete and we are okay with that
				if constexpr (is_specialization_of_v<_Result, decode_result>) {
					this->_M_code_units_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						this->_M_code_units_size, this->_M_code_units.data());
					this->_M_code_points_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						this->_M_code_points_size, this->_M_code_points.data());
				}
				else {
					this->_M_code_units_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						this->_M_code_units_size, this->_M_code_units.data());
					this->_M_code_points_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						this->_M_code_points_size, this->_M_code_points.data());
				}
				return __result;
			}
			return this->get_value()(__encoding, ::std::move(__result), __input_progress, __output_progress);
		}

		//////
		/// @brief Checks if the __result.error_code is ztd::text::encoding_error::incomplete_sequence, it saves the
		/// values from `__progress` and returns. Otherwise, invokes the provided error handler this object was
		/// constructed with.
		///
		/// @param[in] __encoding The Encoding that experienced the error.
		/// @param[in] __result The current state of the encode operation.
		/// @param[in] __input_progress Any code units or code points that were read but not yet used before the
		/// failure occurred. These will be stored in this handler.
		/// @param[in] __output_progress Any code points or code units that have not yet been written before the
		/// failure occurred. These will be stored in this handler.
		//////
		template <typename _Result, typename _InputProgress, typename _OutputProgress>
		constexpr auto operator()(const _Encoding& __encoding, _Result __result,
			const _InputProgress& __input_progress,
			const _OutputProgress& __output_progress) & // clang-format hack
			noexcept(::std::is_nothrow_invocable_v<_ErrorHandler, const _Encoding&, _Result&&, const _InputProgress&,
			     const _OutputProgress&>) {
			if (__result.error_code == encoding_error::incomplete_sequence) {
				// it's incomplete and we are okay with that
				if constexpr (is_specialization_of_v<_Result, decode_result>) {
					this->_M_code_units_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						this->_M_code_units_size, this->_M_code_units.data());
					this->_M_code_points_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						this->_M_code_points_size, this->_M_code_points.data());
				}
				else {
					this->_M_code_units_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						this->_M_code_units_size, this->_M_code_units.data());
					this->_M_code_points_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						this->_M_code_points_size, this->_M_code_points.data());
				}
				return __result;
			}
			return this->get_value()(__encoding, ::std::move(__result), __input_progress, __output_progress);
		}

		//////
		/// @brief Checks if the __result.error_code is ztd::text::encoding_error::incomplete_sequence, it saves the
		/// values from `__progress` and returns. Otherwise, invokes the provided error handler this object was
		/// constructed with.
		///
		/// @param[in] __encoding The Encoding that experienced the error.
		/// @param[in] __result The current state of the encode operation.
		/// @param[in] __input_progress Any code units or code points that were read but not yet used before the
		/// failure occurred. These will be stored in this handler.
		/// @param[in] __output_progress Any code points or code units that have not yet been written before the
		/// failure occurred. These will be stored in this handler.
		//////
		template <typename _Result, typename _InputProgress, typename _OutputProgress>
		constexpr auto operator()(const _Encoding& __encoding, _Result __result,
			const _InputProgress& __input_progress,
			const _OutputProgress& __output_progress) && // clang-format hack
			noexcept(::std::is_nothrow_invocable_v<_ErrorHandler, const _Encoding&, _Result&&, const _InputProgress&,
			     const _OutputProgress&>) {
			if (__result.error_code == encoding_error::incomplete_sequence) {
				// it's incomplete and we are okay with that
				if constexpr (is_specialization_of_v<_Result, decode_result>) {
					this->_M_code_units_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						this->_M_code_units_size, this->_M_code_units.data());
					this->_M_code_points_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						this->_M_code_points_size, this->_M_code_points.data());
				}
				else {
					this->_M_code_units_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						this->_M_code_units_size, this->_M_code_units.data());
					this->_M_code_points_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						this->_M_code_points_size, this->_M_code_points.data());
				}
				return __result;
			}
			return this->get_value()(__encoding, ::std::move(__result), __input_progress, __output_progress);
		}

		//////
		/// @brief Returns the code units from the last incomplete decode operations.

		::ztd::span<_CodeUnit> code_units() const noexcept {
			return ::ztd::span<_CodeUnit>(this->_M_code_units.data(), this->_M_code_units_size);
		}

		//////
		/// @brief Returns the code points from the last incomplete encode operations.

		::ztd::span<_CodePoint> code_points() const noexcept {
			return ::ztd::span<_CodePoint>(this->_M_code_units.data(), this->_M_code_units_size);
		}

	private:
		mutable ::std::array<_CodeUnit, max_code_units_v<_Encoding>> _M_code_units;
		mutable ::std::size_t _M_code_units_size;
		mutable ::std::array<_CodePoint, max_code_points_v<_Encoding>> _M_code_points;
		mutable ::std::size_t _M_code_points_size;
	};

	//////
	/// @brief The default error handler for the entire library. Can be configured to use different strategies at build
	/// time. Without configuration, it defaults to the ztd::text::replacement_handler_t.
	//////
	class default_handler_t
#if ZTD_IS_ON(ZTD_TEXT_DEFAULT_HANDLER_THROWS_I_)
	: private throw_handler_t {
	private:
		using __error_handler_base_t = throw_handler_t;
#else
	: private replacement_handler_t {
	private:
		using __error_handler_base_t = replacement_handler_t;
#endif

	public:
		//////
		///@brief The underlying error handler type.
		using error_handler = __error_handler_base_t;

		using __error_handler_base_t::__error_handler_base_t;

		using __error_handler_base_t::operator();
	};

	//////
	/// @brief An instance of the default_handler_t type for ease of use.

	inline constexpr default_handler_t default_handler = {};

	//////
	/// @brief An incomplete handler that uses the default handler underneath.

	using default_incomplete_handler_t = incomplete_handler<default_handler_t>;

	namespace __txt_detail {
		template <typename _ErrorHandler>
		constexpr auto __duplicate_or_be_careless(_ErrorHandler& __original) {
			using _UErrorHandler = remove_cvref_t<_ErrorHandler>;
			if constexpr (!::std::is_function_v<_UErrorHandler>) {
				if constexpr (::std::is_copy_constructible_v<_UErrorHandler>) {
					return __original;
				}
				else {
					return default_handler_t {};
				}
			}
			else {
				return default_handler_t {};
			}
		}
	} // namespace __txt_detail

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text


#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_ERROR_HANDLER_HPP
