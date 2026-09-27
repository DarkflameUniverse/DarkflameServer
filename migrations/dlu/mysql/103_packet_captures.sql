/* Packet captures (docs/CaptureReplay.md) are saved alongside the game message inspector's captures: capture_kind 0 is
   a game message capture (its messages in message_capture_entries), 1 a packet capture (its packets in a capture file
   on the dashboard's disk, named by the id). capture_target is what a packet capture records: character, account or
   everything. */
ALTER TABLE message_capture_sessions ADD COLUMN capture_kind TINYINT NOT NULL DEFAULT 0;
ALTER TABLE message_capture_sessions ADD COLUMN capture_target VARCHAR(16) NOT NULL DEFAULT '';
