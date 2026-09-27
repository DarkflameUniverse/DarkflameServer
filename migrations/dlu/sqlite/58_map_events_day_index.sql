/* map_events_daily by day, for the economy compaction and the reports over every zone: see the MySQL migration */
CREATE INDEX IF NOT EXISTS map_events_daily_day ON map_events_daily (day);
