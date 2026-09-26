/* Each account's dashboard view choices. See the MySQL migration. */
CREATE TABLE IF NOT EXISTS dashboard_preferences (
    account_id INTEGER NOT NULL PRIMARY KEY,
    prefs TEXT NOT NULL
);
