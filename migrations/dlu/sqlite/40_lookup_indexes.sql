/* Indexes for lookups by owner/parent that otherwise scan the whole table. See the MySQL migration. */
CREATE INDEX IF NOT EXISTS properties_contents_property ON properties_contents (property_id);
CREATE INDEX IF NOT EXISTS properties_contents_ugc ON properties_contents (ugc_id);
CREATE INDEX IF NOT EXISTS properties_owner ON properties (owner_id);
CREATE INDEX IF NOT EXISTS charinfo_account ON charinfo (account_id);
CREATE INDEX IF NOT EXISTS accounts_play_key ON accounts (play_key_id);
CREATE INDEX IF NOT EXISTS friends_friend ON friends (friend_id);
CREATE INDEX IF NOT EXISTS mail_receiver ON mail (receiver_id);
CREATE INDEX IF NOT EXISTS leaderboard_game ON leaderboard (game_id, character_id);
CREATE INDEX IF NOT EXISTS bug_reports_reporter ON bug_reports (reporter_id);
CREATE INDEX IF NOT EXISTS economy_flags_character ON economy_flags (character_id, id);
CREATE INDEX IF NOT EXISTS player_cheat_detections_account ON player_cheat_detections (account_id);
CREATE INDEX IF NOT EXISTS chat_log_recipient ON chat_log (recipient_id);
