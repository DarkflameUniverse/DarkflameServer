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

#ifndef ZTD_TEXT_DEFAULT_ENCODING_HPP
#define ZTD_TEXT_DEFAULT_ENCODING_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/execution.hpp>
#include <ztd/text/wide_execution.hpp>
#include <ztd/text/literal.hpp>
#include <ztd/text/wide_literal.hpp>
#include <ztd/text/utf8.hpp>
#include <ztd/text/utf16.hpp>
#include <ztd/text/utf32.hpp>
#include <ztd/text/encoding_scheme.hpp>
#include <ztd/text/no_encoding.hpp>

#include <ztd/text/unicode_code_point.hpp>
#include <ztd/text/unicode_scalar_value.hpp>

#include <ztd/text/type_traits.hpp>

#include <cstdint>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {
		template <typename _Type, bool _CompileTime>
		class __default_code_unit_encoding {
		private:
			// clang-format off
			using _ChosenType = ::std::conditional_t<
				::std::is_same_v<_Type, char>,
					::std::conditional_t<_CompileTime, literal_t, execution_t>,
					::std::conditional_t<::std::is_same_v<_Type, wchar_t>,
						::std::conditional_t<_CompileTime, wide_literal_t, wide_execution_t>,
						::std::conditional_t<::std::is_same_v<_Type, signed char>, basic_ascii<signed char>,
							::std::conditional_t<::std::is_same_v<_Type, uchar8_t>, utf8_t,
								::std::conditional_t<::std::is_same_v<_Type, unsigned char>, basic_utf8<unsigned char>,
#if ZTD_IS_ON(ZTD_NATIVE_CHAR8_T_I_)
									::std::conditional_t<::std::is_same_v<_Type, char8_t>, basic_utf8<char8_t>,
#endif
										::std::conditional_t<::std::is_same_v<_Type, char16_t>, utf16_t,
											::std::conditional_t<::std::is_same_v<_Type, char32_t>, utf32_t,
#if ZTD_IS_ON(ZTD_TEXT_UNICODE_CODE_POINT_DISTINCT_TYPE_I_)
												::std::conditional_t<::std::is_same_v<_Type, unicode_code_point>, basic_utf32<unicode_code_point>,
#endif
#if ZTD_IS_ON(ZTD_TEXT_UNICODE_SCALAR_VALUE_DISTINCT_TYPE_I_)
													::std::conditional_t<::std::is_same_v<_Type, unicode_scalar_value>, basic_utf32<unicode_scalar_value>,
#endif
														::std::conditional_t<::std::is_same_v<_Type, ::std::byte>, encoding_scheme<utf8_t, endian::native, ::std::byte>, basic_no_encoding<_Type, unicode_code_point>>
#if ZTD_IS_ON(ZTD_TEXT_UNICODE_SCALAR_VALUE_DISTINCT_TYPE_I_)
													>
#endif
#if ZTD_IS_ON(ZTD_TEXT_UNICODE_CODE_POINT_DISTINCT_TYPE_I_)
												>
#endif
											>
#if ZTD_IS_ON(ZTD_NATIVE_CHAR8_T_I_)
										>
#endif
									>
								>
							>
						>
					>
				>;
			// clang-format on

			static_assert(!is_specialization_of_v<_ChosenType, basic_no_encoding>,
				"there is no default encoding for the given code unit type");

		public:
			//////
			/// @brief The chosen type for the given code unit.
			///
			/// @remarks The default encodings for code unit types are as follows
			/// - `char` ➡ ztd::text::(runtime), ztd::text::literal_t (compiletime)
			/// - `wchar_t` ➡ ztd::text::(runtime), ztd::text::wide_literal_t (compiletime)
			/// - `char8_t` ➡ ztd::text::utf8
			/// - `ztd::uchar8_t` ➡ ztd::text::utf8_t (if different from `char8_t` type)
			/// - `std::byte` ➡ ztd::text::basic_utf8<std::byte>
			/// - `signed` char ➡ ztd::text::basic_ascii<signed char>
			/// - `char16_t` ➡ ztd::text::utf16
			/// - `char32_t` ➡ ztd::text::utf32
			/// - `unicode_code_point` ➡ ztd::text::utf32_t (if different from `char32_t` type)
			/// - `unicode_scalar_value` ➡ ztd::text::utf32_t (if different from `char32_t` type)
			//////
			using type = _ChosenType;
		};

		template <typename _Type, bool>
		class __default_code_point_encoding {
		private:
			static_assert(is_unicode_code_point_v<_Type> || ::std::is_same_v<_Type, char32_t>,
				"there is no default encoding for the given code point type");

		public:
			//////
			/// @brief The chosen type for the given code unit.
			///
			/// @remarks The default encodings for code point types are as follows
			/// - `char32_t` ➡ ztd::text::utf8
			/// - `unicode_code_point` ➡ ztd::text::utf8
			/// - `unicode_scalar_value` ➡ ztd::text::utf8
			//////
			using type = utf8_t;
		};
	} // namespace __txt_detail

	//////
	/// @addtogroup ztd_text_properties Property and Trait Helpers
	/// @{
	/////

	//////
	/// @brief The default encoding associated with a given code unit type, that serves as either input to a decode
	/// operation or output from an encode operation.
	///
	/// @tparam _Type The code unit type, with no cv-qualifiers
	//////
	template <typename _Type>
	class default_code_unit_encoding : public __txt_detail::__default_code_unit_encoding<_Type, false> { };

	//////
	/// @brief A `typename` alias for ztd::text::default_code_unit_encoding.
	///
	/// @tparam _Type The code unit type, with no cv-qualifiers
	//////
	template <typename _Type>
	using default_code_unit_encoding_t = typename default_code_unit_encoding<_Type>::type;

	//////
	/// @brief The default encoding associated with a given code unit type, that serves as either input to a decode
	/// operation or output from an encode operation. This uses the additional information that this is compiletime,
	/// not runtime, to help make the decision on what to do.
	///
	/// @tparam _Type The code unit type, with no cv-qualifiers
	//////
	template <typename _Type>
	class default_consteval_code_unit_encoding : public __txt_detail::__default_code_unit_encoding<_Type, true> { };

	//////
	/// @brief A `typename` alias for ztd::text::default_consteval_code_unit_encoding.
	///
	/// @tparam _Type The code unit type, with no cv-qualifiers
	//////
	template <typename _Type>
	using default_consteval_code_unit_encoding_t = typename default_consteval_code_unit_encoding<_Type>::type;

	//////
	/// @brief The default encoding associated with a given code point type, that serves as either input to an encode
	/// operation or output from decode operation.
	///
	/// @tparam _Type The code point type, with no cv-qualifiers
	//////
	template <typename _Type>
	class default_code_point_encoding : public __txt_detail::__default_code_point_encoding<_Type, false> { };

	//////
	/// @brief A `typename` alias for ztd::text::default_code_point_encoding.
	///
	/// @tparam _Type The code point type, with no cv-qualifiers
	//////
	template <typename _Type>
	using default_code_point_encoding_t = typename default_code_point_encoding<_Type>::type;

	//////
	/// @brief The default encoding associated with a given code point type, that serves as either input to an encode
	/// operation or output from decode operation. This uses additional information that this is at compile time, not
	/// run time, to help make a decision as to what to do.
	///
	/// @tparam _Type The code point type, with no cv-qualifiers
	//////
	template <typename _Type>
	class default_consteval_code_point_encoding : public __txt_detail::__default_code_point_encoding<_Type, true> { };

	//////
	/// @brief A `typename` alias for ztd::text::default_consteval_code_point_encoding.
	///
	/// @tparam _Type The code point type, with no cv-qualifiers
	//////
	template <typename _Type>
	using default_consteval_code_point_encoding_t = typename default_consteval_code_point_encoding<_Type>::type;

	//////
	/// @}
	/////

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DEFAULT_ENCODING_HPP
