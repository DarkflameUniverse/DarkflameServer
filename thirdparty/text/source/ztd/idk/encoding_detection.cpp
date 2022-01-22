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

#include <ztd/idk/version.h>

#include <ztd/idk/encoding_detection.h>

#include <ztd/idk/encoding_name.hpp>
#include <ztd/idk/detail/windows.hpp>
#include <ztd/idk/detail/posix.hpp>

#include <string_view>

#if ZTD_IS_ON(ZTD_LOCALE_DEPENDENT_WIDE_EXECUTION_I_)
#if ZTD_IS_ON(ZTD_CXX_I_)
#include <clocale>
#else
#include <locale.h>
#endif
#endif

ZTD_IDK_C_LANGUAGE_LINKAGE_I_ ZTD_IDK_API_LINKAGE_I_ bool ztdc_is_execution_encoding_unicode(
     void) ZTD_NOEXCEPT_IF_CXX_I_ {
#if ZTD_IS_ON(ZTD_PLATFORM_WINDOWS_I_)
	int __codepage_id = ::ztd::__idk_detail::__windows::__determine_active_code_page();
	return ::ztd::__idk_detail::__windows::__is_unicode_code_page(__codepage_id);
#else
#if ZTD_IS_ON(ZTD_NL_LANGINFO_I_) || ZTD_IS_ON(ZTD_LANGINFO_I_)
	const char* __ctype_name = nl_langinfo(CODESET);
#else
	const char* __ctype_name = setlocale(LC_CTYPE, nullptr);
#endif
	::std::string_view __adjusted_ctype_name(__ctype_name);
	::std::size_t __index = __adjusted_ctype_name.find_first_of(".");
	if (__index != ::std::string_view::npos) {
		__adjusted_ctype_name = __adjusted_ctype_name.substr(__index);
	}
	return ::ztd::is_unicode_encoding_name(__adjusted_ctype_name);
#endif
}

ZTD_IDK_C_LANGUAGE_LINKAGE_I_ ZTD_IDK_API_LINKAGE_I_ bool ztdc_is_wide_execution_encoding_unicode(
     void) ZTD_NOEXCEPT_IF_CXX_I_ {
#if ZTD_IS_ON(ZTD_PLATFORM_WINDOWS_I_)
	return true;
#elif ZTD_IS_ON(ZTD_LOCALE_DEPENDENT_WIDE_EXECUTION_I_)
	// TODO: implement!
	return false;
#elif ZTD_IS_ON(ZTD_WCHAR_T_UTF16_COMPATIBLE_I_) || ZTD_IS_ON(ZTD_WCHAR_T_UTF32_COMPATIBLE_I_)
	return true;
#else
	return false;
#endif
}


ZTD_IDK_C_LANGUAGE_LINKAGE_I_ ZTD_IDK_API_LINKAGE_I_ bool ztdc_is_execution_encoding_utf8(void) ZTD_NOEXCEPT_IF_CXX_I_ {
#if ZTD_IS_ON(ZTD_LIBVCXX_I_)
	return MB_CUR_MAX == 4;
#elif ZTD_IS_ON(ZTD_PLATFORM_WINDOWS_I_)
	return ::ztd::__idk_detail::__windows::__determine_active_code_page() == CP_UTF8;
#elif ZTD_IS_ON(ZTD_PLATFORM_MAC_OS_I_)
	return true;
#else
#if ZTD_IS_ON(ZTD_NL_LANGINFO_I_) || ZTD_IS_ON(ZTD_LANGINFO_I_)
	const char* __ctype_name = nl_langinfo(CODESET);
#else
	const char* __ctype_name = setlocale(LC_CTYPE, nullptr);
#endif
	::std::string_view __adjusted_ctype_name(__ctype_name);
	::std::size_t __index = __adjusted_ctype_name.find_first_of(".");
	if (__index != ::std::string_view::npos) {
		__adjusted_ctype_name = __adjusted_ctype_name.substr(__index);
	}
	return ::ztd::is_encoding_name_equal(__adjusted_ctype_name, "UTF-8");
#endif
}

ZTD_IDK_C_LANGUAGE_LINKAGE_I_ ZTD_IDK_API_LINKAGE_I_ bool ztdc_is_wide_execution_encoding_utf16(
     void) ZTD_NOEXCEPT_IF_CXX_I_ {
#if ZTD_IS_ON(ZTD_PLATFORM_WINDOWS_I_)
	return true;
#elif ZTD_IS_ON(ZTD_LOCALE_DEPENDENT_WIDE_EXECUTION_I_)
	// TODO: implement!
	return false;
#elif ZTD_IS_ON(ZTD_WCHAR_T_UTF16_COMPATIBLE_I_)
	return true;
#else
	return false;
#endif
}

ZTD_IDK_C_LANGUAGE_LINKAGE_I_ ZTD_IDK_API_LINKAGE_I_ bool ztdc_is_wide_execution_encoding_utf32(
     void) ZTD_NOEXCEPT_IF_CXX_I_ {
#if ZTD_IS_ON(ZTD_PLATFORM_WINDOWS_I_)
	return false;
#elif ZTD_IS_ON(ZTD_LOCALE_DEPENDENT_WIDE_EXECUTION_I_)
	// TODO: implement!
	return false;
#elif ZTD_IS_ON(ZTD_WCHAR_T_UTF32_COMPATIBLE_I_)
	return true;
#else
	return false;
#endif
}
