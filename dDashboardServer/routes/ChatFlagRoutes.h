#pragma once

/**
 * Chat flags: staff flag a chat message, or several from one conversation, for review. Each flag keeps a copy of the
 * flagged messages and the chat around them, has a status (open, actioned, dismissed), a note, a link to the character
 * and account it is about, and a history of who did what (also in the audit log). The Chat Flags page is the queue;
 * open flags count on the sidebar badge.
 */
namespace ChatFlagRoutes {
	void RegisterRoutes();
}
