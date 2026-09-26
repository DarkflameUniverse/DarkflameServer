#pragma once

#include <cstdint>
#include <string>

/**
 * Who staff may use their tools on: the self and rank rules shared by the dashboard (RouteUtils) and the in-game slash
 * commands (SlashCommandHandler). The functions here are pure; Permissions.h decides the self_* and manage_equal_rank
 * levels they are given.
 */
namespace AccountRules {
	// GM 9: may do anything to anyone, including themselves and other GM 9s
	constexpr uint8_t OPERATOR_LEVEL = 9;

	/**
	 * What a staff action on an account does. It decides which permission a staff member below GM 9 needs to use the
	 * tool on their own account or characters (they still need the tool's own permission too).
	 */
	enum class eAccountAction : uint8_t {
		TOOLS,      // nothing is gained: rescue or move a character, kick, sign out everywhere, email a reset link (self_tools)
		ITEMS,      // gives items, coins or progress: edit or restore characters, missions, mail items (self_items)
		MODERATION, // changes a moderation record or account security: ban, mute, lock, strikes, GM level, password, delete (self_moderation)
	};

	// The permission that lets staff below GM 9 do this kind of action to their own account
	inline const char* SelfPermission(eAccountAction action) {
		switch (action) {
		case eAccountAction::TOOLS: return "self_tools";
		case eAccountAction::ITEMS: return "self_items";
		default: return "self_moderation";
		}
	}

	// The permission that lets staff below GM 9 act on accounts with their own GM level
	constexpr const char* EQUAL_RANK_PERMISSION = "manage_equal_rank";

	// Why an actor may not act on an account (NONE: they may)
	enum class eManageDenial : uint8_t { NONE, SELF, EQUAL_RANK, HIGHER_RANK };

	/**
	 * Whether an actor may act on a target account. GM 9 may act on anyone, themselves included. Below GM 9: their own
	 * account only with selfAllowed (the self_* permission for the action), an account at their own level only with
	 * equalAllowed (manage_equal_rank), and never an account above their own level, whatever the permissions say.
	 */
	inline eManageDenial ManageDenial(uint8_t actorLevel, uint32_t actorAccountId, uint8_t targetLevel, uint32_t targetAccountId, bool selfAllowed, bool equalAllowed) {
		if (actorLevel >= OPERATOR_LEVEL) return eManageDenial::NONE;
		if (actorAccountId != 0 && actorAccountId == targetAccountId) return selfAllowed ? eManageDenial::NONE : eManageDenial::SELF;
		if (targetLevel > actorLevel) return eManageDenial::HIGHER_RANK;
		if (targetLevel == actorLevel && !equalAllowed) return eManageDenial::EQUAL_RANK;
		return eManageDenial::NONE;
	}

	inline bool CanManageAccount(uint8_t actorLevel, uint32_t actorAccountId, uint8_t targetLevel, uint32_t targetAccountId, bool selfAllowed, bool equalAllowed) {
		return ManageDenial(actorLevel, actorAccountId, targetLevel, targetAccountId, selfAllowed, equalAllowed) == eManageDenial::NONE;
	}

	/**
	 * Safety rail for the server, not a limit on the operator: demoting, banning, locking or deleting a GM 9 account is
	 * refused when no other GM 9 account that can still sign in (not banned or locked) would be left.
	 */
	inline bool RemovesLastOperator(uint8_t targetLevel, uint32_t otherActiveOperators) {
		return targetLevel >= OPERATOR_LEVEL && otherActiveOperators == 0;
	}

	// Whether an actor may grant a GM level: never above their own, and only operators may create peers
	inline bool CanGrantGmLevel(uint8_t actorLevel, uint8_t newLevel) {
		if (newLevel > OPERATOR_LEVEL) return false;
		return actorLevel >= OPERATOR_LEVEL || newLevel < actorLevel;
	}

	// Why an action was refused, for the person who tried it (empty for NONE)
	inline std::string DenialMessage(eManageDenial denial, eAccountAction action) {
		switch (denial) {
		case eManageDenial::SELF:
			return std::string("Your GM level may not do this to your own account or characters (the ") + SelfPermission(action) + " permission)";
		case eManageDenial::EQUAL_RANK:
			return std::string("You cannot manage an account with the same GM level as yours (the ") + EQUAL_RANK_PERMISSION + " permission)";
		case eManageDenial::HIGHER_RANK:
			return "You cannot manage an account with a higher GM level than yours";
		default:
			return "";
		}
	}

	// The last-operator refusal, e.g. what = "banned"
	inline std::string LastOperatorMessage(const std::string& what) {
		return "This is the last GM 9 account that can sign in, so it can't be " + what +
			". Make another GM 9 account first, so the server always has someone who can manage it.";
	}

	/**
	 * ManageDenial with the self_* and manage_equal_rank levels as the Permissions page (or the config) sets them now.
	 * Used by the dashboard and the world servers alike.
	 */
	eManageDenial ManageDenialNow(uint8_t actorLevel, uint32_t actorAccountId, uint8_t targetLevel, uint32_t targetAccountId, eAccountAction action);
}
