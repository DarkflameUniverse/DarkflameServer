/* vanity_events.file_switches: vanity files the event switches on or off while it is on, as JSON
   {"halloween.xml": true, "summer.xml": false}, overriding the <file enabled> entries that name them (see
   dCommon/VanityEvents.h). Empty: none, as on every row from before. */
ALTER TABLE vanity_events ADD COLUMN file_switches TEXT NOT NULL DEFAULT '';
