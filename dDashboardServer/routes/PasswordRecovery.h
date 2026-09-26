#pragma once

/**
 * Choosing a new password without email, for accounts with two-factor login: the username, a current code from the
 * authenticator app and one of the account's recovery codes (which is used up). Needing both means someone who
 * found the recovery codes alone, or has the phone alone, can't take the account. Works with or without email set
 * up (setting password_reset_recovery_codes, on by default).
 */
void RegisterPasswordRecoveryRoutes();

// Whether password_reset_recovery_codes is on
bool RecoveryResetEnabled();
