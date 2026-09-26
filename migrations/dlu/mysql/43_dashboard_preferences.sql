/* Each account's dashboard view choices (show staff, terrain in the 3D viewer, ...), as a JSON object, so they follow
   the account from browser to browser. Written only by the dashboard; keys are the pages' own. */
CREATE TABLE IF NOT EXISTS dashboard_preferences (
    account_id INT UNSIGNED NOT NULL PRIMARY KEY,
    prefs TEXT NOT NULL
);
