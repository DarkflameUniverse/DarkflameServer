/*
 * Indexes for lookups by owner/parent that otherwise scan the whole table: a property's models (and the model count
 * on every property list row), a character's properties, an account's characters, a play key's accounts, friends
 * both ways, a character's mail, a leaderboard's scores, and the dashboard's per-character/account related data.
 * The columns' inline REFERENCES clauses in 0_initial.sql create no foreign key (and so no index) in MySQL/MariaDB.
 */
CREATE INDEX properties_contents_property ON properties_contents (property_id);
CREATE INDEX properties_contents_ugc ON properties_contents (ugc_id);
CREATE INDEX properties_owner ON properties (owner_id);
CREATE INDEX charinfo_account ON charinfo (account_id);
CREATE INDEX accounts_play_key ON accounts (play_key_id);
CREATE INDEX friends_friend ON friends (friend_id);
CREATE INDEX mail_receiver ON mail (receiver_id);
CREATE INDEX leaderboard_game ON leaderboard (game_id, character_id);
CREATE INDEX bug_reports_reporter ON bug_reports (reporter_id);
CREATE INDEX economy_flags_character ON economy_flags (character_id, id);
CREATE INDEX player_cheat_detections_account ON player_cheat_detections (account_id);
CREATE INDEX chat_log_recipient ON chat_log (recipient_id);
