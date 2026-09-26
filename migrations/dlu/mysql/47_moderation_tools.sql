/* Strike thresholds: the automatic step (eStrikeStep: WARN, MUTE, BAN) a strike set off and the number of strikes it was
   for, so the same step isn't applied twice for the same count. Statements for columns that already exist fail
   individually and are skipped. */
ALTER TABLE account_strikes ADD COLUMN step VARCHAR(16) NOT NULL DEFAULT '';
ALTER TABLE account_strikes ADD COLUMN step_count INT UNSIGNED NOT NULL DEFAULT 0;

/* Reports players send from the game's Report Abuse window. kind (ePlayerReportKind): PLAYER (ReportBug naming another
   player), MODEL (ReportOffensiveModel), PROPERTY (ReportOffensiveProperty). object_id is what the player picked;
   target_* is the player it is about (the reported player, or the owner of the model or property) when the world knew
   it. status (ePlayerReportStatus): 0 open, 1 actioned, 2 dismissed. */
CREATE TABLE IF NOT EXISTS player_reports (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    created_at BIGINT NOT NULL,
    kind VARCHAR(16) NOT NULL,
    reporter_id BIGINT NOT NULL DEFAULT 0,
    reporter_account_id INT UNSIGNED NOT NULL DEFAULT 0,
    object_id BIGINT NOT NULL DEFAULT 0,
    object_lot INT NOT NULL DEFAULT 0,
    target_character_id BIGINT NOT NULL DEFAULT 0,
    target_account_id INT UNSIGNED NOT NULL DEFAULT 0,
    property_id BIGINT NOT NULL DEFAULT 0,
    zone_id INT UNSIGNED NOT NULL DEFAULT 0,
    instance_id INT UNSIGNED NOT NULL DEFAULT 0,
    clone_id INT UNSIGNED NOT NULL DEFAULT 0,
    body TEXT NOT NULL,
    status TINYINT NOT NULL DEFAULT 0,
    handled_by VARCHAR(64) NOT NULL DEFAULT '',
    handled_at BIGINT NOT NULL DEFAULT 0,
    resolution TEXT NOT NULL,
    INDEX player_reports_status (status, id),
    INDEX player_reports_target (target_account_id)
);

/* Words staff add to the chat filter on the dashboard, on top of the files (chatplus_en_us.txt, blocklist.dcf).
   allowed = 1: accepted in whitelisted chat; 0: always stopped. Lower case. */
CREATE TABLE IF NOT EXISTS chat_filter_words (
    word VARCHAR(64) NOT NULL PRIMARY KEY,
    allowed TINYINT NOT NULL,
    added_by VARCHAR(64) NOT NULL DEFAULT '',
    added_at BIGINT NOT NULL
);
