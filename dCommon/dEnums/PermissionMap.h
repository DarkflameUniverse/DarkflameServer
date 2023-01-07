#pragma once

#include <cstdint>

/**
 * Bitmap of permissions and restrictions for characters.
 */
enum class PermissionMap : uint64_t
{
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

	  //
	  // Combined permissions
	  //

	  /**
	   * The character is marked as 'old', restricted from trade and mail.
	   */
	   Old = RestrictedTradeAccess | RestrictedMailAccess,

	   /**
		* The character is soft banned, restricted from trade, mail, and chat.
		*/
		SoftBanned = RestrictedTradeAccess | RestrictedMailAccess | RestrictedChatAccess,
};
