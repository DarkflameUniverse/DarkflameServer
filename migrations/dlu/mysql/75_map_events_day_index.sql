/* map_events_daily's key starts with the zone, so everything that reads it by day alone read the whole table: the
   economy compaction (twice per day it merges into a month, a whole table scan each) and the reports over every zone.
   The index holds only the day and the key, so adding events to a row (the game's upsert) never has to update it. */
CREATE INDEX map_events_daily_day ON map_events_daily (day);
