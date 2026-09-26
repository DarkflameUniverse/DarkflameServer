/*
 * Indexes for lookups by owner/parent that otherwise scan the whole table: a property's models (and the model count
 * on every property list row), a character's properties, an account's characters, a play key's accounts, friends
 * both ways, a character's mail, a leaderboard's scores, and the dashboard's per-character/account related data.
 * Whether the columns' inline REFERENCES clauses in 0_initial.sql made a foreign key (and with it an index) depends on
 * the server: MySQL before 9.0 ignores them, MariaDB 10.5 and later creates them. So each index on such a column is
 * only created when no index starts with that column yet; a second one would only slow down writes.
 */
SET @dlu_index = (SELECT IF(COUNT(*) = 0, 'CREATE INDEX properties_contents_property ON properties_contents (property_id)', 'DO 0') FROM information_schema.statistics
	WHERE table_schema = DATABASE() AND table_name = 'properties_contents' AND column_name = 'property_id' AND seq_in_index = 1);
PREPARE dlu_index_stmt FROM @dlu_index;
EXECUTE dlu_index_stmt;
DEALLOCATE PREPARE dlu_index_stmt;
SET @dlu_index = (SELECT IF(COUNT(*) = 0, 'CREATE INDEX properties_contents_ugc ON properties_contents (ugc_id)', 'DO 0') FROM information_schema.statistics
	WHERE table_schema = DATABASE() AND table_name = 'properties_contents' AND column_name = 'ugc_id' AND seq_in_index = 1);
PREPARE dlu_index_stmt FROM @dlu_index;
EXECUTE dlu_index_stmt;
DEALLOCATE PREPARE dlu_index_stmt;
SET @dlu_index = (SELECT IF(COUNT(*) = 0, 'CREATE INDEX properties_owner ON properties (owner_id)', 'DO 0') FROM information_schema.statistics
	WHERE table_schema = DATABASE() AND table_name = 'properties' AND column_name = 'owner_id' AND seq_in_index = 1);
PREPARE dlu_index_stmt FROM @dlu_index;
EXECUTE dlu_index_stmt;
DEALLOCATE PREPARE dlu_index_stmt;
SET @dlu_index = (SELECT IF(COUNT(*) = 0, 'CREATE INDEX charinfo_account ON charinfo (account_id)', 'DO 0') FROM information_schema.statistics
	WHERE table_schema = DATABASE() AND table_name = 'charinfo' AND column_name = 'account_id' AND seq_in_index = 1);
PREPARE dlu_index_stmt FROM @dlu_index;
EXECUTE dlu_index_stmt;
DEALLOCATE PREPARE dlu_index_stmt;
SET @dlu_index = (SELECT IF(COUNT(*) = 0, 'CREATE INDEX accounts_play_key ON accounts (play_key_id)', 'DO 0') FROM information_schema.statistics
	WHERE table_schema = DATABASE() AND table_name = 'accounts' AND column_name = 'play_key_id' AND seq_in_index = 1);
PREPARE dlu_index_stmt FROM @dlu_index;
EXECUTE dlu_index_stmt;
DEALLOCATE PREPARE dlu_index_stmt;
SET @dlu_index = (SELECT IF(COUNT(*) = 0, 'CREATE INDEX friends_friend ON friends (friend_id)', 'DO 0') FROM information_schema.statistics
	WHERE table_schema = DATABASE() AND table_name = 'friends' AND column_name = 'friend_id' AND seq_in_index = 1);
PREPARE dlu_index_stmt FROM @dlu_index;
EXECUTE dlu_index_stmt;
DEALLOCATE PREPARE dlu_index_stmt;
SET @dlu_index = (SELECT IF(COUNT(*) = 0, 'CREATE INDEX mail_receiver ON mail (receiver_id)', 'DO 0') FROM information_schema.statistics
	WHERE table_schema = DATABASE() AND table_name = 'mail' AND column_name = 'receiver_id' AND seq_in_index = 1);
PREPARE dlu_index_stmt FROM @dlu_index;
EXECUTE dlu_index_stmt;
DEALLOCATE PREPARE dlu_index_stmt;
SET @dlu_index = (SELECT IF(COUNT(*) = 0, 'CREATE INDEX player_cheat_detections_account ON player_cheat_detections (account_id)', 'DO 0') FROM information_schema.statistics
	WHERE table_schema = DATABASE() AND table_name = 'player_cheat_detections' AND column_name = 'account_id' AND seq_in_index = 1);
PREPARE dlu_index_stmt FROM @dlu_index;
EXECUTE dlu_index_stmt;
DEALLOCATE PREPARE dlu_index_stmt;
CREATE INDEX leaderboard_game ON leaderboard (game_id, character_id);
CREATE INDEX bug_reports_reporter ON bug_reports (reporter_id);
CREATE INDEX economy_flags_character ON economy_flags (character_id, id);
CREATE INDEX chat_log_recipient ON chat_log (recipient_id);
