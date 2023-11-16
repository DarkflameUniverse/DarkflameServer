#pragma once

#include <cstdint>

#ifndef __EPERMISSIONMAP__H__
#define __EPERMISSIONMAP__H__

/**
 * Bitmap of permissions and restrictions for characters.
 */
enum class ePermissionMap : uint64_t {
	/**
	 * Reserved for future use, bit 0-3.
	 */

	/**
	 * The character has restricted trade acccess, bit 4.
	 */
	RestrictedTradeAccess = 0x1 << 4,

	/**
	 * The character has restricted mail access, bit 5.
	 */
	RestrictedMailAccess = 0x1 << 5,

	/**
	 * The character has restricted chat access, bit 6.
	 */
	RestrictedChatAccess = 0x1 << 6,
};

#endif  //!__EPERMISSIONMAP__H__
