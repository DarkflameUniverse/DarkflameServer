#ifndef MINIMUMCHATMODE_H
#define MINIMUMCHATMODE_H

#include "eGameMasterLevel.h"

#include <algorithm>
#include <cstdint>
#include <vector>

// The answer to a client's RequestMinimumChatMode: the lowest chat mode of everyone who would read a message in that
// channel. The client asks only for private (7), team (8) and local team (10) chat and answers every other channel
// itself with its own chat mode (LWOChatComponent::RequestMinimumChatMode). A chat mode is what DLU sends as the
// player's chat mode everywhere else: their GM level. Live answered 0 in all 74 captured answers (team chat among
// players who were not GMs); taking the minimum for teams with GMs in them is inferred from the name.
namespace MinimumChatMode {
	inline uint8_t Of(const std::vector<eGameMasterLevel>& readers) {
		if (readers.empty()) return 0;
		return static_cast<uint8_t>(*std::min_element(readers.begin(), readers.end()));
	}
}

#endif // MINIMUMCHATMODE_H
