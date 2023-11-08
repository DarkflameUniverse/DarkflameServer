#ifndef __IFRIENDS__H__
#define __IFRIENDS__H__

#include <cstdint>
#include <optional>
#include <vector>

class IFriends {
public:
	struct BestFriendStatus {
		uint32_t playerAccountId{};
		uint32_t friendAccountId{};
		uint32_t bestFriendStatus{};
	};

	// Get the friends list for the given character id.
	virtual std::vector<FriendData> GetFriendsList(const uint32_t charId) = 0;

	// Get the best friend status for the given player and friend character ids.
	virtual std::optional<IFriends::BestFriendStatus> GetBestFriendStatus(const uint32_t playerCharacterId, const uint32_t friendCharacterId) = 0;

	// Set the best friend status for the given player and friend character ids.
	virtual void SetBestFriendStatus(const uint32_t playerCharacterId, const uint32_t friendCharacterId, const uint32_t bestFriendStatus) = 0;

	// Add a friend to the given character id.
	virtual void AddFriend(const uint32_t playerCharacterId, const uint32_t friendCharacterId) = 0;

	// Remove a friend from the given character id.
	virtual void RemoveFriend(const uint32_t playerCharacterId, const uint32_t friendCharacterId) = 0;
};

#endif  //!__IFRIENDS__H__
