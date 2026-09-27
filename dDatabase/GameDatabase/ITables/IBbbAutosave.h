#ifndef __IBBBAUTOSAVE__H__
#define __IBBBAUTOSAVE__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"

// The brick by brick model a character was building when the client last sent SetBBBAutosave (the client's quick save:
// every five minutes, before an AFK kick and before quitting). The server rebuilds it into a model when the build ends
// without a save (see docs/BuildWorkflow.md).
class IBbbAutosave {
public:
	struct Info {
		std::string lxfml; // sd0 compressed LXFML as the client sent it
		std::vector<LWOOBJID> sourceItems; // the models that were in the BBB inventory when it was sent
		int64_t updatedAt{}; // Unix seconds
	};

	virtual std::optional<IBbbAutosave::Info> GetBbbAutosave(const LWOOBJID characterId) = 0;

	// Replaces the character's autosave
	virtual void SetBbbAutosave(const LWOOBJID characterId, const IBbbAutosave::Info& info) = 0;

	virtual void DeleteBbbAutosave(const LWOOBJID characterId) = 0;
};

#endif //!__IBBBAUTOSAVE__H__
