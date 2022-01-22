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

#ifndef ZTD_TEXT_DETAIL_ICONV_HPP
#define ZTD_TEXT_DETAIL_ICONV_HPP

#include <ztd/text/version.hpp>

#include <ztd/idk/detail/windows.hpp>
#include <ztd/idk/detail/posix.hpp>

#if ZTD_IS_ON(ZTD_LIBICONV_I_)

#if ZTD_IS_ON(ZTD_ICONV_H_I_)
#include <iconv.h>
#elif ZTD_IS_ON(ZTD_LIBICONV_LOAD_I_)
#if ZTD_IS_ON(ZTD_PLATFORM_POSIX_I_)
#elif ZTD_IS_ON(ZTD_PLATFORM_WINDOWS_I_)
// taken care of above
#else
#error "[ztd.text] Cannot find a runtime loading mechanism to use for this platform!"
#endif
#endif

#include <array>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {

	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail { namespace __iconv {

#if ZTD_IS_ON(ZTD_ICONV_H_I_)
		using __descriptor = iconv_t;
#else
		using __descriptor = void*;
#endif

		using __convert_function = size_t(__descriptor, char**, ::std::size_t*, char**, ::std::size_t*) noexcept;
		using __open_function    = __descriptor(const char*, const char*) noexcept;
		using __close_function   = int(__descriptor) noexcept;

		class __startup {
		public:
			__convert_function* __convert;
			__open_function* __open;
			__close_function* __close;

			__startup()
			: __convert(nullptr)
			, __open(nullptr)
			, __close(nullptr)
#if ZTD_IS_ON(ZTD_LIBICONV_LOAD_I_)
			, _M_handle(nullptr)
#endif
			{
#if ZTD_IS_ON(ZTD_LIBICONV_LOAD_I_)
#if ZTD_IS_ON(ZTD_PLATFORM_POSIX_I_) && ZTD_IS_ON(ZTD_DLFCN_H_I_)
				constexpr ::std::array<const char*, 6> __lib_names { { "libiconv.so", "iconv.so", "libiconv",
					"iconv", "iconv.dll", "libiconv.dll", "libc.so", "libc" } };
				for (::std::size_t __index = 0; __index < std::size(__lib_names); ++__index) {
					const char* __lib_name = __lib_names[__index];
					this->_M_handle        = dlopen(__lib_name, RTLD_LAZY);
					if (this->_M_handle == nullptr) {
						continue;
					}
					constexpr ::std::array<const char*, 2> __convert_names = { { "iconv", "_iconv" } };
					for (::std::size_t __convert_index = 0; __convert_index < __convert_names.size();
						++__convert_index) {
						const char* __convert_name = __convert_names[__convert_index];
						void* __convert_f          = dlsym(this->_M_handle, __convert_name);
						if (__convert_f == nullptr) {
							continue;
						}
						this->__convert = reinterpret_cast<__convert_function*>(__convert_f);
						break;
					}
					constexpr ::std::array<const char*, 2> __open_names = { { "iconv_open", "_iconv_open" } };
					for (::std::size_t __open_index = 0; __open_index < __open_names.size(); ++__open_index) {
						const char* __open_name = __open_names[__open_index];
						void* __open_f          = dlsym(this->_M_handle, __open_name);
						if (__open_f == nullptr) {
							continue;
						}
						this->__open = reinterpret_cast<__open_function*>(__open_f);
						break;
					}
					constexpr ::std::array<const char*, 2> __close_names = { { "iconv_close", "_iconv_close" } };
					for (::std::size_t __close_index = 0; __close_index < __close_names.size(); ++__close_index) {
						const char* __close_name = __close_names[__close_index];
						void* __close_f          = dlsym(this->_M_handle, __close_name);
						if (__close_f == nullptr) {
							continue;
						}
						this->__close = reinterpret_cast<__close_function*>(__close_f);
						break;
					}
					if (this->__open == nullptr || this->__close == nullptr || this->__convert == nullptr) {
						this->__open    = nullptr;
						this->__close   = nullptr;
						this->__convert = nullptr;
					}
					else {
						// we've got everything: leave!
						break;
					}
					if (this->_M_handle != nullptr) {
						dlclose(this->_M_handle);
						this->_M_handle = nullptr;
					}
				}

#else
#error "Not yet implemented, message Shepherd's Oasis about missing functionality for your platform!"
#endif
#else
				__convert = reinterpret_cast<__convert_function*>(&iconv);
				__open    = reinterpret_cast<__open_function*>(&iconv_open);
				__close   = reinterpret_cast<__close_function*>(&iconv_close);
#endif
			}

			bool is_valid() const noexcept {
				return this->__convert != nullptr && this->__open != nullptr && this->__close != nullptr
#if ZTD_IS_ON(ZTD_LIBICONV_LOAD_I_)
					&& this->_M_handle != nullptr
#endif
					;
			}

			~__startup() {
#if ZTD_IS_ON(ZTD_LIBICONV_LOAD_I_)
				if (this->_M_handle != nullptr) {
					dlclose(this->_M_handle);
				}
#endif
			}

		private:
#if ZTD_IS_ON(ZTD_LIBICONV_LOAD_I_)
			void* _M_handle;
#endif
		};

		inline const __descriptor __failure_descriptor      = reinterpret_cast<__descriptor>(-1);
		inline constexpr ::std::size_t __conversion_failure = static_cast<::std::size_t>(-1);
		inline constexpr ::std::size_t __conversion_success = static_cast<::std::size_t>(0);
		inline constexpr int __close_failure                = static_cast<int>(-1);
		inline constexpr int __close_success                = static_cast<int>(0);

		inline bool __descriptor_is_valid(__descriptor __desc) noexcept {
			return __desc != __failure_descriptor;
		}
	}} // namespace __txt_detail::__iconv

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_

}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif

#endif // ZTD_TEXT_DETAIL_ICONV_HPP
