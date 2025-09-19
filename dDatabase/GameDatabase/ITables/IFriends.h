#ifndef __IFRIENDS__H__
#define __IFRIENDS__H__

#include <cstdint>
#include <optional>
#include <vector>

class IFriends {
public:
	struct BestFriendStatus {
		LWOOBJID playerCharacterId{};
		LWOOBJID friendCharacterId{};
		uint32_t bestFriendStatus{};
	};

	// Get the friends list for the given character id.
	virtual std::vector<FriendData> GetFriendsList(const LWOOBJID charId) = 0;

	// Get the best friend status for the given player and friend character ids.
	virtual std::optional<IFriends::BestFriendStatus> GetBestFriendStatus(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId) = 0;

	// Set the best friend status for the given player and friend character ids.
	virtual void SetBestFriendStatus(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId, const uint32_t bestFriendStatus) = 0;

	// Add a friend to the given character id.
	virtual void AddFriend(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId) = 0;

	// Remove a friend from the given character id.
	virtual void RemoveFriend(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId) = 0;
};

#endif  //!__IFRIENDS__H__
