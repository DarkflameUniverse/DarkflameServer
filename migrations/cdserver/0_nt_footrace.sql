-- changes the NT foot race such that it does not share the

BEGIN TRANSACTION;

UPDATE ComponentsRegistry SET component_id = 1901 WHERE id = 12916;
INSERT INTO ActivityRewards (objectTemplate, ActivityRewardIndex, activityRating, LootMatrixIndex, CurrencyIndex, ChallengeRating, description) VALUES (1901, 166, -1, 598, 1, 4, 'NT Foot Race');

COMMIT;
