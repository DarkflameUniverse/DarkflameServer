#pragma once

/**
 * The Contraband page: items staff don't want players to have (IContraband). Each has a reason and says whether a
 * character holding one is only flagged or also has it removed (see dGame/dUtilities/Contraband.h). Viewing needs
 * reports_view, changing the list contraband_manage; every change is audited and running worlds reload the list.
 * What was found shows up as economy flags of kind Contraband (Economy page, and the character's related data).
 */
namespace ContrabandRoutes {
	void RegisterRoutes();
}
