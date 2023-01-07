#ifndef __LUTRIGGERS__H__
#define __LUTRIGGERS__H__

#include <string>
#include <vector>

class Command;
class Event;

namespace LUTriggers {
	struct Command {
		std::string id;
		std::string target;
		std::string targetName;
		std::string args;
	};

	struct Event {
		std::string eventID;
		std::vector<Command*> commands;
	};

	struct Trigger {
		uint32_t id;
		bool enabled;
		std::vector<Event*> events;
	};
};

#endif  //!__LUTRIGGERS__H__
