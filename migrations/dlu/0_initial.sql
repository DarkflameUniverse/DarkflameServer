DROP TABLE IF EXISTS accounts;
CREATE TABLE accounts (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(35) NOT NULL UNIQUE,
    password TEXT NOT NULL,
    gm_level INT UNSIGNED NOT NULL DEFAULT 0,
    locked BOOLEAN NOT NULL DEFAULT FALSE,
    banned BOOLEAN NOT NULL DEFAULT FALSE,
    play_key_id INT NOT NULL DEFAULT 0,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP(),
    mute_expire BIGINT UNSIGNED NOT NULL DEFAULT 0
);

DROP TABLE IF EXISTS charinfo;
CREATE TABLE charinfo (
    id BIGINT NOT NULL PRIMARY KEY,
    account_id INT NOT NULL REFERENCES accounts(id),
    name VARCHAR(35) NOT NULL UNIQUE,
    pending_name VARCHAR(35) NOT NULL,
    needs_rename BOOLEAN NOT NULL DEFAULT FALSE,
    prop_clone_id BIGINT UNSIGNED AUTO_INCREMENT UNIQUE,
    last_login BIGINT UNSIGNED NOT NULL DEFAULT 0,
    permission_map BIGINT UNSIGNED NOT NULL DEFAULT 0
);

DROP TABLE IF EXISTS charxml;
CREATE TABLE charxml (
    id BIGINT NOT NULL PRIMARY KEY REFERENCES charinfo(id),
    xml_data LONGTEXT NOT NULL
);

DROP TABLE IF EXISTS command_log;
CREATE TABLE command_log (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    character_id BIGINT NOT NULL REFERENCES charinfo(id),
    command VARCHAR(256) NOT NULL
);

DROP TABLE IF EXISTS friends;
CREATE TABLE friends (
    player_id BIGINT NOT NULL REFERENCES charinfo(id),
    friend_id BIGINT NOT NULL REFERENCES charinfo(id),
    best_friend BOOLEAN NOT NULL DEFAULT FALSE,

    PRIMARY KEY (player_id, friend_id)
);

DROP TABLE IF EXISTS leaderboard;
CREATE TABLE leaderboard (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    game_id INT UNSIGNED NOT NULL DEFAULT 0,
    last_played TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP(),
    character_id BIGINT NOT NULL REFERENCES charinfo(id),
    time BIGINT UNSIGNED NOT NULL,
    score BIGINT UNSIGNED NOT NULL DEFAULT 0
);

DROP TABLE IF EXISTS mail;
CREATE TABLE mail (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    sender_id INT NOT NULL DEFAULT 0,
    sender_name VARCHAR(35) NOT NULL DEFAULT '',
    receiver_id BIGINT NOT NULL REFERENCES charinfo(id),
    receiver_name VARCHAR(35) NOT NULL,
    time_sent BIGINT UNSIGNED NOT NULL,
    subject TEXT NOT NULL,
    body TEXT NOT NULL,
    attachment_id BIGINT NOT NULL DEFAULT 0,
    attachment_lot INT NOT NULL DEFAULT 0,
    attachment_subkey BIGINT NOT NULL DEFAULT 0,
    attachment_count INT NOT NULL DEFAULT 0,
    was_read BOOLEAN NOT NULL DEFAULT FALSE
);

DROP TABLE IF EXISTS object_id_tracker;
CREATE TABLE object_id_tracker (
    last_object_id BIGINT UNSIGNED NOT NULL DEFAULT 0 PRIMARY KEY
);

DROP TABLE IF EXISTS pet_names;
CREATE TABLE pet_names (
    id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    pet_name TEXT NOT NULL,
    approved INT UNSIGNED NOT NULL
);

DROP TABLE IF EXISTS play_keys;
CREATE TABLE play_keys (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    key_string CHAR(19) NOT NULL UNIQUE,
    key_uses INT NOT NULL DEFAULT 1,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP(),
    active BOOLEAN NOT NULL DEFAULT TRUE
);

DROP TABLE IF EXISTS properties;
CREATE TABLE properties (
    id BIGINT NOT NULL PRIMARY KEY,
    owner_id BIGINT NOT NULL REFERENCES charinfo(id),
    template_id INT UNSIGNED NOT NULL,
    clone_id BIGINT UNSIGNED REFERENCES charinfo(prop_clone_id),
    name TEXT NOT NULL,
    description TEXT NOT NULL,
    rent_amount INT NOT NULL,
    rent_due BIGINT NOT NULL,
    privacy_option INT NOT NULL,
    mod_approved BOOLEAN NOT NULL DEFAULT FALSE,
    last_updated BIGINT NOT NULL,
    time_claimed BIGINT NOT NULL,
    rejection_reason TEXT NOT NULL,
    reputation BIGINT UNSIGNED NOT NULL,
    zone_id INT NOT NULL
);

DROP TABLE IF EXISTS ugc;
CREATE TABLE ugc (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    account_id INT NOT NULL REFERENCES accounts(id),
    character_id BIGINT NOT NULL REFERENCES charinfo(id),
    is_optimized BOOLEAN NOT NULL DEFAULT FALSE,
    lxfml MEDIUMBLOB NOT NULL,
    bake_ao BOOLEAN NOT NULL DEFAULT FALSE,
    filename TEXT NOT NULL DEFAULT ('')
);

DROP TABLE IF EXISTS properties_contents;
CREATE TABLE properties_contents (
    id BIGINT NOT NULL PRIMARY KEY,
    property_id BIGINT NOT NULL REFERENCES properties(id),
    ugc_id INT NULL REFERENCES ugc(id),
    lot INT NOT NULL,
    x FLOAT NOT NULL,
    y FLOAT NOT NULL,
    z FLOAT NOT NULL,
    rx FLOAT NOT NULL,
    ry FLOAT NOT NULL,
    rz FLOAT NOT NULL,
    rw FLOAT NOT NULL
);

DROP TABLE IF EXISTS activity_log;
CREATE TABLE activity_log (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    character_id BIGINT NOT NULL REFERENCES charinfo(id),
    activity INT NOT NULL,
    time BIGINT UNSIGNED NOT NULL,
    map_id INT NOT NULL
);

DROP TABLE IF EXISTS bug_reports;
CREATE TABLE bug_reports (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    body TEXT NOT NULL,
    client_version TEXT NOT NULL,
    other_player_id TEXT NOT NULL,
    selection TEXT NOT NULL,
    submitted TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP()
);

DROP TABLE IF EXISTS servers;
CREATE TABLE servers (
    id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name TEXT NOT NULL,
    ip TEXT NOT NULL,
    port INT NOT NULL,
    state INT NOT NULL,
    version INT NOT NULL DEFAULT 0
);
