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

#ifndef ZTD_TEXT_ENCODING_ERROR_HPP
#define ZTD_TEXT_ENCODING_ERROR_HPP

#include <ztd/text/version.hpp>

#include <ztd/idk/charN_t.hpp>
#include <ztd/text/unicode_code_point.hpp>
#include <ztd/text/detail/to_underlying.hpp>

#include <cstddef>
#include <system_error>
#include <type_traits>
#include <string>
#include <string_view>
#include <exception>
#include <array>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @addtogroup ztd_text_encoding_error ztd::text::encoding_error
	/// @brief This enumeration describes the kind of failures that can happen in ztd_text_transcode,
	/// ztd_text_decode, ztd_text_encode, and lower-level operations.
	/// @{
	//////

	//////
	/// @brief Describes a failure to encode, decode, transcode, or count, for four core various reasons.
	///
	/// @remarks This does not cover specific failures, like if a sequence was overlong (e.g., UTF-8) or if an encode
	/// operation produced an uunpaired surrogate value (e.g. UTF-16).
	//////
	enum class encoding_error : int {
		//////
		/// @brief The okay status; everything is fine.
		///
		/// @remarks This does not necessarily mean an error handler was not called. An error handler can set the
		/// error code to ztd::text::encoding_error::ok after performing corrective action: see
		/// ztd::text::replacement_handler_t for an example.
		//////
		ok = 0x00,
		//////
		/// Input contains ill-formed sequences. This means there were available units of input to read, but what was
		/// read resulted in an error.
		//////
		invalid_sequence = 0x01,
		//////
		/// Input contains incomplete sequences. This means that the input was exhausted, without finding an invalid
		/// sequence, and therefore more input may be required.
		///
		/// @remarks Depending on context, this may or may not be an error in your use case (e.g., reading part of an
		/// incomplete network buffer and waiting for more). See ztd::text::incomplete_handler as a way to aid with
		/// this use case.
		//////
		incomplete_sequence = 0x02,
		//////
		/// Output cannot receive the successfully encoded or decoded sequence. This means that, while there were no
		/// invalid or incomplete sequences in the input, the output ran out of space to receive it.
		///
		/// @remarks Provide a bigger storage area or guarantee that it meets the minimum required size for potential
		/// output. This can be queried for an encoding by using ztd::text::max_code_points_v<the_encoding> for code
		/// points, and ztd::text::max_code_units_v<the_encoding> for code units.
		//////
		insufficient_output_space = 0x03,
#if 0
		/* These error types are not used because they are too specific to Unicode. I suspect that some people will
		   find them useful for error reporting cases, but it does result in more checking for higher-level APIs. It's
		   best to leave them off, for now. */

		//////
		/// Input contains overlong encoding sequence. This is specific to certain encodings, such as Unicode
		/// Transformation Formats like UTF-8 where different sequences can end up encoding the same character but are
		/// not allowed.
		//////
		invalid_sequence = 0x04,
		//////
		/// leading code unit is wrong
		
		invalid_sequence = 0x05,
		//////
		/// leading code units were correct, trailing code units were wrong
		
		invalid_sequence = 0x06
#endif
	};

	//////
	/// @brief Converts an encoding_error to a string value.
	///
	/// @returns A null-terminated string_view to the data.
	///
	/// @remarks If a value outside of the allowed encoding_error is passed, then undefined behavior happens.
	//////
	inline constexpr ::std::string_view to_name(encoding_error __error_code) {
		constexpr ::std::array<::std::string_view, 4> __translation { { "ok", "invalid_sequence",
			"incomplete_sequence", "insufficient_output_space" } };
		return __translation[static_cast<::std::size_t>(__txt_detail::__to_underlying(__error_code))];
	}

	namespace __txt_detail {
		//////
		/// @brief The encoding category for system_error-style exceptions and errors.
		///
		/// @remarks This type is generally paired with a std::error_condition.
		/////
		class __encoding_category : public ::std::error_category {
			virtual const char* name() const noexcept override {
				return "encoding_error";
			}
			virtual ::std::string message(int __untyped_error_code) const override {
				encoding_error __error_code = static_cast<encoding_error>(__untyped_error_code);
				switch (__error_code) {
				case encoding_error::ok:
				case encoding_error::incomplete_sequence:
				case encoding_error::insufficient_output_space:
				case encoding_error::invalid_sequence: {
					::std::string_view __name = ::ztd::text::to_name(__error_code);
					return ::std::string(__name.data(), __name.size());
				}
				}
				return "unrecognized untyped error code";
			}

			virtual ::std::error_condition default_error_condition(
				int __untyped_error_code) const noexcept override {
				return ::std::error_condition(__untyped_error_code, *this);
			}
		};
	} // namespace __txt_detail

	//////
	/// @brief The system_error category type to be used with std::error_category-requiring errors.
	///
	/// @remarks Not constexpr in general because of ABI shenanigans from the 2019 Belfast Meeting, where it did not
	/// successfully pass through Incubator review.
	//////
	inline const ::std::error_category& encoding_category() {
		static const __txt_detail::__encoding_category __category = __txt_detail::__encoding_category();
		return __category;
	}

	//////
	/// @}


	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

namespace std {
	template <>
	struct is_error_condition_enum<::ztd::text::encoding_error> : public ::std::true_type { };

	template <>
	class hash<::ztd::text::encoding_error> {
	private:
		using _UnderlyingErrorType = ::std::underlying_type_t<::ztd::text::encoding_error>;

	public:
		::std::size_t operator()(::ztd::text::encoding_error __error_code) const noexcept(
		     noexcept(::std::hash<_UnderlyingErrorType> {}(static_cast<_UnderlyingErrorType>(__error_code)))) {
			using _UnderlyingErrorType = ::std::underlying_type_t<::ztd::text::encoding_error>;
			::std::hash<_UnderlyingErrorType> __hasher {};
			return __hasher(static_cast<_UnderlyingErrorType>(__error_code));
		}
	};
} // namespace std

#endif // ZTD_TEXT_ENCODING_ERROR_HPP
